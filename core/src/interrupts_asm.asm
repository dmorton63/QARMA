[BITS 64]
section .text
extern interrupt_handler
extern cpu_exception_handler
extern timer_handler_wrapper
extern interrupt_handler_wrapper
extern cpu_exception_handler_wrapper

; ────────────────
; ISR Stubs (Exceptions 0–31)
; ────────────────
; Exceptions WITHOUT error code: 0,1,2,3,4,5,6,7,9,15,16,17,19,20,21,22-31
; Exceptions WITH error code: 8,10,11,12,13,14,17,30
%macro ISR_STUB_NO_ERRCODE 1
global isr%1
isr%1:
    push qword 0         ; Fake error code for consistency
    push qword %1        ; Interrupt number
    jmp isr_exception_stub
%endmacro

%macro ISR_STUB_ERRCODE 1
global isr%1
isr%1:
    ; Error code already pushed by CPU
    push qword %1        ; Interrupt number
    jmp isr_exception_stub
%endmacro

; Generate ISR stubs for all 32 CPU exceptions
ISR_STUB_NO_ERRCODE 0   ; Divide by Zero
ISR_STUB_NO_ERRCODE 1   ; Debug
ISR_STUB_NO_ERRCODE 2   ; NMI
ISR_STUB_NO_ERRCODE 3   ; Breakpoint
ISR_STUB_NO_ERRCODE 4   ; Overflow
ISR_STUB_NO_ERRCODE 5   ; Bound Range
ISR_STUB_NO_ERRCODE 6   ; Invalid Opcode
ISR_STUB_NO_ERRCODE 7   ; Device Not Available
ISR_STUB_ERRCODE    8   ; Double Fault (has error code)
ISR_STUB_NO_ERRCODE 9   ; Coprocessor Segment Overrun
ISR_STUB_ERRCODE    10  ; Invalid TSS (has error code)
ISR_STUB_ERRCODE    11  ; Segment Not Present (has error code)
ISR_STUB_ERRCODE    12  ; Stack Fault (has error code)
ISR_STUB_ERRCODE    13  ; General Protection Fault (has error code)
ISR_STUB_ERRCODE    14  ; Page Fault (has error code)
ISR_STUB_NO_ERRCODE 15  ; Reserved
ISR_STUB_NO_ERRCODE 16  ; x87 FPU Error
ISR_STUB_ERRCODE    17  ; Alignment Check (has error code)
ISR_STUB_NO_ERRCODE 18  ; Machine Check
ISR_STUB_NO_ERRCODE 19  ; SIMD Floating Point
ISR_STUB_NO_ERRCODE 20  ; Virtualization
ISR_STUB_NO_ERRCODE 21  ; Control Protection
ISR_STUB_NO_ERRCODE 22  ; Reserved
ISR_STUB_NO_ERRCODE 23  ; Reserved
ISR_STUB_NO_ERRCODE 24  ; Reserved
ISR_STUB_NO_ERRCODE 25  ; Reserved
ISR_STUB_NO_ERRCODE 26  ; Reserved
ISR_STUB_NO_ERRCODE 27  ; Reserved
ISR_STUB_NO_ERRCODE 28  ; Hypervisor Injection
ISR_STUB_NO_ERRCODE 29  ; VMM Communication
ISR_STUB_ERRCODE    30  ; Security Exception (has error code)
ISR_STUB_NO_ERRCODE 31  ; Reserved

; ────────────────
; IRQ Stubs (IRQs 0–15 → Vectors 32–47)
; ────────────────
%macro IRQ_STUB 1
%if %1 = 32
extern timer_handler
global irq32
irq32:
    push rbp
    mov rbp, rsp
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    xor rdi, rdi          ; Pass NULL as regs* argument
    call timer_handler_wrapper
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp
    iretq
%elif %1 = 33
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
; Exception Common Stub (for CPU exceptions 0-31)
; ────────────────
; Stack layout when we arrive here (from top):
;   [rsp+0]  = exception number
;   [rsp+8]  = error code (real or fake 0)
;   [rsp+16] = RIP (pushed by CPU)
;   [rsp+24] = CS  (pushed by CPU)
;   [rsp+32] = RFLAGS (pushed by CPU)
;   [rsp+40] = RSP (pushed by CPU)
;   [rsp+48] = SS  (pushed by CPU)
;
; CPU leaves RSP misaligned by 8. After pushing 2 qwords (exception# + error),
; RSP is still misaligned by 8 (total 16 bytes pushed after 5 CPU qwords).
isr_exception_stub:
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

    ; Extract exception information from interrupt frame
    ; 15 regs(120) + exc#(8) + error(8) = +136 offset
    mov rdi, [rsp + 120]      ; exception number (1st arg) 
    mov rsi, [rsp + 128]      ; error code (2nd arg)
    mov rdx, [rsp + 136]      ; RIP (3rd arg)
    mov rcx, [rsp + 144]      ; CS (4th arg)
    mov r8,  [rsp + 152]      ; RFLAGS (5th arg)

    call cpu_exception_handler_wrapper   ; Never returns (calls kernel_panic)

    ; If we somehow return (shouldn't happen):
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
    add rsp, 8                ; Remove alignment padding
    add rsp, 16               ; Clean up error code and int number
    iretq

; ────────────────
; ISR Common Stub (for IRQs and other interrupts)
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

    ; 15 registers (120) + int# (8) + error (8) = +136
    mov rdi, [rsp + 120]  ; interrupt number (1st arg)
    mov rsi, [rsp + 128]  ; error code (2nd arg)

    call interrupt_handler_wrapper
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