#include "kernel_types.h"
#include "core/kernel.h"
#include "graphics/graphics.h"

// ────────────────
// IDT Structures (64-bit)
// ────────────────
struct idt_entry {
    uint16_t offset_low;   // Offset bits 0-15
    uint16_t selector;     // Code segment selector
    uint8_t  ist;          // Interrupt Stack Table (0 = don't switch)
    uint8_t  flags;        // Type and attributes
    uint16_t offset_mid;   // Offset bits 16-31
    uint32_t offset_high;  // Offset bits 32-63
    uint32_t reserved;     // Reserved (must be zero)
} PACKED;

struct idt_ptr {
    uint16_t limit;
    uint64_t base;         // 64-bit base address
} PACKED;

// ────────────────
// IDT Table
// ────────────────
static struct idt_entry idt_entries[256];
static struct idt_ptr   idt_pointer;

// ────────────────
// External Symbols
// ────────────────
extern void idt_flush(uint64_t idt_ptr_addr);
extern void* isr_stubs[32];
extern void* irq_stubs[16];
//extern void* irq_stubs[44];
extern void irqdefault(void);

// ────────────────
// IDT Gate Setter (64-bit)
// ────────────────
static void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt_entries[vector].offset_low  = handler & 0xFFFF;
    idt_entries[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    idt_entries[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt_entries[vector].selector    = selector;
    idt_entries[vector].ist         = 0;  // Don't use IST
    idt_entries[vector].flags       = flags;
    idt_entries[vector].reserved    = 0;
}

// ────────────────
// IDT Initialization Ritual
// ────────────────
void idt_init(void) {
    idt_pointer.limit = sizeof(idt_entries) - 1;
    idt_pointer.base  = (uint64_t)&idt_entries;  // 64-bit base

    memset(&idt_entries, 0, sizeof(idt_entries));

    // Phase 1: Bind CPU exceptions (vectors 0–31)
    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_stubs[i], 0x08, 0x8E);
    }

    // Phase 2: Bind IRQs (vectors 32–47)
    for (int i = 0; i < 16; i++) {
        idt_set_gate(32 + i, (uint64_t)irq_stubs[i], 0x08, 0x8E);
    }
    
    // Phase 3: Bind all remaining vectors (48–255) to irqdefault
    for (int i = 48; i < 256; i++) {
        idt_set_gate(i, (uint64_t)irqdefault, 0x08, 0x8E);
    }

    // Final invocation: load the IDT
    idt_flush((uint64_t)&idt_pointer);
    
    gfx_print("IDT: 64-bit entries loaded\n");
}