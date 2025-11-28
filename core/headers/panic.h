/**
 * QARMA Kernel Panic and Assertion Support
 */

#ifndef QARMA_PANIC_H
#define QARMA_PANIC_H

// Kernel panic function - displays error and halts
void kernel_panic(const char* message) __attribute__((noreturn));

// Assertion support
void kernel_assert_failed(const char* expr, const char* file, int line) __attribute__((noreturn));

// Assert macro - checks condition and panics if false
#ifdef NDEBUG
    #define KERNEL_ASSERT(expr) ((void)0)
#else
    #define KERNEL_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                kernel_assert_failed(#expr, __FILE__, __LINE__); \
            } \
        } while(0)
#endif

// Convenience macros for common panic scenarios
#define PANIC(msg) kernel_panic(msg)
#define PANIC_IF(condition, msg) \
    do { \
        if (condition) { \
            kernel_panic(msg); \
        } \
    } while(0)

#define PANIC_IF_NULL(ptr, msg) PANIC_IF((ptr) == NULL, msg)

// Unreachable code marker
#define UNREACHABLE() kernel_panic("Unreachable code executed")

#endif // QARMA_PANIC_H
