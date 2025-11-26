; QARMA OS 64-bit Kernel Boot Stub
; Multiboot2-compliant entry point with long mode transition
; Boot sequence: 32-bit protected → PAE → Long mode → 64-bit kernel

BITS 32

; Memory layout constants
KERNEL_VMA equ 0xFFFFFFFF80000000
KERNEL_LMA equ 0x200000  ; 2 MiB physical load address

; Macro to convert virtual address to physical
; The linker script sets `. = VMA_BASE + LMA_BASE`, so symbols have VMA addresses
; that already include the LMA offset. To get physical address, just subtract VMA_BASE.
; Physical address = Virtual address - KERNEL_VMA
%define PHYS(addr) ((addr) - KERNEL_VMA)

; ============================================================================
; Multiboot2 Header
; ============================================================================
section .multiboot
align 8
multiboot2_header_start:
    dd 0xE85250D6                    ; Multiboot2 magic
    dd 0                             ; Architecture: i386 protected mode
    dd multiboot2_header_end - multiboot2_header_start  ; Header length
    dd -(0xE85250D6 + 0 + (multiboot2_header_end - multiboot2_header_start))  ; Checksum

    ; Entry address tag (tell GRUB where to jump)
    align 8
    dw 3                             ; Type: entry address
    dw 0                             ; Flags
    dd 12                            ; Size
    dd PHYS(_start)                  ; Entry point (computed from _start symbol)

    ; Framebuffer tag
    align 8
    dw 5                             ; Type: framebuffer
    dw 0                             ; Flags
    dd 20                            ; Size
    dd 1024                          ; Width
    dd 768                           ; Height
    dd 32                            ; Depth

    ; End tag
    align 8
    dw 0                             ; Type: end
    dw 0                             ; Flags
    dd 8                             ; Size
multiboot2_header_end:

; ============================================================================
; Boot Page Tables (Identity Map + Higher Half)
; ============================================================================
section .bss
align 4096
boot_pml4:
    resb 4096                        ; PML4 (512 entries × 8 bytes)
boot_pdpt:
    resb 4096                        ; PDPT (512 entries × 8 bytes)
boot_pd:
    resb 4096                        ; PD for 0-1GB
boot_pd1:
    resb 4096                        ; PD for 1-2GB
boot_pd2:
    resb 4096                        ; PD for 2-3GB
boot_pd3:
    resb 4096                        ; PD for 3-4GB
boot_pd_hh:
    resb 4096                        ; PD for higher-half mapping
boot_pd_fb:
    resb 4096                        ; PD for 7GB framebuffer mapping
boot_stack:
    resb 32768                       ; 32KB boot stack
boot_cpu_lock:
    resd 1                           ; SMP lock: only one CPU proceeds

; Basic IDT for fault detection
align 16
idt64:
    resb 256*16                      ; 256 IDT entries, 16 bytes each
idt64_desc:
    dw (256*16 - 1)                  ; Limit
    dq 0                             ; Base (will be set at runtime)

; ============================================================================
; 32-bit Entry Point
; ============================================================================
section .text
global _start
extern quantum_kernel_main
extern kernel_idle_loop

_start:
    ; CRITICAL: Save multiboot values BEFORE anything else!
    ; GRUB passes magic in EAX, info pointer in EBX
    mov [PHYS(multiboot_magic)], eax
    mov [PHYS(multiboot_info)], ebx
    
    ; SMP guard: only let one CPU through (atomic test-and-set)
    mov eax, 1
    xchg eax, [PHYS(boot_cpu_lock)]
    test eax, eax
    jnz .ap_cpu_halt              ; If lock was already set, this is an AP CPU
    jmp .bsp_start                ; Bootstrap CPU continues
    
.ap_cpu_halt:
    ; Application Processor (not BSP) - halt
    cli
    hlt
    jmp .ap_cpu_halt

.bsp_start:
    ; Write OK pattern to VGA - if this appears, GRUB loaded us successfully
    mov word [0xB8000], 0x2F4F  ; Green 'O'
    mov word [0xB8002], 0x2F4B  ; Green 'K'
    mov word [0xB8004], 0x2F20  ; Green ' '
    
    ; Debug: Print saved multiboot magic
    mov eax, [PHYS(multiboot_magic)]
    mov dx, 0x3F8
    mov al, 'A'
    out dx, al
    mov eax, [PHYS(multiboot_magic)]
    out dx, al                       ; Print low byte
    
    ; Set up boot stack
    mov esp, PHYS(boot_stack) + 32768
    
    ; Clear interrupts
    cli
    
    ; Check if CPU supports long mode
    call check_long_mode
    test eax, eax
    jz .no_long_mode
    
    ; Debug: write 'C' after long mode check
    mov dx, 0x3F8
    mov al, 'C'
    out dx, al
    
    ; Build initial page tables
    call setup_page_tables
    
    ; Debug: write 'D' after page tables
    mov dx, 0x3F8
    mov al, 'D'
    out dx, al
    
    ; Debug: Write to VGA to confirm we got here
    mov word [0xB8006], 0x2F44  ; Green 'D'
    
    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)                 ; CR4.PAE = 1
    mov cr4, eax
    
    ; Load PML4
    mov eax, PHYS(boot_pml4)
    mov cr3, eax
    
    ; Enable long mode (EFER.LME = 1)
    mov ecx, 0xC0000080              ; EFER MSR
    rdmsr
    or eax, (1 << 8)                 ; LME bit
    wrmsr
    
    ; Load 64-bit GDT before enabling paging
    lgdt [PHYS(gdt64_descriptor)]
    
    ; Enable paging (CR0.PG = 1) - this activates long mode
    mov eax, cr0
    or eax, (1 << 31)                ; PG bit
    mov cr0, eax
    
    ; Debug: about to enter long mode
    mov dx, 0x3F8
    mov al, 'J'
    out dx, al
    
    ; Far jump to 64-bit code segment using indirect far jump
    ; Cannot encode 64-bit virtual address directly in 32-bit mode
    jmp 0x08:PHYS(.enter_64bit)

BITS 64
.enter_64bit:
    ; Now in 64-bit mode, jump to higher-half virtual address
    mov rax, long_mode_start
    jmp rax

BITS 32
.no_long_mode:
    ; Hang if CPU doesn't support long mode
    cli
    hlt
    jmp .no_long_mode

; ============================================================================
; Check Long Mode Support
; ============================================================================
check_long_mode:
    ; Check if CPUID is supported
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)               ; Flip CPUID bit
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid                      ; CPUID not supported
    
    ; Check if extended CPUID is available
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_cpuid
    
    ; Check if long mode is supported
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)              ; LM bit
    jz .no_cpuid
    
    mov eax, 1
    ret

.no_cpuid:
    xor eax, eax
    ret

; ============================================================================
; Setup Initial Page Tables
; ============================================================================
section .text
setup_page_tables:
    ; Save stack pointer
    push esp
    
    ; Debug: entered setup_page_tables
    mov dx, 0x3F8
    mov al, 'P'
    out dx, al
    
    ; Clear PML4, PDPT, PD, PD_HH (4 tables × 4096 bytes = 16384 bytes)
    mov edi, PHYS(boot_pml4)
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    mov edi, PHYS(boot_pdpt)
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    mov edi, PHYS(boot_pd)
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    mov edi, PHYS(boot_pd_hh)
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    ; Identity: PML4[0] -> PDPT
    mov eax, PHYS(boot_pdpt)
    or  eax, 0x03
    mov [PHYS(boot_pml4) + 0*8], eax

    ; Wire PDPT entries for 0-4GB identity mapping
    ; PDPT[0] -> PD (0-1GB)
    mov eax, PHYS(boot_pd)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 0*8], eax
    
    ; PDPT[1] -> PD1 (1-2GB)
    mov eax, PHYS(boot_pd1)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 1*8], eax
    
    ; PDPT[2] -> PD2 (2-3GB)
    mov eax, PHYS(boot_pd2)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 2*8], eax
    
    ; PDPT[3] -> PD3 (3-4GB)
    mov eax, PHYS(boot_pd3)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 3*8], eax

    ; Higher-half: PML4[511] -> PDPT, PDPT[510] -> boot_pd_hh
    ; Kernel at 0xFFFFFFFF80000000 uses PML4[511], PDPT[510]
    mov eax, PHYS(boot_pdpt)
    or  eax, 0x03
    mov [PHYS(boot_pml4) + 511*8], eax

    mov eax, PHYS(boot_pd_hh)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 510*8], eax    ; Changed from 511 to 510!

    ; Identity map 0-4GB using 2 MiB pages (4 page directories)
    ; PD0: 0-1GB
    mov edi, PHYS(boot_pd)
    mov eax, 0x83              ; P | RW | PS (2MB pages)
    mov ecx, 512
.id_map_pd0:
    mov [edi], eax
    add eax, 0x200000          ; Next 2MB page
    add edi, 8
    loop .id_map_pd0
    
    ; PD1: 1-2GB
    mov edi, PHYS(boot_pd1)
    mov ecx, 512
.id_map_pd1:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .id_map_pd1
    
    ; PD2: 2-3GB
    mov edi, PHYS(boot_pd2)
    mov ecx, 512
.id_map_pd2:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .id_map_pd2
    
    ; PD3: 3-4GB (includes MMIO at 0xFEBB0000)
    mov edi, PHYS(boot_pd3)
    mov ecx, 512
.id_map_pd3:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .id_map_pd3

    ; Higher-half PD: Map virtual 0xFFFFFFFF80000000+ to physical 0x000000+
    ; PD[0] -> phys 0x000000-0x1FFFFF (virtual 0xFFFFFFFF80000000-0xFFFFFFFF801FFFFF)
    ; PD[1] -> phys 0x200000-0x3FFFFF (virtual 0xFFFFFFFF80200000-0xFFFFFFFF803FFFFF) <- kernel is here
    ; PD[2] -> phys 0x400000-0x5FFFFF (virtual 0xFFFFFFFF80400000-0xFFFFFFFF805FFFFF)
    ; etc. Map 96 MiB total to cover kernel + BSS + stack (kernel needs ~35 MiB)
    mov edi, PHYS(boot_pd_hh)
    mov eax, 0x000083               ; Physical 0x000000, P|RW|PS
    mov ecx, 48                      ; 48 entries = 96 MiB
.hh_map:
    mov [edi], eax
    add eax, 0x200000               ; Next 2 MiB page
    add edi, 8                       ; Next PD entry
    loop .hh_map

    ; Debug: PD_HH setup complete
    mov dx, 0x3F8
    mov al, 'M'
    out dx, al

    ; Map 7GB region for framebuffer (identity map)
    ; Framebuffer at ~0x1C0000000A (7 GiB) needs PDPT[7] -> PD_FB
    ; Clear PD_FB table
    mov edi, PHYS(boot_pd_fb)
    mov ecx, 4096/4
    xor eax, eax
    rep stosd
    
    ; Wire PDPT[112] -> PD_FB (7GB = 0x1C0000000, PDPT index = 112)
    ; PDPT index = (0x1C0000000 >> 30) & 0x1FF = 112
    mov eax, PHYS(boot_pd_fb)
    or  eax, 0x03                    ; Present | RW
    mov [PHYS(boot_pdpt) + 112*8], eax
    
    ; Fill PD_FB with 2 MiB pages covering 7-8 GiB
    ; Physical base = 0x1C0000000 (7 GiB aligned to 2 MiB)
    ; In 64-bit page tables, each entry is 8 bytes (low 32 + high 32)
    ; 7GB = 0x00000001_C0000000 (high=1, low=0xC0000000)
    mov edi, PHYS(boot_pd_fb)
    mov eax, 0xC0000083              ; Low: 0xC0000000 | flags (Present | RW | PS)
    mov edx, 0x00000001              ; High: 0x00000001
    mov ecx, 512                     ; 512 entries = 1 GiB coverage
.fb_map:
    mov [edi], eax                   ; Write low 32 bits
    mov [edi + 4], edx               ; Write high 32 bits  
    add eax, 0x200000                ; Add 2 MiB to low dword
    adc edx, 0                       ; Add carry to high dword
    add edi, 8                       ; Next PD entry (8 bytes)
    loop .fb_map
    
    ; Debug: FB mapping complete
    mov dx, 0x3F8
    mov al, 'F'
    out dx, al

    ; Restore stack pointer
    pop esp
    ret
; ============================================================================
; 64-bit Code
; ============================================================================
BITS 64
section .text
long_mode_start:
    ; Debug marker: entered long mode
    ; mov dx, 0x3F8
    ; mov al, 'E'
    ; out dx, al

    ; Load data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack (use higher-half virtual address)
    mov rsp, boot_stack + 32768

    ; Clear frame pointer
    xor rbp, rbp

    ; Initialize FPU/SSE (required for C code with floats)
    ; Enable FPU (clear CR0.EM, set CR0.MP)
    mov rax, cr0
    and ax, 0xFFFB      ; Clear EM (bit 2)
    or ax, 0x0002       ; Set MP (bit 1)
    mov cr0, rax
    
    ; Enable SSE (set CR4.OSFXSR and CR4.OSXMMEXCPT)
    mov rax, cr4
    or ax, 0x0600       ; Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov cr4, rax
    
    ; Initialize FPU state
    fninit

    ; Debug marker
    mov dx, 0x3F8
    mov al, 'F'
    out dx, al

    ; Install basic IDT with minimal exception handlers
    ; Set IDT base address in the descriptor (use RIP-relative)
    lea rax, [rel idt64]
    mov [rel idt64_desc + 2], rax  ; Set IDT base (8-byte address at offset 2)
    
    ; Install minimal handlers for critical exceptions
    ; IDT Entry format: offset_low(16) | selector(16) | ist(8) | flags(8) | offset_mid(16) | offset_high(32) | reserved(32)
    ; Flags: Present(1) | DPL(2) | 0 | Type(4) = 0x8E for interrupt gate
    
    ; Exception 8: Double Fault (#DF)
    lea rdi, [idt64 + 8*16]           ; IDT[8] - use virtual address
    lea rax, [exception_handler]      ; handler virtual address
    mov [rdi], ax                     ; offset_low
    shr rax, 16
    mov [rdi + 6], ax                 ; offset_mid  
    shr rax, 16
    mov [rdi + 8], eax                ; offset_high
    mov word [rdi + 2], 0x08          ; code selector
    mov byte [rdi + 5], 0x8E          ; present, interrupt gate, DPL=0
    
    ; Exception 13: General Protection (#GP)
    lea rdi, [idt64 + 13*16]          ; IDT[13] - use virtual address
    lea rax, [exception_handler]      ; handler virtual address
    mov [rdi], ax
    shr rax, 16
    mov [rdi + 6], ax
    shr rax, 16
    mov [rdi + 8], eax
    mov word [rdi + 2], 0x08
    mov byte [rdi + 5], 0x8E
    
    ; Exception 14: Page Fault (#PF)
    lea rdi, [idt64 + 14*16]          ; IDT[14] - use virtual address
    lea rax, [exception_handler]      ; handler virtual address
    mov [rdi], ax
    shr rax, 16
    mov [rdi + 6], ax
    shr rax, 16
    mov [rdi + 8], eax
    mov word [rdi + 2], 0x08
    mov byte [rdi + 5], 0x8E
    
    ; Load the IDT (use RIP-relative addressing)
    lidt [rel idt64_desc]

    ; Debug: Verify PML4[0] is set for identity mapping  
    ; Print all 8 bytes of PML4[0]
    mov rbx, PHYS(boot_pml4)
    mov rcx, [rbx]
    mov dx, 0x3F8
    mov al, cl
    out dx, al
    shr rcx, 8
    mov al, cl
    out dx, al
    shr rcx, 8
    mov al, cl
    out dx, al
    shr rcx, 8
    mov al, cl
    out dx, al                    ; First 4 bytes (should be 03 10 25 00)
    
    ; Reload GDT with virtual base address before jumping to higher-half
    lgdt [PHYS(gdt64_descriptor_virtual)]
    
    ; Jump to higher-half kernel
    mov rax, higher_half_entry
    jmp rax

section .text
higher_half_entry:
    ; Debug marker

    mov dx, 0x3F8
    mov al, 'G'
    out dx, al
    
    ; Debug: confirm we're in higher half
    mov dx, 0x3F8
    mov al, 'H'
    out dx, al

    ; Switch to higher-half stack
    mov rsp, boot_stack + 32768
    
    ; Debug marker
    mov dx, 0x3F8
    mov al, 'I'
    out dx, al
    
    ; Now running in higher-half, can use virtual addresses    
    ; Clear the frame pointer
    xor rbp, rbp
    
    ; Debug marker before reading multiboot
    mov dx, 0x3F8
    mov al, 'J'
    out dx, al
    
    ; Load multiboot info using RIP-relative addressing
    mov edi, [rel multiboot_magic]   ; First argument
    
    ; Debug marker - got multiboot_magic
    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    
    mov esi, [rel multiboot_info]    ; Second argument (32-bit from bootloader)
    
    ; Debug marker - got multiboot_info
    mov dx, 0x3F8
    mov al, 'L'
    out dx, al
    
    ; Debug marker - got multiboot_info
    mov dx, 0x3F8
    mov al, 'M'
    out dx, al
    
    ; Debug: Print EDI (multiboot magic) - low 4 bytes
    mov dx, 0x3F8
    mov eax, edi
    out dx, al
    shr eax, 8
    out dx, al
    shr eax, 8
    out dx, al
    shr eax, 8
    out dx, al
    
    ; Validate multiboot magic
    cmp edi, 0x36D76289              ; Multiboot2 magic
    
    ; Debug: After cmp
    mov dx, 0x3F8
    mov al, 'N'
    out dx, al
    
    jne .bad_multiboot
    
    ; Debug: magic validated
    mov dx, 0x3F8
    mov al, 'O'
    out dx, al
    
    ; Ensure 16-byte stack alignment BEFORE call
    ; System V AMD64 ABI requires RSP to be 16-byte aligned before CALL
    ; The CALL itself pushes return address (8 bytes), so we need RSP+8 aligned to 16
    and rsp, ~0xF             ; Align to 16 bytes
    sub rsp, 8                ; Adjust so after call pushes 8 bytes, we're aligned
    
    ; Debug: validate call target address
    ; mov rax, quantum_kernel_main
    ; mov dx, 0x3F8
    ; mov al, 'K'               ; Marker: loaded target address
    ; out dx, al
    mov rax, higher_half_entry
    mov dx, 0x3F8
    mov al, 'I'
    out dx, al
    mov al, al      ; AL already holds the low 8 bits of RAX
    out dx, al      ; crude check

    ; Debug: About to call kernel main
    mov dx, 0x3F8
    mov al, 'Q'
    out dx, al

    ; Call kernel main (System V AMD64 ABI: args in RDI, RSI)
    call quantum_kernel_main
    
    ; Debug: Returned from kernel main
    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    
    ; Instead of halting, tail-jump into kernel idle loop
    ; This is a proper tail call that never returns
    jmp kernel_idle_loop

.bad_multiboot:
    ; Invalid multiboot magic
.halt:
    cli
    hlt
    jmp .halt

; ============================================================================
; Minimal Exception Handler (prevents triple fault)
; ============================================================================
exception_handler:
    ; Print 'X' to indicate exception occurred
    mov dx, 0x3F8
    mov al, 'X'
    out dx, al
    
    ; Read CR2 (page fault address) and print low byte
    mov rax, cr2
    mov dx, 0x3F8
    mov al, al
    out dx, al
    
    ; Halt forever
    cli
.loop:
    hlt
    jmp .loop

; ============================================================================
; 64-bit GDT
; ============================================================================
section .data
align 8
gdt64:
    ; Null descriptor
    dq 0x0000000000000000
    
    ; 64-bit code segment (0x08)
    ; Base=0, Limit=0 (ignored), Access=0x9A, Flags=0xA0 (L=1, D=0)
    dq 0x00AF9A000000FFFF
    
    ; 64-bit data segment (0x10)
    ; Base=0, Limit=0 (ignored), Access=0x92, Flags=0x00
    dq 0x00CF92000000FFFF

gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64 - 1         ; Limit
    dq PHYS(gdt64)                   ; Base (physical for initial load)

gdt64_descriptor_virtual:
    dw gdt64_end - gdt64 - 1         ; Limit  
    dq gdt64                         ; Base (virtual, no PHYS macro)

; ============================================================================
; Data Storage
; ============================================================================
section .data
align 4
multiboot_magic: dd 0
multiboot_info:  dq 0                ; 64-bit pointer