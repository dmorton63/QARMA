#include "panic.h"
#include "graphics/graphics.h"

void panic(const char* msg)
{
    gfx_printf("[QARMA WINDOW MANAGER PANIC] %s\n", msg);
    while (1) {
        __asm__ volatile("hlt");
    }
}