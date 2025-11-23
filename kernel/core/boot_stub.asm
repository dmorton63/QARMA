; QARMA OS 64-bit Kernel Boot Stub
; Multiboot2-compliant entry point with long mode transition
; Boot sequence: 32-bit protected → PAE → Long mode → 64-bit kernel

BITS 32

; Memory layout constants
KERNEL_VMA equ 0xFFFFFFFF80000000
KERNEL_LMA equ 0x100000

; Macro to convert virtual address to physical
%define PHYS(addr) ((addr) - KERNEL_VMA + KERNEL_LMA)

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
    dd 0x101040                      ; Entry point (physical address of _start)

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
    resb 4096                        ; PD (512 entries × 8 bytes)
boot_stack:
    resb 32768                       ; 32KB boot stack

; ============================================================================
; 32-bit Entry Point
; ============================================================================
section .text
global _start
extern quantum_kernel_main

_start:
    ; Write OK pattern to VGA - if this appears, GRUB loaded us successfully
    mov word [0xB8000], 0x2F4F  ; Green 'O'
    mov word [0xB8002], 0x2F4B  ; Green 'K'
    mov word [0xB8004], 0x2F20  ; Green ' '
    
    ; Save multiboot info (EAX = magic, EBX = info pointer)
    mov [PHYS(multiboot_magic)], eax
    mov [PHYS(multiboot_info)], ebx
    
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
    
    ; Enable paging (CR0.PG = 1) - this activates long mode
    mov eax, cr0
    or eax, (1 << 31)                ; PG bit
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [PHYS(gdt64_descriptor)]
    
    ; Far jump to 64-bit code segment (use physical address)
    jmp 0x08:PHYS(long_mode_start)

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
section .bss
align 4096
boot_pd_hh:
    resb 4096                        ; PD for higher-half mapping
setup_page_tables:
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

    ; Identity: PML4[0] -> PDPT, PDPT[0] -> PD
    mov eax, PHYS(boot_pdpt)
    or  eax, 0x03
    mov [PHYS(boot_pml4) + 0*8], eax

    mov eax, PHYS(boot_pd)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 0*8], eax

    ; Higher-half: PML4[511] -> PDPT, PDPT[511] -> boot_pd_hh
    mov eax, PHYS(boot_pdpt)
    or  eax, 0x03
    mov [PHYS(boot_pml4) + 511*8], eax

    mov eax, PHYS(boot_pd_hh)
    or  eax, 0x03
    mov [PHYS(boot_pdpt) + 511*8], eax

    ; Identity PD: map 0..1 GiB using 2 MiB pages
    mov edi, PHYS(boot_pd)
    mov eax, 0x83              ; P | RW | PS
    mov ecx, 512
.id_map:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .id_map

    ; Higher-half PD: map kernel physical (KERNEL_LMA) upward
    mov edi, PHYS(boot_pd_hh)
    mov eax, (KERNEL_LMA & ~0x1FFFFF) | 0x83
    ; Map the first N*2 MiB of the kernel (start with 16 MiB => 8 entries)
    mov ecx, 8
.hh_map:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .hh_map

    ret
; ============================================================================
; 64-bit Code
; ============================================================================
BITS 64
section .text
long_mode_start:
    ; Debug marker: entered long mode
    mov dx, 0x3F8
    mov al, 'E'
    out dx, al

    ; Load data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; *** Set up stack here ***
    mov rsp, PHYS(boot_stack) + 32768   ; use identity-mapped physical stack first

    ; Clear frame pointer
    xor rbp, rbp

    ; Debug marker
    mov dx, 0x3F8
    mov al, 'F'
    out dx, al

    ; Now safe to jump to higher-half
    mov rax, .higher_half
    jmp rax
    
.higher_half:
    ; Debug marker
    mov dx, 0x3F8
    mov al, 'G'
    out dx, al

    ; Switch to higher-half stack (only if mapped!)
    mov rsp, boot_stack + 32768
    
    ; Now running in higher-half, can use virtual addresses    
    ; Clear the frame pointer
    xor rbp, rbp
    
    ; Load multiboot info from physical addresses (they're identity-mapped)
    mov rax, PHYS(multiboot_magic)
    mov edi, [rax]                   ; First argument
    
    mov rax, PHYS(multiboot_info)
    mov esi, [rax]                   ; Second argument (32-bit from bootloader)
    
    ; Validate multiboot magic
    cmp edi, 0x36D76289              ; Multiboot2 magic
    jne .bad_multiboot
    
    ; Call kernel main (System V AMD64 ABI: args in RDI, RSI)
    call quantum_kernel_main
    
    ; If kernel returns, halt
    jmp .halt

.bad_multiboot:
    ; Invalid multiboot magic
.halt:
    cli
    hlt
    jmp .halt

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
    dq PHYS(gdt64)                   ; Base (physical address)

; ============================================================================
; Data Storage
; ============================================================================
section .data
align 4
multiboot_magic: dd 0
multiboot_info:  dq 0                ; 64-bit pointer