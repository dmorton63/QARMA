/**
 * Interrupt Handler Wrappers
 * 
 * These wrappers ensure proper stack alignment when calling C handlers
 * from assembly interrupt stubs. Only these functions need stack realignment,
 * not the entire kernel.
 */

#include "stdtools.h"

// Forward declarations of real handlers
extern void timer_handler(void* regs);
extern void interrupt_handler(uint32_t int_no, uint32_t err_code);
extern void cpu_exception_handler(uint32_t exception_num, uint32_t error_code, 
                                   uint64_t rip, uint64_t cs, uint64_t rflags);

// Wrapper for timer interrupt - ensures stack alignment
__attribute__((force_align_arg_pointer))
void timer_handler_wrapper(void* regs) {
    timer_handler(regs);
}

// Wrapper for generic interrupt handler - ensures stack alignment
__attribute__((force_align_arg_pointer))
void interrupt_handler_wrapper(uint32_t int_no, uint32_t err_code) {
    interrupt_handler(int_no, err_code);
}

// Wrapper for CPU exception handler - ensures stack alignment
__attribute__((force_align_arg_pointer))
void cpu_exception_handler_wrapper(uint32_t exception_num, uint32_t error_code,
                                    uint64_t rip, uint64_t cs, uint64_t rflags) {
    cpu_exception_handler(exception_num, error_code, rip, cs, rflags);
}
