#pragma once
#include "kernel_types.h"

#define MAX_INTERRUPTS 255
#define IDT_ENTRIES 255
typedef struct {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;


idt_entry_t idt[IDT_ENTRIES];

typedef void (*isr_t)(regs_t*);

void register_interrupt_handler(uint8_t int_no, isr_t handler);

// Initialize and remap the legacy PICs. Implemented in assembly (pic.asm).
void init_pic(void);
void divide_by_zero_handler();
void timer_handler(struct regs* r);
void send_eoi(uint8_t int_no);
idt_ptr_t idt_ptr;