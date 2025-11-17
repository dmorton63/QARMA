#include "kernel_types.h"
#include "core/kernel.h"
#include "graphics/graphics.h"

// ────────────────
// IDT Structures
// ────────────────
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} PACKED;

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} PACKED;

// ────────────────
// IDT Table
// ────────────────
static struct idt_entry idt_entries[256];
static struct idt_ptr   idt_pointer;

// ────────────────
// External Symbols
// ────────────────
extern void idt_flush(uint32_t idt_ptr_addr);
extern void* isr_stubs[32];
extern void* irq_stubs[16];
//extern void* irq_stubs[44];
extern void irqdefault(void);

// ────────────────
// IDT Gate Setter
// ────────────────
static void idt_set_gate(uint8_t vector, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt_entries[vector].base_low  = handler & 0xFFFF;
    idt_entries[vector].base_high = (handler >> 16) & 0xFFFF;
    idt_entries[vector].selector  = selector;
    idt_entries[vector].zero      = 0;
    idt_entries[vector].flags     = flags | 0x80; // Present, ring 0, interrupt gate
}

// ────────────────
// IDT Initialization Ritual
// ────────────────
void idt_init(void) {
    for (int i = 32; i <= 33; i++) {
        uint32_t addr = (idt_entries[i].base_high << 16) | idt_entries[i].base_low;
        gfx_print("IDT[");
        gfx_print_decimal(i);
        gfx_print("] → ");
        gfx_print_hex(addr);
        gfx_print("\n");
    }
    gfx_print("irq_stubs[1] = ");
    gfx_print_hex((uint32_t)irq_stubs[1]);
    gfx_print("\n");
    idt_pointer.limit = sizeof(idt_entries) - 1;
    idt_pointer.base  = (uint32_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entries));

    // Phase 1: Bind CPU exceptions (vectors 0–31)
    for (int i = 0; i < 32; i++) {
        // if(i == 12) 
        // {
        //     idt_set_gate(44, (uint32_t)irq_stubs[i], 0x08, 0x8E);
        // }
        // else{
        idt_set_gate(i, (uint32_t)isr_stubs[i], 0x08, 0x8E);
    }

    // Phase 2: Bind IRQs (vectors 32–47)
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, (uint32_t)irq_stubs[i], 0x08, 0x8E);
    }
    
    // Phase 3: Bind all remaining vectors (48–255) to irqdefault
    for (int i = 48; i < 256; i++) {
        idt_set_gate(i, (uint32_t)irqdefault, 0x08, 0x8E);
    }

    // Final invocation: load the IDT
    idt_flush((uint32_t)&idt_pointer);
}