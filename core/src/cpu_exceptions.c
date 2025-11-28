/**
 * QARMA CPU Exception Handlers
 * 
 * Handles all x86-64 CPU exceptions (0-31) with detailed error reporting
 * and graceful system halt instead of silent reboots.
 */

#include "panic.h"
#include "io.h"
#include "config.h"

// Exception names for display
static const char* exception_names[32] = {
    "Divide by Zero",                    // 0
    "Debug",                             // 1
    "Non-Maskable Interrupt",            // 2
    "Breakpoint",                        // 3
    "Overflow",                          // 4
    "Bound Range Exceeded",              // 5
    "Invalid Opcode",                    // 6
    "Device Not Available",              // 7
    "Double Fault",                      // 8
    "Coprocessor Segment Overrun",       // 9
    "Invalid TSS",                       // 10
    "Segment Not Present",               // 11
    "Stack-Segment Fault",               // 12
    "General Protection Fault",          // 13
    "Page Fault",                        // 14
    "Reserved",                          // 15
    "x87 FPU Error",                     // 16
    "Alignment Check",                   // 17
    "Machine Check",                     // 18
    "SIMD Floating-Point Exception",     // 19
    "Virtualization Exception",          // 20
    "Control Protection Exception",      // 21
    "Reserved",                          // 22
    "Reserved",                          // 23
    "Reserved",                          // 24
    "Reserved",                          // 25
    "Reserved",                          // 26
    "Reserved",                          // 27
    "Hypervisor Injection Exception",    // 28
    "VMM Communication Exception",       // 29
    "Security Exception",                // 30
    "Reserved"                           // 31
};

// Helper to convert number to string
static void uint_to_str(uint64_t value, char* buffer, int base) {
    const char* digits = "0123456789ABCDEF";
    char temp[32];
    int i = 0;
    
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    while (value > 0) {
        temp[i++] = digits[value % base];
        value /= base;
    }
    
    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

// Decode General Protection Fault error code
// See Intel SDM Vol. 3, §6.13 for details
static void decode_gp_error(uint32_t error_code) {
    extern void serial_debug(const char* msg);
    char num_buf[32];
    
    if (error_code == 0) {
        serial_debug("[GPF] Error code = 0 (no selector involved)\n");
        serial_debug("[GPF] Likely invalid memory access or privileged instruction\n");
        return;
    }

    uint16_t selector_index = (error_code >> 3) & 0x1FFF; // bits 3–15
    uint8_t table = (error_code >> 1) & 0x3;              // bits 1–2
    uint8_t external = error_code & 0x1;                  // bit 0

    const char *table_name;
    switch (table) {
        case 0: table_name = "GDT"; break;
        case 1: table_name = "LDT"; break;
        case 2: table_name = "IDT"; break;
        default: table_name = "Unknown"; break;
    }

    serial_debug("[GPF] Error code = 0x");
    uint_to_str(error_code, num_buf, 16);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("[GPF] Selector index = ");
    uint_to_str(selector_index, num_buf, 10);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("[GPF] Table = ");
    serial_debug(table_name);
    serial_debug("\n");
    
    serial_debug("[GPF] External = ");
    serial_debug(external ? "Yes" : "No");
    serial_debug("\n");
}

/**
 * Generic CPU exception handler
 * Called from interrupt_handler for exceptions 0-31
 */
void cpu_exception_handler(uint32_t exception_num, uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    char panic_msg[512];
    char num_buf[32];
    int pos = 0;
    
    // Build panic message
    const char* exc_name = exception_names[exception_num];
    
    // Copy exception name
    while (*exc_name && pos < 400) {
        panic_msg[pos++] = *exc_name++;
    }
    
    // Add exception number
    const char* num_label = " (Exception #";
    while (*num_label && pos < 400) {
        panic_msg[pos++] = *num_label++;
    }
    
    uint_to_str(exception_num, num_buf, 10);
    char* p = num_buf;
    while (*p && pos < 400) {
        panic_msg[pos++] = *p++;
    }
    
    panic_msg[pos++] = ')';
    panic_msg[pos] = '\0';
    
    // Log detailed information to serial
    extern void serial_debug(const char* msg);
    serial_debug("\n\n");
    serial_debug("==================================================\n");
    serial_debug("          CPU EXCEPTION DETECTED\n");
    serial_debug("==================================================\n");
    serial_debug("Exception: ");
    serial_debug(exception_names[exception_num]);
    serial_debug("\n");
    
    serial_debug("Number:    ");
    uint_to_str(exception_num, num_buf, 10);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("Error:     0x");
    uint_to_str(error_code, num_buf, 16);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("RIP:       0x");
    uint_to_str(rip, num_buf, 16);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("CS:        0x");
    uint_to_str(cs, num_buf, 16);
    serial_debug(num_buf);
    serial_debug("\n");
    
    serial_debug("RFLAGS:    0x");
    uint_to_str(rflags, num_buf, 16);
    serial_debug(num_buf);
    serial_debug("\n");
    
    // Special handling for specific exceptions
    if (exception_num == 13) {  // General Protection Fault
        decode_gp_error(error_code);
    } else if (exception_num == 14) {  // Page Fault
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_debug("CR2 (fault address): 0x");
        uint_to_str(cr2, num_buf, 16);
        serial_debug(num_buf);
        serial_debug("\n");
        
        serial_debug("Page Fault Details:\n");
        serial_debug("  Present:   ");
        serial_debug((error_code & 0x1) ? "YES" : "NO");
        serial_debug("\n");
        serial_debug("  Write:     ");
        serial_debug((error_code & 0x2) ? "YES" : "NO");
        serial_debug("\n");
        serial_debug("  User mode: ");
        serial_debug((error_code & 0x4) ? "YES" : "NO");
        serial_debug("\n");
        serial_debug("  Reserved:  ");
        serial_debug((error_code & 0x8) ? "YES" : "NO");
        serial_debug("\n");
        serial_debug("  Exec:      ");
        serial_debug((error_code & 0x10) ? "YES" : "NO");
        serial_debug("\n");
    }
    
    serial_debug("==================================================\n\n");
    
    // Trigger kernel panic with the exception message
    kernel_panic(panic_msg);
}

/**
 * Individual exception handlers
 * These provide entry points for each specific exception type
 */

void exception_divide_by_zero(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(0, error_code, rip, cs, rflags);
}

void exception_debug(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(1, error_code, rip, cs, rflags);
}

void exception_nmi(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(2, error_code, rip, cs, rflags);
}

void exception_breakpoint(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(3, error_code, rip, cs, rflags);
}

void exception_overflow(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(4, error_code, rip, cs, rflags);
}

void exception_bound_range(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(5, error_code, rip, cs, rflags);
}

void exception_invalid_opcode(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(6, error_code, rip, cs, rflags);
}

void exception_device_not_available(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(7, error_code, rip, cs, rflags);
}

void exception_double_fault(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(8, error_code, rip, cs, rflags);
}

void exception_invalid_tss(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(10, error_code, rip, cs, rflags);
}

void exception_segment_not_present(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(11, error_code, rip, cs, rflags);
}

void exception_stack_fault(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(12, error_code, rip, cs, rflags);
}

void exception_general_protection(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(13, error_code, rip, cs, rflags);
}

void exception_page_fault(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(14, error_code, rip, cs, rflags);
}

void exception_fpu_error(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(16, error_code, rip, cs, rflags);
}

void exception_alignment_check(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(17, error_code, rip, cs, rflags);
}

void exception_machine_check(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(18, error_code, rip, cs, rflags);
}

void exception_simd_fp(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(19, error_code, rip, cs, rflags);
}

void exception_virtualization(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(20, error_code, rip, cs, rflags);
}

void exception_security(uint32_t error_code, uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(30, error_code, rip, cs, rflags);
}
