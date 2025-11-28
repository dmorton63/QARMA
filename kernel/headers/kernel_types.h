#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#include "core/stdtools.h"


// Null pointer
#ifndef NULL
#define NULL ((void*)0)
#endif

// Maximum values
#define UINT8_MAX   0xFF
#define UINT16_MAX  0xFFFF
#define UINT32_MAX  0xFFFFFFFF
#define UINT64_MAX  0xFFFFFFFFFFFFFFFF

#define INT8_MAX    0x7F
#define INT16_MAX   0x7FFF
#define INT32_MAX   0x7FFFFFFF
#define INT64_MAX   0x7FFFFFFFFFFFFFFF

#define INT8_MIN    (-INT8_MAX - 1)
#define INT16_MIN   (-INT16_MAX - 1)
#define INT32_MIN   (-INT32_MAX - 1)
#define INT64_MIN   (-INT64_MAX - 1)

// Common macros
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Bit manipulation macros
#define BIT(n)           (1U << (n))
#define SET_BIT(x, n)    ((x) |= BIT(n))
#define CLEAR_BIT(x, n)  ((x) &= ~BIT(n))
#define TOGGLE_BIT(x, n) ((x) ^= BIT(n))
#define TEST_BIT(x, n)   (((x) & BIT(n)) != 0)

// Memory alignment attributes
#define PACKED           __attribute__((packed))
#define ALIGNED(x)       __attribute__((aligned(x)))
#define SECTION(name)    __attribute__((section(name)))

// Function attributes
#define NORETURN         __attribute__((noreturn))
#define INLINE           __attribute__((always_inline)) inline
#define NOINLINE         __attribute__((noinline))
#define UNUSED           __attribute__((unused))

// Memory barriers and atomic operations
#define MEMORY_BARRIER() __asm__ __volatile__("" ::: "memory")
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

// Assembly instruction macros
#define CLI() __asm__ __volatile__("cli")
#define STI() __asm__ __volatile__("sti") 
#define HLT() __asm__ __volatile__("hlt")
#define NOP() __asm__ __volatile__("nop")

// Likely/unlikely branch prediction hints
#define likely(x)        __builtin_expect(!!(x), 1)
#define unlikely(x)      __builtin_expect(!!(x), 0)

extern bool is_ready;

typedef struct regs {
    uint32_t gs, fs, es, ds;                          // Segment registers
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // General purpose registers
    uint32_t int_no, err_code;                        // Interrupt number + error code
    uint32_t eip, cs, eflags, useresp, ss;            // Pushed by CPU automatically
} regs_t;

#endif // KERNEL_TYPES_H