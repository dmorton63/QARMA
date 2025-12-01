/**
 * QARMA Kernel - Main Entry Point
 * 
 * Simplified kernel entry point that delegates initialization to init.c
 */

#include "multiboot.h"
#include "config.h"
#include "graphics.h"
#include "init.h"
#include "rtc.h"
#include "log_timestamp.h"

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

bool g_log_use_datetime = false;

static void print_two_digits(uint32_t v) {
    char buf[3];
    buf[0] = '0' + ((v / 10) % 10);
    buf[1] = '0' + (v % 10);
    buf[2] = '\0';
    serial_debug(buf);
}

static void print_three_digits(uint32_t v) {
    char buf[4];
    buf[0] = '0' + ((v / 100) % 10);
    buf[1] = '0' + ((v / 10) % 10);
    buf[2] = '0' + (v % 10);
    buf[3] = '\0';
    serial_debug(buf);
}

void log_print_timestamp(void) {
    if (g_log_use_datetime) {
        rtc_datetime_t dt;
        rtc_read(&dt);
        // Format: YYYY-MM-DD HH:MM:SS
        serial_debug("[");
        serial_debug_decimal(dt.year);
        serial_debug("-");
        print_two_digits(dt.month);
        serial_debug("-");
        print_two_digits(dt.day);
        serial_debug(" ");
        print_two_digits(dt.hour);
        serial_debug(":");
        print_two_digits(dt.minute);
        serial_debug(":");
        print_two_digits(dt.second);
        serial_debug("] ");
        return;
    }

    // Fallback: [ticks:millis]
    uint32_t _ts_ticks = get_ticks();
    uint32_t _ts_ms = (uint32_t)get_system_time_millis(TIMER_FREQUENCY);
    serial_debug("[");
    serial_debug_decimal(_ts_ticks);
    serial_debug(":");
    serial_debug_decimal(_ts_ms);
    serial_debug("ms] ");
}
void serial_debug_signed(int32_t value) {
    if (value < 0) {
        serial_debug("-");
        value = -value;
    }
    serial_debug_decimal((uint32_t)value);
}

// Minimal printf-style formatter for serial logging
// Supports %s, %d, %u, %x, %c
static void serial_tiny_vformat(char* out, size_t outsz, const char* fmt, va_list ap) {
    size_t pos = 0;
    const char* p = fmt ? fmt : "";
    while (*p && pos + 1 < outsz) {
        if (*p != '%') {
            out[pos++] = *p++;
            continue;
        }
        p++;
        if (*p == '%') { out[pos++] = '%'; p++; continue; }
        if (*p == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && pos + 1 < outsz) out[pos++] = *s++;
            p++;
        } else if (*p == 'c') {
            int c = va_arg(ap, int);
            out[pos++] = (char)c;
            p++;
        } else if (*p == 'u' || *p == 'd') {
            int is_signed = (*p == 'd');
            long v = is_signed ? va_arg(ap, int) : (long)va_arg(ap, unsigned int);
            if (is_signed && v < 0) { if (pos + 1 < outsz) out[pos++] = '-'; v = -v; }
            char buf[32]; int bp = 0; unsigned long uv = (unsigned long)v;
            if (uv == 0) buf[bp++] = '0';
            while (uv && bp < (int)sizeof(buf)) { buf[bp++] = (char)('0' + (uv % 10)); uv /= 10; }
            for (int i = bp - 1; i >= 0 && pos + 1 < outsz; --i) out[pos++] = buf[i];
            p++;
        } else if (*p == 'x' || *p == 'X') {
            unsigned int v = va_arg(ap, unsigned int);
            const char* hex = (*p == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            char buf[8]; int bp = 0;
            if (v == 0) buf[bp++] = '0';
            while (v && bp < (int)sizeof(buf)) { buf[bp++] = hex[v & 0xF]; v >>= 4; }
            for (int i = bp - 1; i >= 0 && pos + 1 < outsz; --i) out[pos++] = buf[i];
            p++;
        } else {
            // Unknown specifier: emit literally
            out[pos++] = '%';
            if (*p && pos + 1 < outsz) out[pos++] = *p;
            if (*p) p++;
        }
    }
    out[pos] = '\0';
}

void serial_logf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    serial_tiny_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    serial_debug(buf);
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
    
    // Run desktop (handles its own event loop, no shell_init needed)
    //qarma_run_desktop();
    
    SERIAL_LOG("[KERNEL] qarma_run_desktop returned (should never happen)\n");
    
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
