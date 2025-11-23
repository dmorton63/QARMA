; task_switch.asm - Low-level context switching for x86_64
; This file provides assembly functions for saving and restoring CPU context
; during task switches in the Quantum OS kernel.

[BITS 64]

SECTION .text

; External symbols
extern task_mgr_current_task

; Exported symbols
global task_switch_context_asm
global task_save_context
global task_restore_context

; CPU context structure offsets (must match cpu_context_t in task_manager.h)
; 64-bit registers are 8 bytes each
%define CTX_RAX     0
%define CTX_RBX     8  
%define CTX_RCX     16
%define CTX_RDX     24
%define CTX_RSI     32
%define CTX_RDI     40
%define CTX_RBP     48
%define CTX_RSP     56
%define CTX_RIP     64
%define CTX_RFLAGS  72
%define CTX_R8      80
%define CTX_R9      88
%define CTX_R10     96
%define CTX_R11     104
%define CTX_R12     112
%define CTX_R13     120
%define CTX_R14     128
%define CTX_R15     136
%define CTX_CS      144
%define CTX_DS      146
%define CTX_ES      148
%define CTX_FS      150
%define CTX_GS      152
%define CTX_SS      154

; Task structure offsets (must match task_t in task_manager.h)
; task_id(4) + name(32) + state(4) + priority(4) + flags(4) = 48 bytes
%define TASK_CONTEXT_OFFSET 48  ; Offset of context field in task_t

;
; task_switch_context_asm(task_t *from_task, task_t *to_task)
; Perform complete context switch between tasks
;
; Parameters (System V AMD64):
;   RDI = from_task (can be NULL for first task)
;   RSI = to_task
;
task_switch_context_asm:
    ; Disable interrupts during context switch
    cli
    
    ; Get parameters (already in RDI, RSI)
    mov rax, rdi            ; from_task
    mov rbx, rsi            ; to_task
    
    ; Test if from_task is NULL (first task switch)
    test rax, rax
    jz .load_new_task
    
    ; Save current context to from_task
    add rax, TASK_CONTEXT_OFFSET    ; Point to context field
    
    ; Save general registers
    mov [rax + CTX_RAX], rax
    mov [rax + CTX_RBX], rbx
    mov [rax + CTX_RCX], rcx
    mov [rax + CTX_RDX], rdx
    mov [rax + CTX_RSI], rsi
    mov [rax + CTX_RDI], rdi
    mov [rax + CTX_RBP], rbp
    mov [rax + CTX_RSP], rsp
    mov [rax + CTX_R8], r8
    mov [rax + CTX_R9], r9
    mov [rax + CTX_R10], r10
    mov [rax + CTX_R11], r11
    mov [rax + CTX_R12], r12
    mov [rax + CTX_R13], r13
    mov [rax + CTX_R14], r14
    mov [rax + CTX_R15], r15
    
    ; Save instruction pointer (return address)
    mov rcx, [rsp]
    mov [rax + CTX_RIP], rcx
    
    ; Save flags
    pushfq
    pop qword [rax + CTX_RFLAGS]
    
    ; Save segment registers
    mov [rax + CTX_CS], cs
    mov [rax + CTX_DS], ds
    mov [rax + CTX_ES], es
    mov [rax + CTX_FS], fs
    mov [rax + CTX_GS], gs
    mov [rax + CTX_SS], ss
    
.load_new_task:
    ; Load context from to_task
    add rbx, TASK_CONTEXT_OFFSET    ; Point to context field
    
    ; Load segment registers first
    mov ax, [rbx + CTX_DS]
    mov ds, ax
    mov ax, [rbx + CTX_ES]
    mov es, ax
    mov ax, [rbx + CTX_FS]
    mov fs, ax
    mov ax, [rbx + CTX_GS]
    mov gs, ax
    mov ax, [rbx + CTX_SS]
    mov ss, ax
    
    ; Load stack pointer
    mov rsp, [rbx + CTX_RSP]
    
    ; Load flags
    push qword [rbx + CTX_RFLAGS]
    popfq
    
    ; Load general registers
    mov rax, [rbx + CTX_RAX]
    mov rcx, [rbx + CTX_RCX]
    mov rdx, [rbx + CTX_RDX]
    mov rsi, [rbx + CTX_RSI]
    mov rdi, [rbx + CTX_RDI]
    mov rbp, [rbx + CTX_RBP]
    mov r8, [rbx + CTX_R8]
    mov r9, [rbx + CTX_R9]
    mov r10, [rbx + CTX_R10]
    mov r11, [rbx + CTX_R11]
    mov r12, [rbx + CTX_R12]
    mov r13, [rbx + CTX_R13]
    mov r14, [rbx + CTX_R14]
    mov r15, [rbx + CTX_R15]
    
    ; Load RBX last since we're using it
    push qword [rbx + CTX_RIP]      ; Push return address
    mov rbx, [rbx + CTX_RBX]
    
    ; Enable interrupts and jump to new task
    sti
    ret         ; Jump to RIP that we pushed

;
; task_save_context(cpu_context_t *context)
; Save current CPU state to context structure
;
; Parameters (System V AMD64):
;   RDI = context pointer
;
task_save_context:
    ; Save general registers
    mov [rdi + CTX_RAX], rax
    mov [rdi + CTX_RBX], rbx
    mov [rdi + CTX_RCX], rcx
    mov [rdi + CTX_RDX], rdx
    mov [rdi + CTX_RSI], rsi
    mov [rdi + CTX_RDI], rdi
    mov [rdi + CTX_RBP], rbp
    mov [rdi + CTX_RSP], rsp
    mov [rdi + CTX_R8], r8
    mov [rdi + CTX_R9], r9
    mov [rdi + CTX_R10], r10
    mov [rdi + CTX_R11], r11
    mov [rdi + CTX_R12], r12
    mov [rdi + CTX_R13], r13
    mov [rdi + CTX_R14], r14
    mov [rdi + CTX_R15], r15
    
    ; Save return address as RIP
    mov rbx, [rsp]
    mov [rdi + CTX_RIP], rbx
    
    ; Save flags
    pushfq
    pop qword [rdi + CTX_RFLAGS]
    
    ; Save segment registers
    mov [rdi + CTX_CS], cs
    mov [rdi + CTX_DS], ds
    mov [rdi + CTX_ES], es
    mov [rdi + CTX_FS], fs
    mov [rdi + CTX_GS], gs
    mov [rdi + CTX_SS], ss
    
    ret

;
; task_restore_context(cpu_context_t *context)
; Restore CPU state from context structure
; Note: This function does not return to caller!
;
; Parameters (System V AMD64):
;   RDI = context pointer
;
task_restore_context:
    ; Load segment registers
    mov bx, [rdi + CTX_DS]
    mov ds, bx
    mov bx, [rdi + CTX_ES]
    mov es, bx
    mov bx, [rdi + CTX_FS]
    mov fs, bx
    mov bx, [rdi + CTX_GS]
    mov gs, bx
    mov bx, [rdi + CTX_SS]
    mov ss, bx
    
    ; Load stack pointer
    mov rsp, [rdi + CTX_RSP]
    
    ; Load flags
    push qword [rdi + CTX_RFLAGS]
    popfq
    
    ; Push return address for final jump
    push qword [rdi + CTX_RIP]
    
    ; Load general registers (RAX/RDI last since we're using them)
    mov rbx, [rdi + CTX_RBX]
    mov rcx, [rdi + CTX_RCX]
    mov rdx, [rdi + CTX_RDX]
    mov rsi, [rdi + CTX_RSI]
    mov rbp, [rdi + CTX_RBP]
    mov r8, [rdi + CTX_R8]
    mov r9, [rdi + CTX_R9]
    mov r10, [rdi + CTX_R10]
    mov r11, [rdi + CTX_R11]
    mov r12, [rdi + CTX_R12]
    mov r13, [rdi + CTX_R13]
    mov r14, [rdi + CTX_R14]
    mov r15, [rdi + CTX_R15]
    mov rax, [rdi + CTX_RAX]
    mov rdi, [rdi + CTX_RDI]
    
    ; Jump to restored context
    ret

SECTION .data
    ; Reserved for future use

SECTION .bss
    ; Reserved for future use