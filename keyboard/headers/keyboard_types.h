#ifndef KEYBOARD_TYPES_H
#define KEYBOARD_TYPES_H

#include "kernel_types.h" 

// Keyboard hardware constants
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Special keys
#define KEY_ESC       0x01
#define KEY_BACKSPACE 0x0E
#define KEY_TAB       0x0F
#define KEY_ENTER     0x1C
#define KEY_CTRL      0x1D
#define KEY_LSHIFT    0x2A
#define KEY_RSHIFT    0x36
#define KEY_ALT       0x38
#define KEY_SPACE     0x39
#define KEY_CAPS      0x3A
#define KEY_F1        0x3B
#define KEY_F2        0x3C
#define KEY_F3        0x3D
#define KEY_F4        0x3E
#define KEY_F5        0x3F
#define KEY_F6        0x40
#define KEY_F7        0x41
#define KEY_F8        0x42
#define KEY_F9        0x43
#define KEY_F10       0x44

// Arrow keys (extended scancodes)
#define KEY_UP        0x48
#define KEY_DOWN      0x50
#define KEY_LEFT      0x4B
#define KEY_RIGHT     0x4D

// PGUP, PGDN, HOME, END (extended scancodes)
#define KEY_PGUP      0x49
#define KEY_PGDN      0x51
#define KEY_HOME      0x47
#define KEY_END       0x4F
// Insert, Delete (extended scancodes)
#define KEY_INSERT    0x52
#define KEY_DELETE    0x53
// Numeric keypad keys
#define KEY_NUMPAD_0  0x52
#define KEY_NUMPAD_1  0x4F
#define KEY_NUMPAD_2  0x50
#define KEY_NUMPAD_3  0x51
#define KEY_NUMPAD_4  0x4B
#define KEY_NUMPAD_5  0x4C
#define KEY_NUMPAD_6  0x4D
#define KEY_NUMPAD_7  0x47
#define KEY_NUMPAD_8  0x48
#define KEY_NUMPAD_9  0x49
#define KEY_NUMPAD_ADD 0x4E
#define KEY_NUMPAD_SUB 0x4A
#define KEY_NUMPAD_MUL 0x37
#define KEY_NUMPAD_DIV 0x35
#define KEY_NUMPAD_DECIMAL 0x53
#define KEY_NUMPAD_ENTER 0x1C

// Key release bit
#define KEY_RELEASE   0x80

// Buffer sizes
#define KEYBOARD_BUFFER_SIZE 512
#define MAX_INPUT_LENGTH     255

typedef struct {
    bool ctrl_left;
    bool ctrl_right;
    bool shift_left;
    bool shift_right;
    bool alt_left;
    bool alt_right;
    bool caps_lock;
    bool num_lock;
    bool scroll_lock;
} key_modifiers_t;

// Keyboard state
typedef struct keyboard_state{
    char input_buffer[KEYBOARD_BUFFER_SIZE];
    uint16_t buffer_head;
    uint16_t buffer_tail;
    uint16_t buffer_count;
    bool command_ready;
    key_modifiers_t modifiers;
} keyboard_state_t;

extern keyboard_state_t kb_state;
void keyboard_handler(regs_t* regs, uint8_t scancode);

#endif // KEYBOARD_TYPES_H