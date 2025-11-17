/**
 * QARMA - GDT Setup
 * Ritual initialization of the Global Descriptor Table for protected mode
 */

#include "kernel_types.h"
#include "core/kernel.h"
#include "graphics/graphics.h"

// ────────────────
// GDT Structures
// ────────────────
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} PACKED;

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} PACKED;

// ────────────────
// GDT Table
// ────────────────
static struct gdt_entry gdt_entries[5];
static struct gdt_ptr   gdt_pointer;

// ────────────────
// External Invocation
// ────────────────
extern void gdt_flush(uint32_t gdt_ptr_addr);

// ────────────────
// Gate Setter
// ────────────────
static void gdt_set_gate(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    gdt_entries[index].base_low     = base & 0xFFFF;
    gdt_entries[index].base_middle  = (base >> 16) & 0xFF;
    gdt_entries[index].base_high    = (base >> 24) & 0xFF;

    gdt_entries[index].limit_low    = limit & 0xFFFF;
    gdt_entries[index].granularity  = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    gdt_entries[index].access       = access;
}

// ────────────────
// GDT Initialization Ritual
// ────────────────
void gdt_init(void) {
    gfx_print("Invoking GDT setup...\n");

    gdt_pointer.limit = sizeof(gdt_entries) - 1;
    gdt_pointer.base  = (uint32_t)&gdt_entries;

    // Phase 1: Null descriptor (required by legacy)
    gdt_set_gate(0, 0, 0, 0, 0);

    // Phase 2: Kernel segments
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel code: ring 0, executable
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel data: ring 0, writable

    // Phase 3: User segments (reserved for future rituals)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User code: ring 3, executable
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User data: ring 3, writable

    // Final invocation: load the GDT
    gdt_flush((uint32_t)&gdt_pointer);

    gfx_print("GDT initialized successfully.\n");
}