#pragma once

#include "kernel_types.h"  // For regs_t
#include "keyboard_types.h"          // For keyboard_state_t, key_modifiers_t

// Modifier key flags
#define MODIFIER_SHIFT  0x01
#define MODIFIER_CTRL   0x02
#define MODIFIER_ALT    0x04

// Key event structure - captures full context of a keyboard event
typedef struct __attribute__((packed)) {
    uint8_t scancode;    // The scancode (raw, without KEY_RELEASE bit)
    uint8_t extended;    // 1 if this was preceded by 0xE0, 0 otherwise
    uint8_t released;    // 1 for key release, 0 for key press
    uint8_t modifiers;   // Bitmask of MODIFIER_* flags at time of event
} key_event_t;







bool keyboard_init(void);
//void keyboard_handler(regs_t* regs);
void keyboard_send_eoi(uint32_t int_no);
void keyboard_process_scancode(uint8_t scancode);
void keyboard_set_enabled(bool enabled);
bool keyboard_is_enabled(void);
void keyboard_handle_key_press(uint8_t scancode);
void keyboard_handle_key_release(uint8_t scancode);
void keyboard_handle_ctrl_combo(uint8_t scancode);
void keyboard_clear_buffer(void);
bool keyboard_ctrl_pressed(void);
bool is_printable_key(uint8_t scancode);
char scancode_to_ascii(uint8_t scancode, bool shift, bool caps);
void keyboard_add_to_buffer(char c);
keyboard_state_t* get_keyboard_state(void);
// Simple polling helpers used by input layer
bool keyboard_has_input(void);
char keyboard_get_char(void);

// Raw scancode buffer helpers (useful for modal UI needing immediate scancode
// events). These expose scancodes from the IRQ-driven handler without
// interfering with the ASCII input buffer.
bool keyboard_has_scancode(void);
uint8_t keyboard_get_scancode(void);

// Window keyboard buffer - captures ALL key events for window/mouse control
void keyboard_enable_window_mode(bool enable);
bool keyboard_is_window_mode_enabled(void);
bool keyboard_has_window_key_event(void);
bool keyboard_get_window_key_event(key_event_t* out);
uint16_t keyboard_get_window_key_count(void);

// Peek APIs: read buffered scancodes/chars without consuming them. These are
// used by modal UI to decide when to act without removing keys from the
// input stream. Implementations must be careful with concurrent IRQ updates.
bool keyboard_peek_scancode(uint8_t *out);
bool keyboard_peek_scancode_at(size_t offset, uint8_t *out);

// ASCII buffer peek
bool keyboard_peek_char(char *out);
