/**
 * QARMA - Interrupt System
 * Handles IRQ routing, exception dispatch, and system initialization
 */

#include "interrupts.h"
#include "core/kernel.h"
#include "graphics/graphics.h"
//#include "keyboard/keyboard_types.h"
#include "core/io.h"
#include "config.h"
#include "kernel.h"
#include "kernel_types.h"
#include "keyboard/keyboard_types.h"
#include "core/clock_overlay.h"
#include "core/timer.h"
#include "core/input/mouse.h"
#include "scheduler/task_manager.h"
// ────────────────
// External Symbols
// ────────────────
extern void idt_flush(uint32_t);
extern void irq33();
extern void isr0();
extern void irq44();
extern void irq0_handler();

// ────────────────
// Interrupt Handler Table
// ────────────────
static isr_t interrupt_handlers[MAX_INTERRUPTS];

void register_interrupt_handler(uint8_t int_no, isr_t handler) {
    if (int_no < MAX_INTERRUPTS) {
        interrupt_handlers[int_no] = handler;
    }
}



// ────────────────
// Central Interrupt Dispatcher
// ────────────────
void interrupt_handler(uint32_t int_no, uint32_t err_code) {
    int_no &= 0xFF;

    gfx_print("INT ");
    gfx_print_hex(int_no);
    gfx_print(" ERR ");
    gfx_print_hex(err_code);
    gfx_print("\n");

    switch (int_no) {
        case 0:
            gfx_print("Divide-by-zero fault\n");
            break;

        case 33: { // IRQ1: Keyboard
            regs_t regs = { .int_no = int_no, .err_code = err_code };
            keyboard_handler(&regs, inb(0x60));
            break;
        }

        case 160:
            SERIAL_LOG("Spurious interrupt 160 received and ignored.\n");
            gfx_print("Spurious interrupt 160 received and ignored.\n");
            break;

        case 208:
            outb(0xA0, 0x20); // EOI to slave PIC
            outb(0x20, 0x20); // EOI to master PIC
            break;

        default:
            gfx_print("Unhandled interrupt: ");
            gfx_print_decimal(int_no);
            gfx_print(" (err=");
            gfx_print_decimal(err_code);
            gfx_print(")\n");
            break;
    }

    // Send EOI for hardware IRQs (32–47)
    if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) outb(0xA0, 0x20); // Slave PIC
        outb(0x20, 0x20);                   // Master PIC
    }
}

// ────────────────
// Exception Handlers
// ────────────────
void divide_by_zero_handler() {
    gfx_print("Divide-by-zero fault!\n");
    // Optional: halt or recover
}

// ────────────────
// IDT Gate Setup
// ────────────────
void set_idt_gate(int n, uint32_t handler) {
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel       = 0x08;     // Kernel code segment
    idt[n].always0   = 0;
    idt[n].flags     = 0x8E;     // Present, ring 0, interrupt gate
}

// ────────────────
// IDT Initialization
// ────────────────
void init_idt() {
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));

    set_idt_gate(0,  (uint32_t)isr0);   // Divide-by-zero
    set_idt_gate(33, (uint32_t)irq33);  // Keyboard
    set_idt_gate(32, (uint32_t)irq0_handler); // Timer
    // set_idt_gate(44, (uint32_t)irq44); // Mouse IRQ12 - Disabled to prevent conflicts
    idt_flush((uint32_t)&idt_ptr);
}


void timer_handler(struct regs* r) {
    (void)r; // Suppress unused parameter warning
    static uint32_t tick_count = 0;
    tick_count++;
    inc_ticks();
    clock_tick();
    
    /* Task manager timer tick for scheduling */
    task_timer_tick();

    // if(tick_count % 10 == 0) {
    //     // Every second at 100Hz
    //     gfx_print("Tick\n");
    // }   
    // Periodically flush any IRQ-queued debug lines to serial so they
    // become visible in headless captures.
    extern void irq_log_flush_to_serial(void);
    irq_log_flush_to_serial();
    send_eoi(32); // assuming regs contains int_no
}


void send_eoi(uint8_t int_no) { 
       if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) outb(0xA0, 0x20); // Slave PIC
        outb(0x20, 0x20);                   // Master PIC
    }
}

// ────────────────
// PIC Initialization   NOT USED - moved to assembly pic.asm
// ────────────────
//extern void init_pic();

// static void pic_init(void) {
//     uint8_t mask1 = inb(0x21);
//     outb(0x21, mask1 & ~0x02); // Unmask IRQ1 (keyboard)

//     outb(0x20, 0x11);  // ICW1: Master
//     outb(0xA0, 0x11);  // ICW1: Slave

//     outb(0x21, 0x20);  // ICW2: Master offset 0x20
//     outb(0xA1, 0x28);  // ICW2: Slave offset 0x28

//     outb(0x21, 0x04);  // ICW3: Master has slave at IRQ2
//     outb(0xA1, 0x02);  // ICW3: Slave connected to IRQ2

//     outb(0x21, 0x01);  // ICW4: 8086 mode
//     outb(0xA1, 0x01);  // ICW4: 8086 mode

//     outb(0x21, 0xFE); // Unmask IRQ0 only
//     outb(0xA1, 0xFF); // Mask all on slave
//     gfx_print("PICs initialized and keyboard unmasked.\n");
// }

// ────────────────
// System Initialization
// ────────────────
void keyboard_service_handler(regs_t* regs) {
    uint8_t scancode = inb(0x60);
    
    // Debug logging
    static int kbd_log_count = 0;
    if (kbd_log_count < 100) {
        SERIAL_LOG_HEX("KBD IRQ: 0x", scancode);
        kbd_log_count++;
    }
    
    keyboard_handler(regs, scancode);
    send_eoi(33);
}
    

void interrupts_system_init(void) {
    gfx_print("Setting up interrupt system...\n");

    gdt_init();       // Segment setup
    init_idt();       // IDT + IRQ stubs
    gfx_print("IDT initialized.\n");
    gfx_print("Remapping PIC...\n");
    init_pic();       // PIC remapping
    // Initialize PIT to 100Hz so sleep_ms and timer ticks advance
    init_timer(100);
    // Log PIC masks to help debug IRQ masking
    uint8_t mask1 = inb(0x21);
    uint8_t mask2 = inb(0xA1);
    SERIAL_LOG_HEX("PIC1 mask=0x", mask1);
    SERIAL_LOG_HEX("PIC2 mask=0x", mask2);
    register_interrupt_handler(0, divide_by_zero_handler);
    register_interrupt_handler(32, timer_handler);
    register_interrupt_handler(33, keyboard_service_handler);
    // register_interrupt_handler(44, mouse_handler); // Disabled to prevent USB/PS2 mouse conflicts
    GFX_LOG_MIN("Keyboard handler registered for IRQ1 (vector 33).\n");
    //GFX_LOG_MIN("Mouse handler disabled (USB mouse in use).\n");
    gfx_print("GDT and IDT setup complete.\n");
}

// ────────────────
// Quantum Enhancements
// ────────────────
void quantum_interrupts_init(void) {
    gfx_print("Quantum-aware interrupt handling enabled.\n");
}