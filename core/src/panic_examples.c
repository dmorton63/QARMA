/**
 * QARMA Kernel Panic Usage Examples
 * 
 * This file demonstrates how to use the kernel panic system.
 * Include this documentation in your code where needed.
 * 
 * NOTE: This file is for reference only and is not actively used in the kernel.
 */

#include "panic.h"
#include "memory/heap.h"
#include "stdtools.h"

// Dummy definitions for examples
#define MAX_WINDOWS 32
#define KERNEL_STACK_BASE 0xFFFF800000000000ULL

// Dummy function for examples
static int check_hardware_status(void) { return 1; }

/*
 * BASIC USAGE:
 * 
 * 1. Simple panic with message:
 *    kernel_panic("Out of memory!");
 * 
 * 2. Conditional panic:
 *    if (critical_error) {
 *        kernel_panic("Critical hardware failure detected");
 *    }
 * 
 * 3. Using convenience macro:
 *    PANIC_IF(ptr == NULL, "NULL pointer dereference");
 * 
 * 4. Assert (debug builds only):
 *    KERNEL_ASSERT(window_count < MAX_WINDOWS);
 * 
 * EXAMPLE SCENARIOS:
 */

// Memory exhaustion
void example_memory_panic(void) {
    void* ptr = heap_alloc(1024);
    if (!ptr) {
        kernel_panic("Heap exhausted - unable to allocate memory");
    }
}

// Hardware failure
void example_hardware_panic(void) {
    if (!check_hardware_status()) {
        kernel_panic("Critical hardware fault detected");
    }
}

// Stack overflow detection
void example_stack_panic(void) {
    uint64_t stack_ptr;
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack_ptr));
    
    if (stack_ptr < KERNEL_STACK_BASE) {
        kernel_panic("Stack overflow detected");
    }
}

// Double fault handler
void double_fault_handler(void) {
    kernel_panic("Double fault - unrecoverable exception");
}

// Assertion example
void example_assert(int window_count) {
    KERNEL_ASSERT(window_count >= 0);
    KERNEL_ASSERT(window_count < MAX_WINDOWS);
}

// Unreachable code
void example_unreachable(int type) {
    switch(type) {
        case 0: return;
        case 1: return;
        default: 
            UNREACHABLE();  // Should never get here
    }
}
