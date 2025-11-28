#pragma once

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            __asm__ __volatile__("int3"); /* Trigger breakpoint interrupt */ \
        } \
    } while (0)
