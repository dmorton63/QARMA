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
extern void idt_flush(uint64_t);
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

// IDT initialization moved to idt.c for 64-bit compatibility
// init_idt() is now idt_init() in idt.c


void timer_handler(struct regs* r) {
    (void)r; // Suppress unused parameter warning
    static uint32_t tick_count = 0;
    tick_count++;
    inc_ticks();
    clock_tick();
    
    /* Task manager timer tick for scheduling */
    task_timer_tick();

    // Poll USB keyboard every tick (10ms at 100Hz)
    extern void usb_keyboard_poll_xhci(void);
    usb_keyboard_poll_xhci();
    
    // Poll USB mouse less frequently to avoid cursor racing ahead
    // Every 5 ticks = 50ms polling rate
    if (tick_count % 5 == 0) {
        extern void usb_mouse_poll_xhci(void);
        usb_mouse_poll_xhci();
    }
    
    // Render if framebuffer is dirty (e.g., from console toggle)
    // Check every 2 ticks (20ms) to avoid excessive rendering
    if (tick_count % 2 == 0) {
        extern bool fb_is_dirty(void);
        extern void compositor_render_all(void);
        if (fb_is_dirty()) {
            compositor_render_all();
        }
    }

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
    // Check status register to determine if data is from keyboard or mouse
    uint8_t status = inb(0x64);
    uint8_t scancode = inb(0x60);
    
    // Bit 5 of status register: 0 = keyboard data, 1 = mouse data
    if (status & 0x20) {
        // This is mouse data leaking into keyboard IRQ - discard it
        static int mouse_discard_count = 0;
        if (mouse_discard_count < 10) {
            SERIAL_LOG("KBD IRQ: Discarding mouse data 0x");
            SERIAL_LOG_HEX("", scancode);
            SERIAL_LOG("\n");
            mouse_discard_count++;
        }
        send_eoi(33);
        return;
    }
    
    // Debug logging for actual keyboard data
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
    idt_init();       // IDT + IRQ stubs (64-bit)
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
    // register_interrupt_handler(33, keyboard_service_handler); // Disabled - using USB keyboard
    // register_interrupt_handler(44, mouse_handler); // Disabled - using USB mouse
    
    // Mask IRQ1 (PS/2 keyboard) and IRQ12 (PS/2 mouse) in PIC since we're using USB
    uint8_t current_mask1 = inb(0x21);
    uint8_t current_mask2 = inb(0xA1);
    outb(0x21, current_mask1 | 0x02);  // Mask IRQ1 (bit 1)
    outb(0xA1, current_mask2 | 0x10);  // Mask IRQ12 (bit 4)
    
    GFX_LOG_MIN("PS/2 keyboard/mouse interrupts disabled (USB in use).\n");
    gfx_print("GDT and IDT setup complete.\n");
}

// ────────────────
// Quantum Enhancements
// ────────────────
void quantum_interrupts_init(void) {
    gfx_print("Quantum-aware interrupt handling enabled.\n");
}