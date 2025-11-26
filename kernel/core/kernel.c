/**
 * QARMA Kernel - Main Entry Point
 * 
 * Simplified kernel entry point that delegates initialization to init.c
 */

#include "multiboot.h"
#include "config.h"
#include "graphics/graphics.h"
#include "core/init.h"

// Global verbosity level  
verbosity_level_t g_verbosity = VERBOSITY_VERBOSE;

// I/O port functions
uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Serial debug functions
void serial_debug(const char* msg) {
    const char* ptr = msg;
    while (*ptr) {
        while ((inb(0x3F8 + 5) & 0x20) == 0);
        outb(0x3F8, *ptr);
        ptr++;
    }
}

void serial_debug_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[9] = "00000000";
    for (int i = 7; i >= 0; i--) {
        buffer[7-i] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    serial_debug(buffer);
}

void serial_debug_decimal(uint32_t value) {
    char buffer[11];
    char* ptr = buffer + 10;
    *ptr = '\0';
    
    do {
        *--ptr = '0' + (value % 10);
        value /= 10;
    } while (value > 0);
    
    serial_debug(ptr);
}

void serial_debug_signed(int32_t value) {
    if (value < 0) {
        serial_debug("-");
        value = -value;
    }
    serial_debug_decimal((uint32_t)value);
}

// Callback for successful login
static void on_login_success(const char* username) {
    SERIAL_LOG("[KERNEL] User logged in: ");
    SERIAL_LOG((char*)username);
    SERIAL_LOG("\n");
    gfx_print("Login successful! Welcome, ");
    gfx_print((char*)username);
    gfx_print("\n");
}

/**
 * Main kernel entry point
 */
int kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // Debug marker M - entered kernel_main
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'M', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // Early debug output using VGA text mode
    volatile char* vga_buffer = (volatile char*)0xB8000;
    const char* msg = "BOOT: kernel_main started     ";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_buffer[80*2 + i * 2] = msg[i];
        vga_buffer[80*2 + i * 2 + 1] = 0x07;
    }

    // Initialize all subsystems
    qarma_init_all(magic, mbi);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'N', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Run login screen - Legacy disabled
    // qarma_run_login_screen(on_login_success);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'O', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Run shell instead of desktop (for testing)
    extern void shell_init(void);
    extern void shell_run(void);
    
    // Enable interrupts for shell
    __asm__ volatile("sti");
    SERIAL_LOG("[KERNEL] Interrupts enabled for shell\n");
    
    SERIAL_LOG("[KERNEL] About to call shell_init\n");
    shell_init();
    SERIAL_LOG("[KERNEL] shell_init returned, about to call shell_run\n");
    shell_run();
    SERIAL_LOG("[KERNEL] shell_run returned (should never happen)\n");
    
    // Should never reach here
    return 0;
}

/**
 * Kernel idle loop - runs when all tasks complete
 * This is the final fallback scheduler that never returns
 */
void kernel_idle_loop(void) {
    SERIAL_LOG("[KERNEL] Entering idle loop\n");
    
    // Infinite loop with HLT to save power
    while (1) {
        // Enable interrupts and halt
        // CPU will wake on next interrupt, then loop back
        __asm__ volatile(
            "sti\n"     // Enable interrupts
            "hlt\n"     // Halt until interrupt
            "cli\n"     // Disable interrupts before loop
        );
        
        // Optional: Call scheduler tick or task management here
        // quantum_scheduler_tick();
    }
}
