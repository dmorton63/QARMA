#include "system_control.h"
#include "io.h"
#include "config.h"

// Shutdown the system via ACPI or fallback methods
void system_shutdown(void) {
    SERIAL_LOG("[SYSTEM] Initiating shutdown...\n");
    
    // Try QEMU/Bochs debug exit first (for testing)
    outw(0x604, 0x2000);  // QEMU
    outw(0xB004, 0x2000); // Bochs
    
    // Try ACPI shutdown (SLP_TYPa/SLP_TYPb  via PM1a_CNT)
    // This is simplified - real ACPI requires parsing DSDT/FADT
    outw(0x604, 0x2000);
    
    // If we're still here, halt the CPU
    SERIAL_LOG("[SYSTEM] Shutdown failed, halting CPU\n");
    __asm__ volatile("cli; hlt");
    
    // Infinite loop as fallback
    while(1) {
        __asm__ volatile("hlt");
    }
}

// Reboot the system via keyboard controller
void system_reboot(void) {
    SERIAL_LOG("[SYSTEM] Initiating reboot...\n");
    
    // Method 1: Keyboard controller reset (most compatible)
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);  // Pulse reset line
    
    // Method 2: Triple fault (if keyboard controller doesn't work)
    __asm__ volatile("lidt 0");  // Load invalid IDT
    __asm__ volatile("int3");    // Trigger interrupt with no IDT
    
    // Fallback: halt
    SERIAL_LOG("[SYSTEM] Reboot failed, halting CPU\n");
    while(1) {
        __asm__ volatile("hlt");
    }
}
