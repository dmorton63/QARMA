; QuantumOS - GDT Assembly Functions
; Assembly functions for loading GDT and setting up segments

[BITS 64]

global gdt_flush

; gdt_flush - Load new GDT and update segment registers
; Parameter: RDI = address of GDT pointer structure (System V AMD64 calling convention)
gdt_flush:
    lgdt [rdi]          ; Load new GDT
    
    ; Update segment registers
    mov ax, 0x10        ; Kernel data segment selector (GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far return to update CS register with kernel code segment
    pop rdi             ; Save return address
    mov rax, 0x08       ; Kernel code segment selector
    push rax
    push rdi
    retfq               ; Far return to update CS