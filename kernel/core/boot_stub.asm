; QARMA OS 64-bit Kernel Boot Stub
; Multiboot2-compliant entry point with long mode transition
; Boot sequence: 32-bit protected → PAE → Long mode → 64-bit kernel

BITS 32

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
    ; Save multiboot info (EAX = magic, EBX = info pointer)
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx
    
    ; Set up boot stack
    mov esp, boot_stack + 32768
    
    ; Clear interrupts
    cli
    
    ; Check if CPU supports long mode
    call check_long_mode
    test eax, eax
    jz .no_long_mode
    
    ; Build initial page tables
    call setup_page_tables
    
    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)                 ; CR4.PAE = 1
    mov cr4, eax
    
    ; Load PML4
    mov eax, boot_pml4
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
    lgdt [gdt64_descriptor]
    
    ; Far jump to 64-bit code segment
    jmp 0x08:long_mode_start

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
setup_page_tables:
    ; Clear page tables
    mov edi, boot_pml4
    mov ecx, 3072                    ; 3 × 4096 / 4 = 3072 dwords
    xor eax, eax
    rep stosd
    
    ; PML4[0] → PDPT (identity map first 2MB)
    mov eax, boot_pdpt
    or eax, 0x03                     ; Present + Writable
    mov [boot_pml4], eax
    
    ; PML4[511] → PDPT (higher-half kernel)
    mov eax, boot_pdpt
    or eax, 0x03
    mov [boot_pml4 + 511*8], eax
    
    ; PDPT[0] → PD (identity map)
    mov eax, boot_pd
    or eax, 0x03
    mov [boot_pdpt], eax
    
    ; PDPT[510] → PD (higher-half at -2GB)
    mov eax, boot_pd
    or eax, 0x03
    mov [boot_pdpt + 510*8], eax
    
    ; Map first 2MB using 2MB pages in PD
    mov edi, boot_pd
    mov eax, 0x83                    ; Present + Writable + Page Size (2MB)
    mov ecx, 512                     ; 512 entries
.map_pd:
    mov [edi], eax
    add eax, 0x200000                ; Next 2MB
    add edi, 8
    loop .map_pd
    
    ret

; ============================================================================
; 64-bit Code
; ============================================================================
BITS 64
section .text
long_mode_start:
    ; Set up segment registers for 64-bit mode
    mov ax, 0x10                     ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up 64-bit stack
    mov rsp, boot_stack + 32768
    
    ; Clear the frame pointer
    xor rbp, rbp
    
    ; Validate multiboot magic
    mov eax, [multiboot_magic]
    cmp eax, 0x36D76289              ; Multiboot2 magic
    jne .bad_multiboot
    
    ; Call kernel main (System V AMD64 ABI: args in RDI, RSI)
    mov edi, [multiboot_magic]       ; First argument
    mov rsi, qword [multiboot_info]  ; Second argument (need full 64-bit)
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
    dq gdt64                         ; Base

; ============================================================================
; Data Storage
; ============================================================================
section .data
align 4
multiboot_magic: dd 0
multiboot_info:  dq 0                ; 64-bit pointer