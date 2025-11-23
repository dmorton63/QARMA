#include "sleep.h"
#include "timer.h"

// Simple millisecond sleep using system tick counter
void sleep_ms(uint32_t ms) {
    uint32_t target_ticks = get_ticks() + (ms / MS_PER_TICK);
    while (get_ticks() < target_ticks) {
        halt();  // HLT wrapper from timer.h
    }
}

// Microsecond delay using busy-wait loop
// This is needed for USB HID devices (mice/keyboards) that require sub-millisecond timing
// Calibrated for typical i386 CPU speeds (assuming ~1000MHz, adjust if needed)
void sleep_us(uint32_t us) {
    if (us == 0) return;
    
    // For very small delays, use I/O port delay (each inb takes ~1μs)
    if (us < 10) {
        for (uint32_t i = 0; i < us; i++) {
            __asm__ volatile("outb %%al, $0x80" : : "a"(0) : "memory");
        }
        return;
    }
    
    // For larger delays, use RDTSC if available, otherwise use calibrated loop
    // Simple calibrated busy-wait loop (approximately 1000 iterations per microsecond)
    volatile uint32_t count = us * 1000;
    while (count--) {
        __asm__ volatile("nop");
    }
}