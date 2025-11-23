/**
 * @file vmm.c
 * @brief Virtual Memory Manager for QARMA OS
 * 
 * Provides paging, virtual memory allocation, and address translation.
 * Uses identity mapping for the first 32MB (kernel + early data structures).
 * Virtual allocations start at 3GB (0xC0000000).
 */

#include "vmm.h"
#include "config.h"
#include "../memory.h"
#include "../pmm/pmm.h"

// ============================================================================
// Constants and Configuration
// ============================================================================

#define PAGE_SIZE           0x1000              // 4KB pages
#define IDENTITY_MAP_SIZE   0x02000000          // 32MB identity-mapped
#define IDENTITY_PDE_COUNT  8                   // 8 PDEs × 4MB = 32MB
#define EARLY_PAGETABLE_POOL 16                 // Pre-allocated page tables
#define VIRT_ALLOC_START    0xC0000000          // 3GB virtual address start

// ============================================================================
// Static Data Structures (Identity-Mapped)
// ============================================================================

// Page directory (must be 4KB-aligned)
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

// Pre-allocated page tables for identity mapping
static uint32_t identity_page_tables[IDENTITY_PDE_COUNT][1024] __attribute__((aligned(4096)));

// Pool of early page tables (before PMM is fully operational)
static uint32_t early_page_tables[EARLY_PAGETABLE_POOL][1024] __attribute__((aligned(4096)));

// Static page table for framebuffer mapping
static uint32_t framebuffer_page_table[1024] __attribute__((aligned(4096)));

// ============================================================================
// Module State
// ============================================================================

static bool paging_enabled = false;
static bool vmm_initialized = false;
static uint32_t vmm_next_virtual_addr = VIRT_ALLOC_START;
static uint32_t early_pt_count = 0;
static bool early_pt_exhausted = false;

// ============================================================================
// Private Helper Functions
// ============================================================================

/**
 * @brief Get physical address of VMM's own statically-allocated structures
 * @param ptr Pointer to VMM structure (must be in identity-mapped region)
 * @return Physical address (same as virtual for identity-mapped region)
 * 
 * This is used only for VMM's internal structures like page_directory and
 * identity_page_tables, which are statically allocated in .bss and loaded
 * in the first 32MB of memory.
 */
static inline uint32_t vmm_virt_to_phys(void* ptr) {
    uint32_t addr = (uint32_t)ptr;
    
    // All VMM static structures are in identity-mapped region
    if (addr < IDENTITY_MAP_SIZE) {
        return addr;
    }
    
    // Should never happen for VMM's own structures
    SERIAL_LOG("VMM: WARNING - vmm_virt_to_phys called on non-identity address: ");
    SERIAL_LOG_HEX("", addr);
    SERIAL_LOG("\n");
    return addr;
}

/**
 * @brief Invalidate TLB entry for a virtual address
 */
static inline void tlb_invalidate_page(uint32_t virtual_addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

/**
 * @brief Flush entire TLB by reloading CR3
 */
static inline void tlb_flush_all(void) {
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

/**
 * @brief Get page table for a virtual address, creating if necessary
 * @param page_dir_idx Page directory index
 * @param flags Flags to apply to the page table entry
 * @return Pointer to page table, or NULL on failure
 */
static uint32_t* vmm_get_or_create_page_table(uint32_t page_dir_idx, uint32_t flags) {
    // Check if page table already exists
    if (page_directory[page_dir_idx] & PAGE_PRESENT) {
        return (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
    }
    
    uint32_t* page_table;
    uint32_t page_table_phys;
    
    // Try to use early page table pool first
    if (!early_pt_exhausted && early_pt_count < EARLY_PAGETABLE_POOL) {
        page_table = early_page_tables[early_pt_count++];
        page_table_phys = vmm_virt_to_phys(page_table);
    } else {
        // Allocate from PMM
        page_table_phys = pmm_alloc_page();
        if (page_table_phys == 0) {
            SERIAL_LOG("VMM: ERROR - Failed to allocate page table\n");
            return NULL;
        }
        page_table = (uint32_t*)page_table_phys; // Identity-mapped
        early_pt_exhausted = true;
    }
    
    // Clear the page table
    memset(page_table, 0, PAGE_SIZE);
    
    // Install in page directory
    page_directory[page_dir_idx] = page_table_phys | PAGE_PRESENT | PAGE_WRITE | flags;
    
    return page_table;
}

// ============================================================================
// Public API Implementation
// ============================================================================

/**
 * @brief Initialize Virtual Memory Manager
 * 
 * Sets up identity mapping for first 32MB and prepares page directory.
 * Must be called before any other VMM functions.
 */
void vmm_init(void) {
    if (vmm_initialized) {
        SERIAL_LOG("VMM: Already initialized\n");
        return;
    }

    gfx_print("Initializing Virtual Memory Manager...\n");
    
    // Clear page directory
    memset(page_directory, 0, sizeof(page_directory));

    // Identity-map the first 32MB (kernel, early data, DMA buffers)
    for (int pde = 0; pde < IDENTITY_PDE_COUNT; pde++) {
        // Set up page table entries
        for (int entry = 0; entry < 1024; entry++) {
            uint32_t phys_addr = (pde * 1024u + entry) * PAGE_SIZE;
            identity_page_tables[pde][entry] = phys_addr | PAGE_PRESENT | PAGE_WRITE;
        }
        
        // Install page table in page directory
        uint32_t pt_phys = vmm_virt_to_phys(identity_page_tables[pde]);
        page_directory[pde] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    // Initialize early page table pool
    early_pt_count = 0;
    early_pt_exhausted = false;
    
    vmm_initialized = true;
    
    SERIAL_LOG("VMM: Identity mapping established for 0-32MB\n");
    SERIAL_LOG_HEX("VMM: Page directory at physical: ", vmm_virt_to_phys(page_directory));
    SERIAL_LOG("\n");
    SERIAL_LOG_HEX("VMM: Virtual allocation starts at: ", vmm_next_virtual_addr);
    SERIAL_LOG("\n");
    
    gfx_print("Virtual Memory Manager initialized.\n");
}

/**
 * @brief Check if VMM is initialized
 * @return true if initialized, false otherwise
 */
bool vmm_is_initialized(void) {
    return vmm_initialized;
}

/**
 * @brief Ensure VMM is initialized (initialize if not)
 */
void vmm_ensure_initialized(void) {
    if (!vmm_initialized) {
        vmm_init();
    }
}

/**
 * @brief Enable paging and switch to virtual memory mode
 * @param page_directory_phys_addr Physical address of page directory
 */
void enable_paging(uint64_t page_directory_phys_addr) {
    SERIAL_LOG_HEX("VMM: Enabling paging with PD at: ", page_directory_phys_addr);
    SERIAL_LOG("\n");
    
    __asm__ volatile (
        "mov %0, %%cr3\n"           // Set page directory
        "mov %%cr0, %%rax\n"
        "or $0x80000001, %%eax\n"   // Set PG bit (use eax for 32-bit immediate)
        "mov %%rax, %%cr0\n"
        : : "r" (page_directory_phys_addr) : "rax"
    );
    
    paging_enabled = true;
    SERIAL_LOG("VMM: Paging enabled\n");
}

/**
 * @brief Map a virtual page to a physical page
 * @param virtual_addr Virtual address (will be page-aligned)
 * @param physical_addr Physical address (will be page-aligned)
 * @param flags Page flags (PAGE_PRESENT, PAGE_WRITE, etc.)
 */
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!vmm_initialized) {
        SERIAL_LOG("VMM: ERROR - Cannot map page, VMM not initialized\n");
        return;
    }

    // Calculate indices
    uint32_t page_dir_idx = virtual_addr >> 22;
    uint32_t page_table_idx = (virtual_addr >> 12) & 0x3FF;
    
    // Get or create page table
    uint32_t* page_table = vmm_get_or_create_page_table(page_dir_idx, flags);
    if (!page_table) {
        SERIAL_LOG("VMM: ERROR - Failed to get page table for mapping\n");
        return;
    }

    // Map the page
    page_table[page_table_idx] = (physical_addr & ~0xFFF) | PAGE_PRESENT | flags;
    
    // Invalidate TLB
    tlb_invalidate_page(virtual_addr);
}

/**
 * @brief Unmap a virtual page
 * @param virtual_addr Virtual address to unmap
 */
void vmm_unmap_page(uint32_t virtual_addr) {
    if (!vmm_initialized) {
        return;
    }

    uint32_t page_dir_idx = virtual_addr >> 22;
    uint32_t page_table_idx = (virtual_addr >> 12) & 0x3FF;

    // Check if page table exists
    if (!(page_directory[page_dir_idx] & PAGE_PRESENT)) {
        return;
    }

    // Get page table and clear entry
    uint32_t* page_table = (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
    page_table[page_table_idx] = 0;

    // Invalidate TLB
    tlb_invalidate_page(virtual_addr);
}

/**
 * @brief Get physical address for a virtual address
 * @param vaddr Virtual address
 * @return Physical address, or 0 if not mapped
 */
uint32_t vmm_get_physical_address(uint32_t vaddr) {
    // Fast path for identity-mapped region
    if (vaddr < IDENTITY_MAP_SIZE) {
        return vaddr;
    }

    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    // Check page directory entry
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        return 0;
    }

    // Get page table and check entry
    uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    if (!(page_table[pt_index] & PAGE_PRESENT)) {
        return 0;
    }

    // Return physical address
    return (page_table[pt_index] & ~0xFFF) | (vaddr & 0xFFF);
}

/**
 * @brief Convert virtual pointer to physical address (64-bit for DMA)
 * @param virt Virtual pointer
 * @return 64-bit physical address, or 0 if not mapped
 */
uint64_t virt_to_phys(void* virt) {
    uint32_t vaddr = (uint32_t)virt;
    uint32_t phys = vmm_get_physical_address(vaddr);
    
    if (phys == 0) {
        SERIAL_LOG("VMM: WARNING - virt_to_phys failed for address: ");
        SERIAL_LOG_HEX("", vaddr);
        SERIAL_LOG("\n");
    }
    
    return (uint64_t)phys;
}

/**
 * @brief Allocate contiguous virtual pages
 * @param num_pages Number of pages to allocate
 * @return Virtual address of allocated region, or NULL on failure
 */
void* vmm_alloc_pages(size_t num_pages) {
    if (!vmm_initialized || num_pages == 0) {
        return NULL;
    }

    void* base = (void*)vmm_next_virtual_addr;

    // Allocate and map each page
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t phys = pmm_alloc_page();
        if (!phys) {
            // Allocation failed - free what we allocated and return NULL
            vmm_free_pages(base, i);
            SERIAL_LOG("VMM: ERROR - Failed to allocate physical page\n");
            return NULL;
        }

        vmm_map_page(vmm_next_virtual_addr, phys, PAGE_PRESENT | PAGE_WRITE);
        vmm_next_virtual_addr += PAGE_SIZE;
    }

    return base;
}

/**
 * @brief Free pages allocated with vmm_alloc_pages
 * @param addr Base virtual address
 * @param num_pages Number of pages to free
 */
void vmm_free_pages(void* addr, size_t num_pages) {
    if (!vmm_initialized || !addr || num_pages == 0) {
        return;
    }

    uint32_t base = (uint32_t)addr;
    uint32_t size = num_pages * PAGE_SIZE;
    vmm_free_region(base, size);
}

/**
 * @brief Allocate a virtual memory region
 * @param size Size in bytes (will be rounded up to page boundary)
 * @return Virtual address of allocated region, or 0 on failure
 */
uint32_t vmm_alloc_region(uint32_t size) {
    if (!vmm_initialized || !paging_enabled || size == 0) {
        return 0;
    }
    
    // Round up to page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t num_pages = size / PAGE_SIZE;
    
    // Allocate pages
    void* region = vmm_alloc_pages(num_pages);
    if (!region) {
        SERIAL_LOG("VMM: ERROR - Failed to allocate region\n");
        return 0;
    }
    
    return (uint32_t)region;
}

/**
 * @brief Free a virtual memory region
 * @param virtual_addr Base virtual address
 * @param size Size in bytes (will be rounded up to page boundary)
 */
void vmm_free_region(uint32_t virtual_addr, uint32_t size) {
    if (!vmm_initialized) {
        return;
    }
    
    // Round up to page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t num_pages = size / PAGE_SIZE;
    
    // Free each page
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virtual_page = virtual_addr + (i * PAGE_SIZE);
        uint32_t page_dir_idx = virtual_page >> 22;
        uint32_t page_table_idx = (virtual_page >> 12) & 0x3FF;
        
        // Check if page is mapped
        if (!(page_directory[page_dir_idx] & PAGE_PRESENT)) {
            continue;
        }
        
        uint32_t* page_table = (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
        if (!(page_table[page_table_idx] & PAGE_PRESENT)) {
            continue;
        }
        
        // Free physical page
        uint32_t phys_addr = page_table[page_table_idx] & ~0xFFF;
        pmm_free_page(phys_addr);
        
        // Clear page table entry
        page_table[page_table_idx] = 0;
        tlb_invalidate_page(virtual_page);
    }
}

/**
 * @brief Map framebuffer memory region
 * @param fb_physical_addr Physical address of framebuffer
 * @param fb_size Size of framebuffer in bytes
 * @return true on success, false on failure
 * 
 * Identity-maps the framebuffer region so virtual address = physical address.
 * Typically used for QEMU/VGA framebuffer at 0xFD000000 or similar.
 */
bool vmm_map_framebuffer(uint32_t fb_physical_addr, uint32_t fb_size) {
    if (!vmm_initialized) {
        SERIAL_LOG("VMM: ERROR - Cannot map framebuffer, not initialized\n");
        return false;
    }

    SERIAL_LOG("VMM: Mapping framebuffer - phys=");
    SERIAL_LOG_HEX("", fb_physical_addr);
    SERIAL_LOG(" size=");
    SERIAL_LOG_HEX("", fb_size);
    SERIAL_LOG("\n");
    
    // Low memory is already identity-mapped
    if (fb_physical_addr < 0x100000) {
        SERIAL_LOG("VMM: Framebuffer in low memory, already accessible\n");
        return true;
    }
    
    // Check if paging is already enabled
    if (paging_enabled) {
        SERIAL_LOG("VMM: Paging already enabled\n");
        return true;
    }
    
    // Validate framebuffer address range
    if (fb_physical_addr < 0xE0000000 || fb_physical_addr >= 0xFFE00000) {
        SERIAL_LOG("VMM: WARNING - Framebuffer address outside expected range\n");
    }
    
    // Calculate page coverage
    uint32_t fb_page_count = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Set up page table for framebuffer region
    uint32_t fb_base_page = (fb_physical_addr >> 22) << 10;
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t page_addr = ((fb_base_page + i) << 12);
        framebuffer_page_table[i] = page_addr | PAGE_PRESENT | PAGE_WRITE;
    }
    
    // Identity-map framebuffer pages (optimized bulk mapping)
    uint32_t pages_mapped = 0;
    for (uint32_t page = 0; page < fb_page_count; page++) {
        uint32_t phys_addr = fb_physical_addr + (page * PAGE_SIZE);
        uint32_t virt_addr = phys_addr;  // Identity mapping
        uint32_t page_dir_idx = virt_addr >> 22;
        uint32_t page_table_idx = (virt_addr >> 12) & 0x3FF;
        
        // Get or create page table
        uint32_t* page_table;
        if (!(page_directory[page_dir_idx] & PAGE_PRESENT)) {
            uint32_t fb_pt_phys = vmm_virt_to_phys(framebuffer_page_table);
            page_directory[page_dir_idx] = fb_pt_phys | PAGE_PRESENT | PAGE_WRITE;
            page_table = framebuffer_page_table;
        } else {
            page_table = (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
        }
        
        // Map the page
        page_table[page_table_idx] = phys_addr | PAGE_PRESENT | PAGE_WRITE;
        pages_mapped++;
    }
    
    // Flush TLB for entire range
    tlb_flush_all();
    
    SERIAL_LOG_HEX("VMM: Mapped ", pages_mapped);
    SERIAL_LOG(" framebuffer pages\n");
    
    // Enable paging with framebuffer mapping
    uint32_t page_dir_phys = vmm_virt_to_phys(page_directory);
    enable_paging(page_dir_phys);
    
    SERIAL_LOG("VMM: Paging enabled with framebuffer\n");
    return true;
}

/**
 * @brief Map a Memory-Mapped I/O region (identity-mapped, uncached)
 * @param physical_addr Physical address of MMIO region
 * @param size Size of MMIO region in bytes
 * @return true on success, false on failure
 * 
 * Used for device registers like XHCI controller MMIO space.
 * Creates identity mapping (virtual = physical) with PAGE_NO_CACHE flag.
 */
bool vmm_map_mmio_region(uint32_t physical_addr, uint32_t size) {
    if (!vmm_initialized) {
        SERIAL_LOG("VMM: ERROR - Cannot map MMIO, not initialized\n");
        return false;
    }
    
    // Align to page boundaries
    uint32_t start_page = physical_addr & ~0xFFF;
    uint32_t end_addr = physical_addr + size;
    uint32_t end_page = (end_addr + 0xFFF) & ~0xFFF;
    uint32_t num_pages = (end_page - start_page) / PAGE_SIZE;
    
    SERIAL_LOG("VMM: Mapping MMIO region - phys=");
    SERIAL_LOG_HEX("", physical_addr);
    SERIAL_LOG(" size=");
    SERIAL_LOG_HEX("", size);
    SERIAL_LOG(" pages=");
    SERIAL_LOG_HEX("", num_pages);
    SERIAL_LOG("\n");
    
    // Identity map each page with no-cache flag
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t addr = start_page + (i * PAGE_SIZE);
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);
    }
    
    SERIAL_LOG("VMM: MMIO region mapped\n");
    return true;
}

