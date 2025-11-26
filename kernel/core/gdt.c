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
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// ────────────────
// GDT Table
// ────────────────
static struct gdt_entry gdt_entries[5];
static struct gdt_ptr   gdt_pointer;

// ────────────────
// External Invocation
// ────────────────
extern void gdt_flush(void* gdt_ptr_addr);

// ────────────────
// GDT Initialization Ritual
// ────────────────
void gdt_init(void) {
    gfx_print("Invoking GDT setup...\n");

    gdt_pointer.limit = sizeof(gdt_entries) - 1;
    gdt_pointer.base  = (uint64_t)&gdt_entries;   // 64-bit base

    // Null descriptor (entry 0)
    gdt_entries[0] = (struct gdt_entry){
        .limit_low   = 0,
        .base_low    = 0,
        .base_middle = 0,
        .access      = 0,
        .granularity = 0,
        .base_high   = 0,
    };

    // Kernel code segment (entry 1): Access=0x9A, Flags=0xA0 (L=1, D=0)
    // In 64-bit mode, base and limit are ignored for code/data segments
    gdt_entries[1] = (struct gdt_entry){
        .limit_low   = 0,
        .base_low    = 0,
        .base_middle = 0,
        .access      = 0x9A,      // Present, Ring 0, Code, Readable
        .granularity = 0xA0,      // Long mode (L=1), not 32-bit (D=0)
        .base_high   = 0,
    };

    // Kernel data segment (entry 2): Access=0x92, Flags=0x00
    gdt_entries[2] = (struct gdt_entry){
        .limit_low   = 0,
        .base_low    = 0,
        .base_middle = 0,
        .access      = 0x92,      // Present, Ring 0, Data, Writable
        .granularity = 0x00,      // No special flags needed for data
        .base_high   = 0,
    };

    // User code segment (entry 3): Access=0xFA, Flags=0xA0
    gdt_entries[3] = (struct gdt_entry){
        .limit_low   = 0,
        .base_low    = 0,
        .base_middle = 0,
        .access      = 0xFA,      // Present, Ring 3, Code, Readable
        .granularity = 0xA0,      // Long mode (L=1)
        .base_high   = 0,
    };

    // User data segment (entry 4): Access=0xF2, Flags=0x00
    gdt_entries[4] = (struct gdt_entry){
        .limit_low   = 0,
        .base_low    = 0,
        .base_middle = 0,
        .access      = 0xF2,      // Present, Ring 3, Data, Writable
        .granularity = 0x00,
        .base_high   = 0,
    };

    gdt_flush(&gdt_pointer); // Load 64-bit GDTR

    gfx_print("GDT initialized successfully.\n");
}
