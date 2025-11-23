# 64-bit Migration Audit Report
**Date:** November 23, 2025  
**Branch:** 64bit-migration  
**Tagged:** v32bit-stable

## Executive Summary
QARMA is currently a **32-bit protected mode** kernel with **clean, well-structured code**. Migration to 64-bit is feasible and necessary for quantum computing, AI workloads, and utilizing 128GB host RAM.

---

## Current Architecture Analysis

### Assembly Files (8 files - ALL need porting)
```
BITS 32 files requiring update:
1. kernel/core/boot_stub.asm         - Boot entry, GDT, multiboot
2. kernel/core/interrupts_asm.asm    - ISR/IRQ handlers
3. kernel/core/pic.asm               - PIC programming
4. kernel/core/gdt_flush.asm         - GDT reload
5. kernel/core/input/mouse_asm.asm   - PS/2 mouse handler
6. kernel/keyboard/keyboard_asm.asm  - Keyboard interrupt handler
7. kernel/core/scheduler/task_switch.asm - Context switching
8. boot/boot.asm                     - Bootloader (if used)
```

**Impact:** HIGH - All assembly must be rewritten for 64-bit registers and calling convention

---

## Memory Management (Critical Components)

### VMM (Virtual Memory Manager)
**File:** `kernel/core/memory/vmm/vmm.c` (552 lines)

**Current Structure (32-bit, 2-level paging):**
- Page Directory: 1024 entries × 32-bit = 4KB
- Page Tables: 1024 entries × 32-bit = 4KB each
- Identity maps first 32MB (8 PDEs)
- Virtual allocations start at 0xC0000000 (3GB)

**64-bit Changes Required:**
```c
// Current (32-bit):
static uint32_t page_directory[1024];          // 10-bit index
static uint32_t identity_page_tables[8][1024]; // 10-bit index

// Future (64-bit):
static uint64_t pml4[512];                     // 9-bit index
static uint64_t pdpt[512][512];                // 9-bit index
static uint64_t pd[512][512];                  // 9-bit index
static uint64_t pt[512][512];                  // 9-bit index
```

**Key Functions to Port:**
- `vmm_map_page()` - Change to 4-level traversal
- `vmm_get_physical_address()` - Update address translation
- `enable_paging()` - Load PML4 into CR3, set long mode
- `vmm_get_or_create_page_table()` - Handle 4 levels

**Complexity:** VERY HIGH (core kernel functionality)

---

### PMM (Physical Memory Manager)
**File:** `kernel/core/memory/pmm/pmm.c` (164 lines)

**Current Limitations:**
```c
#define MAX_PHYSICAL_PAGES 32768  // 128MB max!
static uint32_t pmm_next = 1;     // 32-bit addresses
uint32_t pmm_alloc_page(void);    // Returns 32-bit
```

**64-bit Changes Required:**
```c
#define MAX_PHYSICAL_PAGES (128ULL * 1024 * 1024 / 4)  // 128GB / 4KB
static uint64_t pmm_next = 1;     // 64-bit addresses
uint64_t pmm_alloc_page(void);    // Returns 64-bit phys addr
```

**Impact:** MEDIUM - Simple type changes, bitmap stays 32-bit indexed

---

### Heap Allocator
**File:** `kernel/core/memory/heap.c`

**Issues Found:**
```c
SERIAL_LOG_HEX("", (uint32_t)phys);  // Truncates 64-bit addresses!
SERIAL_LOG_HEX("", (uint32_t)virt);  // Will break in 64-bit
```

**64-bit Changes Required:**
- Update all pointer types to `uintptr_t`
- Fix format specifiers: `%x` → `%llx` for 64-bit
- Update DMA allocation to use 64-bit addresses

**Complexity:** MEDIUM

---

## Data Type Audit

### Critical Type Changes
Found **100+ instances** of 32-bit address types that need updating:

**vmm.c:**
```c
// Before:
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
uint32_t vmm_get_physical_address(uint32_t vaddr);
void vmm_free_region(uint32_t virtual_addr, uint32_t size);

// After:
void vmm_map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
uint64_t vmm_get_physical_address(uint64_t vaddr);
void vmm_free_region(uint64_t virtual_addr, uint64_t size);
```

**USB Drivers (xhci.c, uhci.c):**
```c
// Before:
static inline uint32_t vaddr_to_phys(void *vaddr);
uint32_t phys = vmm_get_physical_address(addr);

// After:
static inline uint64_t vaddr_to_phys(void *vaddr);
uint64_t phys = vmm_get_physical_address(addr);
```

**Good News:** XHCI already uses `uint64_t` for DMA addresses! ✓

---

## Hardware/Driver Compatibility

### USB Stack
**Status:** MOSTLY COMPATIBLE
- XHCI uses 64-bit addresses (TRBs, rings) ✓
- UHCI uses 32-bit addresses (legacy, may need updates)
- virt_to_phys64() already exists for XHCI

### Multiboot
**Current:** Multiboot1 (0x1BADB002)
**Required:** Multiboot2 (better 64-bit support)

**Changes:**
- Update boot_stub.asm with Multiboot2 header
- Update multiboot.c to parse Multiboot2 tags
- Handle 64-bit framebuffer addresses

---

## Toolchain Requirements

### Current Toolchain
```makefile
CC       = i686-elf-gcc
CFLAGS   = -m32 -nostdlib -nostdinc
LDFLAGS  = -m elf_i386
ASFLAGS  = -f elf32
```

### Required Toolchain
```makefile
CC       = x86_64-elf-gcc
CFLAGS   = -m64 -mcmodel=kernel -mno-red-zone
LDFLAGS  = -m elf_x86_64
ASFLAGS  = -f elf64
```

**Installation:**
```bash
# Check if already installed
which x86_64-elf-gcc

# If not, build from source or install via package manager
sudo apt install gcc-x86-64-linux-gnu  # Ubuntu/Debian
# OR build cross-compiler from source
```

---

## Linker Script

### Current (32-bit)
```ld
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
. = 0x00100000;  /* 1MB mark */
```

### Required (64-bit)
```ld
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)
. = 0xFFFFFFFF80000000;  /* Higher-half kernel */
```

---

## 64-bit Memory Layout Design

### Proposed Virtual Address Space
```
0x0000000000000000 - 0x00007FFFFFFFFFFF  User Space (128TB)
├─ 0x0000000000400000: User code start
├─ 0x0000100000000000: User heap
└─ 0x00007F0000000000: User stack top

[Canonical hole: 0x0000800000000000 - 0xFFFF7FFFFFFFFFFF]

0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF  Kernel Space (128TB)
├─ 0xFFFF800000000000: VMM allocator region (start)
├─ 0xFFFFFFDF80000000: Linear physical map (128GB)
│   └─ Maps physical 0x00000000 - 0x1FFFFFFFFF linearly
├─ 0xFFFFFFFF80000000: Kernel code/data (.text, .rodata, .data, .bss)
├─ 0xFFFFFFFFC0000000: Kernel heap
└─ 0xFFFFFFFFFFFFFFFF: Top of address space
```

**Benefits:**
- Full 128GB physical memory access via linear mapping
- Large kernel heap space for quantum/AI workloads
- Security: KASLR possible with huge address space

---

## Reference Code (CPP_TestCode)

**Available 64-bit implementations:**
- `CPP_TestCode/kernel/src/memory/paging.cpp` - 4-level page tables ✓
- `CPP_TestCode/kernel/include/memory/paging.h` - PTE/PDE structures ✓
- 64-bit page table traversal algorithms ✓
- Large page (2MB) support ✓

**Can be ported to pure C** (no C++ required)

---

## Risk Assessment

### High Risk Areas
1. **Boot sequence** - Transitioning to long mode is complex
2. **VMM rewrite** - Core kernel functionality, hard to test incrementally
3. **Interrupt handling** - Different stack frame layout in 64-bit
4. **Regression potential** - Hard to maintain 32-bit compatibility

### Medium Risk Areas
1. **USB drivers** - Need address translation updates
2. **DMA allocations** - Must work with 64-bit addresses
3. **Multiboot2 migration** - Different tag format

### Low Risk Areas
1. **PMM** - Simple type changes
2. **Heap** - Mostly pointer arithmetic updates
3. **Graphics** - Already uses physical addresses

---

## Migration Strategy

### Phase 1: Foundation (Week 1-2)
1. ✓ Create branch & backup (DONE)
2. Install x86_64-elf-gcc toolchain
3. Create 64-bit linker script
4. Write boot_stub64.asm (long mode transition)
5. Create 64-bit GDT/IDT structures

### Phase 2: Core Memory (Week 2-3)
6. Implement 4-level page tables in vmm.c
7. Update PMM for 64-bit addresses
8. Port interrupt handlers to 64-bit
9. Update task switching for 64-bit

### Phase 3: Drivers & Testing (Week 3-4)
10. Update all assembly files (8 files)
11. Fix C data types throughout codebase
12. Update USB drivers for 64-bit
13. Migrate to Multiboot2
14. Test boot in QEMU with 4GB, 8GB, 16GB

### Phase 4: Validation (Week 4+)
15. Memory stress tests
16. DMA allocation verification
17. Native boot on 128GB hardware
18. Documentation update

---

## Rollback Plan

**32-bit stable version:**
```bash
git checkout main
git reset --hard v32bit-stable
make clean && make
```

**32-bit remains available for:**
- Legacy hardware support
- Testing/comparison
- Incremental migration validation

---

## Recommendations

### Proceed with Migration? **YES**

**Reasons:**
1. Quantum computing requires >4GB memory
2. AI workloads need large address space
3. 128GB host RAM currently wasted
4. Clean codebase makes migration feasible
5. CPP_TestCode provides working reference

### Timeline: **3-4 weeks** (full-time development)

### Next Steps:
1. ✓ Branch created and tagged
2. Install x86_64-elf-gcc toolchain
3. Begin with boot_stub64.asm
4. Port VMM to 4-level paging

---

## File Inventory

### Assembly Files Requiring Porting (8)
- boot_stub.asm (127 lines) - CRITICAL
- interrupts_asm.asm - HIGH
- task_switch.asm - HIGH
- gdt_flush.asm - MEDIUM
- pic.asm - MEDIUM
- keyboard_asm.asm - LOW
- mouse_asm.asm - LOW
- boot/boot.asm - LOW (may be unused)

### C Files Requiring Major Changes
- kernel/core/memory/vmm/vmm.c (552 lines) - CRITICAL
- kernel/core/memory/pmm/pmm.c (164 lines) - HIGH
- kernel/core/multiboot.c - HIGH
- kernel/core/interrupts.c - HIGH
- kernel/core/idt.c - HIGH

### C Files Requiring Type Updates (50+)
All files with `uint32_t` addresses (100+ instances)

---

## Conclusion

QARMA is **well-positioned for 64-bit migration**. The codebase is clean, modular, and well-documented. The primary challenge is the VMM rewrite, but with CPP_TestCode as reference, this is manageable.

**Migration Status:** Ready to begin  
**Confidence Level:** HIGH  
**Estimated Effort:** 3-4 weeks full-time

---

**Next:** Step 3 - Set up 64-bit toolchain
