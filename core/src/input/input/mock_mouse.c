/**
 * QARMA - Mock Mouse Driver
 * 
 * Provides keyboard-based mouse control when USB mouse isn't working.
 * Arrow keys move cursor, WASD for fine control, Space/Enter for clicks.
 */

#include "mock_mouse.h"
#include "mouse.h"
#include "../io.h"
#include "graphics/graphics.h"
#include "config.h"

extern uint32_t fb_width;
extern uint32_t fb_height;
extern mouse_state_t mouse_state;

// Movement speed
#define MOUSE_SPEED_NORMAL  10
#define MOUSE_SPEED_FINE    2

// Keyboard scancodes
#define KEY_UP      0x48
#define KEY_DOWN    0x50
#define KEY_LEFT    0x4B
#define KEY_RIGHT   0x4D
#define KEY_W       0x11
#define KEY_A       0x1E
#define KEY_S       0x1F
#define KEY_D       0x20
#define KEY_SPACE   0x39
#define KEY_ENTER   0x1C

// Track key states for smooth movement
static bool keys_down[256] = {0};

void mock_mouse_init(void) {
    // Initialize mouse to center of screen
    mouse_state.x = fb_width / 2;
    mouse_state.y = fb_height / 2;
    mouse_state.dx = 0;
    mouse_state.dy = 0;
    mouse_state.left_pressed = false;
    mouse_state.right_pressed = false;
    mouse_state.middle_pressed = false;
    
    SERIAL_LOG("Mock mouse initialized (keyboard control)\n");
    SERIAL_LOG("  Arrow keys: Move cursor\n");
    SERIAL_LOG("  WASD: Fine movement\n");
    SERIAL_LOG("  Space: Left click\n");
    SERIAL_LOG("  Enter: Right click\n");
}

void mock_mouse_handle_key_event(key_event_t event) {
    // Build key index - extended keys get offset to avoid collision
    uint8_t key = event.scancode & 0x7F;
    if (event.extended) {
        key |= 0x80;  // Mark as extended in our tracking array
    }
    
    if (event.released) {
        keys_down[key] = false;
        
        // Release mouse buttons (not extended)
        if (!event.extended) {
            if ((event.scancode & 0x7F) == KEY_SPACE) {
                mouse_state.left_pressed = false;
            }
            if ((event.scancode & 0x7F) == KEY_ENTER) {
                mouse_state.right_pressed = false;
            }
        }
    } else {
        keys_down[key] = true;
        
        // Press mouse buttons (not extended)
        if (!event.extended) {
            if ((event.scancode & 0x7F) == KEY_SPACE) {
                mouse_state.left_pressed = true;
            }
            if ((event.scancode & 0x7F) == KEY_ENTER) {
                mouse_state.right_pressed = true;
            }
        }
    }
}

void mock_mouse_update(void) {
    int dx = 0;
    int dy = 0;
    int speed = MOUSE_SPEED_NORMAL;
    
    // Arrow keys are extended scancodes (0x80 | scancode)
    uint8_t ext_up = KEY_UP | 0x80;
    uint8_t ext_down = KEY_DOWN | 0x80;
    uint8_t ext_left = KEY_LEFT | 0x80;
    uint8_t ext_right = KEY_RIGHT | 0x80;
    
    // Check for fine movement (WASD)
    if (keys_down[KEY_W] || keys_down[KEY_A] || 
        keys_down[KEY_S] || keys_down[KEY_D]) {
        speed = MOUSE_SPEED_FINE;
        
        if (keys_down[KEY_W]) dy -= speed;
        if (keys_down[KEY_S]) dy += speed;
        if (keys_down[KEY_A]) dx -= speed;
        if (keys_down[KEY_D]) dx += speed;
    }
    // Normal movement (arrows - extended scancodes)
    else {
        if (keys_down[ext_up])    dy -= speed;
        if (keys_down[ext_down])  dy += speed;
        if (keys_down[ext_left])  dx -= speed;
        if (keys_down[ext_right]) dx += speed;
    }
    
    // Update mouse position
    if (dx != 0 || dy != 0) {
        mouse_state.dx = dx;
        mouse_state.dy = dy;
        mouse_state.x += dx;
        mouse_state.y += dy;
        
        // Clamp to screen bounds
        if (mouse_state.x < 0) mouse_state.x = 0;
        if (mouse_state.y < 0) mouse_state.y = 0;
        if (mouse_state.x >= (int)fb_width) mouse_state.x = fb_width - 1;
        if (mouse_state.y >= (int)fb_height) mouse_state.y = fb_height - 1;
    } else {
        mouse_state.dx = 0;
        mouse_state.dy = 0;
    }
}
