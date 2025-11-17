#include "keyboard.h"
#include "keyboard_types.h"
#include "shell/shell.h"
#include "command.h"
#include "graphics/graphics.h"
#include "core/io.h"
#include "graphics/irq_logger.h"

// Global flag for detecting any keypress
volatile bool key_pressed = false;

// Global keyboard state
static const char scancode_to_ascii_lower[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', // 0x00-0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  // 0x10-0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  // 0x20-0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x30-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  // 0x40-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    // 0x50-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0     // 0x70-0x7F
};

static const char scancode_to_ascii_upper[128] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', // 0x00-0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  // 0x10-0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  // 0x20-0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x30-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  // 0x40-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    // 0x50-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0     // 0x70-0x7F
};



// Define global keyboard state (extern declared in keyboard_types.h)
keyboard_state_t kb_state;

// Whether keyboard processing is enabled. When false, IRQs still enqueue
// scancodes into the raw scancode buffer but higher-level processing (echo,
// command handling, etc.) is suppressed so modal UI will not consume keys.
static bool keyboard_enabled = true;

// Track extended scancode prefix (0xE0)
static bool extended_scancode = false;

// Simple scancode ring buffer for UI consumers that need raw scancodes
#define SCANCODE_BUF_SIZE 128
static uint8_t scancode_buf[SCANCODE_BUF_SIZE];
static uint16_t scancode_head = 0;
static uint16_t scancode_tail = 0;
static uint16_t scancode_count = 0;

// Window keyboard buffer - captures ALL key events for window system
#define WIN_KEY_BUF_SIZE 256
static key_event_t win_key_buf[WIN_KEY_BUF_SIZE];
static uint16_t win_key_head = 0;
static uint16_t win_key_tail = 0;
static uint16_t win_key_count = 0;
static bool win_key_enabled = false;

bool keyboard_has_scancode(void) {
    return scancode_count > 0;
}

uint8_t keyboard_get_scancode(void) {
    if (scancode_count == 0) return 0;
    uint8_t v = scancode_buf[scancode_head];
    scancode_head = (scancode_head + 1) % SCANCODE_BUF_SIZE;
    scancode_count--;
    return v;
}

// Window keyboard buffer functions
void keyboard_enable_window_mode(bool enable) {
    SERIAL_LOG(enable ? "ENABLING window mode" : "DISABLING window mode");
    win_key_enabled = enable;
    if (enable) {
        // Clear buffer when enabling - zero out entire buffer
        for (int i = 0; i < WIN_KEY_BUF_SIZE; i++) {
            win_key_buf[i].scancode = 0;
            win_key_buf[i].extended = 0;
            win_key_buf[i].released = 0;
            win_key_buf[i].modifiers = 0;
        }
        win_key_head = 0;
        win_key_tail = 0;
        win_key_count = 0;
        SERIAL_LOG("Window mode enabled, buffer cleared and zeroed");
    }
}

bool keyboard_is_window_mode_enabled(void) {
    return win_key_enabled;
}

bool keyboard_has_window_key_event(void) {
    // No cli/sti needed - reading count is atomic and we can tolerate stale reads
    return win_key_count > 0;
}

uint16_t keyboard_get_window_key_count(void) {
    // No cli/sti needed - reading count is atomic
    return win_key_count;
}

bool keyboard_get_window_key_event(key_event_t* out) {
    if (!out) return false;
    
    static int get_log = 0;
    if (get_log < 20) {
        SERIAL_LOG_DEC("GET_EVENT: count=", win_key_count);
        get_log++;
    }
    
    // Check without cli - if we race, worst case we return false
    if (win_key_count == 0) {
        return false;
    }
    
    // Read and advance - this needs to be quick but not interrupt-blocking
    // The interrupt handler only writes, we only read/advance head
    *out = win_key_buf[win_key_head];
    
    if (get_log < 25) {
        SERIAL_LOG("  RETURNING TRUE!");
        SERIAL_LOG_HEX("  Reading scancode=0x", out->scancode);
        SERIAL_LOG_DEC("  head=", win_key_head);
        SERIAL_LOG_DEC("  tail=", win_key_tail);
    }
    
    win_key_head = (win_key_head + 1) % WIN_KEY_BUF_SIZE;
    win_key_count--;
    return true;
}

// Peek next scancode without consuming it. Returns true if a scancode is
// available and writes it to *out.
bool keyboard_peek_scancode(uint8_t *out) {
    if (!out) return false;
    bool has = false;
    // brief critical section to stabilize head/count
    __asm__ volatile("cli");
    if (scancode_count > 0) {
        *out = scancode_buf[scancode_head];
        has = true;
    }
    __asm__ volatile("sti");
    return has;
}

// Peek scancode at offset (lookahead) without consuming. offset=0 is same
// as keyboard_peek_scancode.
bool keyboard_peek_scancode_at(size_t offset, uint8_t *out) {
    if (!out) return false;
    bool has = false;
    __asm__ volatile("cli");
    if (scancode_count > offset) {
        size_t idx = (scancode_head + offset) % SCANCODE_BUF_SIZE;
        *out = scancode_buf[idx];
        has = true;
    }
    __asm__ volatile("sti");
    return has;
}

// Peek next ASCII char from the input buffer without consuming it.
bool keyboard_peek_char(char *out) {
    if (!out) return false;
    bool has = false;
    __asm__ volatile("cli");
    if (kb_state.buffer_count > 0) {
        *out = kb_state.input_buffer[kb_state.buffer_head];
        has = true;
    }
    __asm__ volatile("sti");
    return has;
}


bool keyboard_init(void) {
    GFX_LOG_MIN("Initializing keyboard subsystem...\n");

    // Clear keyboard state
    memset(&kb_state, 0, sizeof(keyboard_state_t));

    // Initialize buffer pointers
    kb_state.buffer_head = 0;
    kb_state.buffer_tail = 0;
    kb_state.buffer_count = 0;
    kb_state.command_ready = false;

    // Clear all modifier states
    memset(&kb_state.modifiers, 0, sizeof(key_modifiers_t));

    // --- Enable keyboard IRQs on the 8042 controller ---
    // Read command byte
    outb(KEYBOARD_COMMAND_PORT, 0x20); // 0x20 = Read Command Byte
    uint8_t command_byte = inb(KEYBOARD_DATA_PORT);
    // Set bit 0 (enable IRQ1)
    command_byte |= 0x01;
    // Write command byte
    outb(KEYBOARD_COMMAND_PORT, 0x60); // 0x60 = Write Command Byte
    outb(KEYBOARD_DATA_PORT, command_byte);

    return true;
}



void keyboard_handler(regs_t* regs, uint8_t scancode) {

    // Set global flag for any keypress detection
    if (!(scancode & KEY_RELEASE)) {
        key_pressed = true;
    }

    static uint32_t interrupt_count = 0;
    interrupt_count++;
    
    // Debug: Log first few keyboard interrupts
    if (interrupt_count <= 5) {
        SERIAL_LOG_HEX("[KBD_HANDLER] Interrupt #", interrupt_count);
        SERIAL_LOG_HEX("[KBD_HANDLER] Scancode: ", scancode);
    }
    
    keyboard_process_scancode(scancode);    

    //gfx_print_decimal(scancode);
    // Debug: Show first few interrupts
    // if (interrupt_count <= 5) {
    //     gfx_print("Keyboard interrupt #");
    //     gfx_print_decimal(interrupt_count);
    //     gfx_print("\n");
    // }
            // if (int_no >= 40) outb(0xA0, 0x20); // Slave PIC
    keyboard_send_eoi(regs->int_no);

        // if (int_no >= 32 && int_no < 48) {

}

void keyboard_send_eoi(uint32_t int_no) {
    if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) outb(0xA0, 0x20); // Slave PIC
        outb(0x20, 0x20);                   // Master PIC
    }
}


void keyboard_process_scancode(uint8_t scancode) {
    // Push raw scancode into scancode buffer for consumers that poll it.
    if (scancode_count < SCANCODE_BUF_SIZE - 1) {
        scancode_buf[scancode_tail] = scancode;
        scancode_tail = (scancode_tail + 1) % SCANCODE_BUF_SIZE;
        scancode_count++;
    }
    
    // Handle extended scancode prefix (0xE0)
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;  // Don't process 0xE0 itself
    }
    
    // Build key event structure (initialize all fields explicitly)
    key_event_t event;
    event.scancode = scancode & 0x7F;  // Remove release bit, keep only 7 bits
    event.extended = extended_scancode ? 1 : 0;
    event.released = (scancode & KEY_RELEASE) ? 1 : 0;
    
    // Capture current modifier state
    event.modifiers = 0;
    if (kb_state.modifiers.shift_left || kb_state.modifiers.shift_right) {
        event.modifiers |= MODIFIER_SHIFT;
    }
    if (kb_state.modifiers.ctrl_left || kb_state.modifiers.ctrl_right) {
        event.modifiers |= MODIFIER_CTRL;
    }
    if (kb_state.modifiers.alt_left || kb_state.modifiers.alt_right) {
        event.modifiers |= MODIFIER_ALT;
    }
    
    // If window mode is enabled, capture ALL key events
    if (win_key_enabled && win_key_count < WIN_KEY_BUF_SIZE - 1) {
        static int win_log_count = 0;
        if (win_log_count < 50) {
            SERIAL_LOG_HEX("WIN_BUF write event, scancode=0x", event.scancode);
            SERIAL_LOG_DEC("  extended=", event.extended);
            SERIAL_LOG_DEC("  released=", event.released);
            win_log_count++;
        }
        
        win_key_buf[win_key_tail] = event;
        win_key_tail = (win_key_tail + 1) % WIN_KEY_BUF_SIZE;
        win_key_count++;
    }
    
    // Mock mouse disabled - focusing on USB mouse
    // extern void mock_mouse_handle_key_event(key_event_t event);
    // mock_mouse_handle_key_event(event);
    
    // If this was an extended scancode, don't let it fall through to normal processing
    if (extended_scancode) {
        extended_scancode = false;  // Reset for next scancode
        return;  // Don't process extended codes as regular keys
    }

    // If keyboard processing is disabled, do not dispatch to the higher-
    // level handlers. This allows modal UI (popups) to be bypassed or the
    // shell to continue seeing raw input without interference.
    if (!keyboard_enabled) return;

    if (scancode & KEY_RELEASE) {
        // Key release
        keyboard_handle_key_release(scancode & ~KEY_RELEASE);
    } else {
        // Key press
        keyboard_handle_key_press(scancode);
    }
}

void keyboard_set_enabled(bool enabled) {
    keyboard_enabled = enabled;
}

bool keyboard_is_enabled(void) {
    return keyboard_enabled;
}

void keyboard_handle_key_press(uint8_t scancode) {
    // Handle modifier keys and special keys
    switch (scancode) {
        case KEY_CTRL:
            kb_state.modifiers.ctrl_left = true;
            SERIAL_LOG("Ctrl pressed\n");
            return;
            
        case KEY_LSHIFT:
            kb_state.modifiers.shift_left = true;
            SERIAL_LOG("Left Shift pressed\n");
            return;
            
        case KEY_RSHIFT:
            kb_state.modifiers.shift_right = true;
            SERIAL_LOG("Right Shift pressed\n");
            return;
           
        case KEY_ALT:
            kb_state.modifiers.alt_left = true;
            SERIAL_LOG("Alt pressed\n");
            return;
            
        case KEY_CAPS:
            kb_state.modifiers.caps_lock = !kb_state.modifiers.caps_lock;
            SERIAL_LOG("Caps Lock toggled\n");
            return;

        case KEY_BACKSPACE:
            if (kb_state.buffer_count > 0) {
                kb_state.buffer_count--;
                kb_state.input_buffer[kb_state.buffer_count] = '\0';
                // Simple backspace: move cursor back, print space, move back again
                gfx_print("\b \b");
            }
            return;
            
        case KEY_ENTER:
            // Null-terminate the current input
            kb_state.input_buffer[kb_state.buffer_count] = '\0';
            
            // Print newline
            gfx_print("\n");
            
            // Process the command if there's input
            if (kb_state.buffer_count > 0) {
                execute_command(kb_state.input_buffer);
            }
            
            // Clear input buffer for next command
            keyboard_clear_buffer();
            
            // Show prompt for next command
            show_prompt("/");
            
            SERIAL_LOG("Enter pressed, command processed\n");
            return;
            
        case KEY_PGUP:
        case KEY_PGDN:
        case KEY_UP:
        case KEY_DOWN:
            // Ignore these keys for now
            return;
            
        default:
            // Handle regular character input
            if (is_printable_key(scancode)) {
                char ascii = scancode_to_ascii(scancode,
                    kb_state.modifiers.shift_left || kb_state.modifiers.shift_right,
                    kb_state.modifiers.caps_lock);
                    
                if (ascii != 0 && kb_state.buffer_count < KEYBOARD_BUFFER_SIZE - 1) {
                    kb_state.input_buffer[kb_state.buffer_count] = ascii;
                    kb_state.buffer_count++;
                    gfx_putchar(ascii); // Echo character to screen
                }
            }
            break;
    }
    
    // Handle special key combinations
    if (keyboard_ctrl_pressed()) {
        keyboard_handle_ctrl_combo(scancode);
    }
}

void keyboard_handle_key_release(uint8_t scancode) {
    switch (scancode) {
        case KEY_CTRL:
            kb_state.modifiers.ctrl_left = false;
            break;
            
        case KEY_LSHIFT:
            kb_state.modifiers.shift_left = false;
            break;
            
        case KEY_RSHIFT:
            kb_state.modifiers.shift_right = false;
            break;
            
        case KEY_ALT:
            kb_state.modifiers.alt_left = false;
            break;
    }
}

void keyboard_handle_ctrl_combo(uint8_t scancode) {
    switch (scancode) {
        case 0x2E: // Ctrl+C
            keyboard_clear_buffer();
            gfx_print("^C\n");
            show_prompt("/");
            break;
            
        case 0x26: // Ctrl+L
            gfx_clear_screen();
            show_prompt("/");
            break;
            
        case 0x20: // Ctrl+D
            // EOF signal - ignore for now
            break;
            
        default:
            break;
    }
}

char scancode_to_ascii(uint8_t scancode, bool shift, bool caps) {
    if (scancode >= 128) {
        return 0; // Invalid scancode
    }
    
    bool use_upper = shift;
    
    // Handle caps lock for letters only
    if (caps && scancode >= 0x10 && scancode <= 0x32) {
        // Letter keys (Q-P, A-L, Z-M ranges)
        if ((scancode >= 0x10 && scancode <= 0x19) || // Q-P
            (scancode >= 0x1E && scancode <= 0x26) || // A-L  
            (scancode >= 0x2C && scancode <= 0x32)) { // Z-M
            use_upper = !use_upper;
        }
    }
    
    return use_upper ? scancode_to_ascii_upper[scancode] : scancode_to_ascii_lower[scancode];
}

void keyboard_add_to_buffer(char c) {
    if (kb_state.buffer_count < KEYBOARD_BUFFER_SIZE - 1) {
        kb_state.input_buffer[kb_state.buffer_tail] = c;
        kb_state.buffer_tail = (kb_state.buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
        kb_state.buffer_count++;
        
        if (c == '\n') {
            kb_state.command_ready = true;
        }
    }
}

struct keyboard_state* get_keyboard_state(void) {
    return &kb_state;
}


char keyboard_get_char(void) {
    if (kb_state.buffer_count == 0) {
        return 0; // No characters available
    }
    
    char c = kb_state.input_buffer[kb_state.buffer_head];
    kb_state.buffer_head = (kb_state.buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    kb_state.buffer_count--;
    
    return c;
}

bool keyboard_has_input(void) {
    return kb_state.buffer_count > 0;
}

void keyboard_clear_buffer(void) {
    kb_state.buffer_head = 0;
    kb_state.buffer_tail = 0;
    kb_state.buffer_count = 0;
    kb_state.command_ready = false;
    memset(kb_state.input_buffer, 0, KEYBOARD_BUFFER_SIZE);
}

bool keyboard_ctrl_pressed(void) {
    return kb_state.modifiers.ctrl_left || kb_state.modifiers.ctrl_right;
}

bool keyboard_shift_pressed(void) {
    return kb_state.modifiers.shift_left || kb_state.modifiers.shift_right;
}

bool keyboard_alt_pressed(void) {
    return kb_state.modifiers.alt_left || kb_state.modifiers.alt_right;
}

bool is_printable_key(uint8_t scancode) {
    return scancode_to_ascii_lower[scancode] != 0;
}

bool is_modifier_key(uint8_t scancode) {
    switch (scancode) {
        case KEY_CTRL:
        case KEY_LSHIFT:
        case KEY_RSHIFT:
        case KEY_ALT:
        case KEY_CAPS:
            return true;
        default:
            return false;
    }
}

const char* keyboard_get_input_buffer(void) {
    return kb_state.input_buffer;
}

void keyboard_reset_input(void) {
    keyboard_clear_buffer();
}

void keyboard_set_debug(bool enable) {
    (void)enable;
}

// Convenience wrappers for event polling
bool keyboard_has_event(void) {
    return keyboard_has_window_key_event();
}

key_event_t keyboard_poll_event(void) {
    key_event_t event = {0};
    keyboard_get_window_key_event(&event);
    return event;
}