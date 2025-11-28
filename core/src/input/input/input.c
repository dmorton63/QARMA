// input.c

#include "input/input.h"
#include "keyboard.h"

char wait_for_key(void) {
    // Poll keyboard buffer exposed by keyboard API
    while (!keyboard_has_input()) {
        __asm__ volatile ("hlt");
    }

    return keyboard_get_char();
}