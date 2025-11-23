[BITS 64]
section .text
extern interrupt_handler

; ────────────────
; ISR Stubs (Exceptions 0–31)
; ────────────────
%macro ISR_STUB_NO_ERRCODE 1
global isr%1
isr%1:
    push qword 0         ; Fake error code
    push qword %1        ; Interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_STUB_ERRCODE 1
global isr%1
isr%1:
    push qword %1        ; Interrupt number
    jmp isr_common_stub
%endmacro

%assign i 0
%rep 32
    ISR_STUB_NO_ERRCODE i
%assign i i+1
%endrep

; ────────────────
; IRQ Stubs (IRQs 0–15 → Vectors 32–47)
; ────────────────
%macro IRQ_STUB 1
%if %1 = 33
extern irqkeyboard
global irq33
irq33:
    jmp irqkeyboard
%elif %1 = 44
extern irqmouse
global irq44
irq44:
    jmp irqmouse
%else
    global irq%1
irq%1:
    push qword 0         ; Error code
    push qword %1        ; Interrupt number
    jmp irq_common_stub
%endif
%endmacro

%assign i 32
%rep 16
    IRQ_STUB i
%assign i i+1
%endrep

; ────────────────
; Dedicated IRQ1 Stub (Keyboard)
; ────────────────

extern timer_handler
global irq0_handler
irq0_handler:
    push qword 0          ; dummy error code
    push qword 32         ; vector number
    call timer_handler
    add rsp, 16
    iretq

; ────────────────
; Default IRQ Stub (Fallback)
; ────────────────
global irqdefault
irqdefault:
    push qword 0
    push qword 255       ; Unknown IRQ
    jmp irq_common_stub

; ────────────────
; ISR Common Stub
; ────────────────
isr_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 128]  ; interrupt number (1st arg)
    mov rsi, [rsp + 136]  ; error code (2nd arg)

    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16           ; Remove error code and int number
    iretq

; ────────────────
; IRQ Common Stub
; ────────────────
irq_common_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 128]  ; interrupt number (1st arg)
    mov rsi, [rsp + 136]  ; error code (2nd arg)

    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16           ; Remove error code and int number
    iretq

; ────────────────
; IDT Flush
; ────────────────
global idt_flush
idt_flush:
    lidt [rdi]           ; RDI = first argument (System V AMD64)
    ret
section .data
global irq_stubs
irq_stubs:
    dq irq32, irq33, irq34, irq35
    dq irq36, irq37, irq38, irq39
    dq irq40, irq41, irq42, irq43
    dq irq44, irq45, irq46, irq47

section .data
global isr_stubs
isr_stubs:
    dq isr0, isr1, isr2, isr3
    dq isr4, isr5, isr6, isr7
    dq isr8, isr9, isr10, isr11
    dq isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19
    dq isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27
    dq isr28, isr29, isr30, isr31