#include "keyboard.h"
#include "keyboard_types.h"
#include "shell/shell.h"
#include "command.h"
#include "graphics/graphics.h"
#include "core/io.h"
#include "graphics/irq_logger.h"
#include "qarma_win_handle/qarma_input_events.h"

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

// Keyboard focus - which component should receive keyboard events
// NULL = shell/console, non-NULL = focused window/component
static void* keyboard_focus_target = NULL;

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

// Keyboard focus management
void keyboard_set_focus(void* target) {
    keyboard_focus_target = target;
    if (target) {
        SERIAL_LOG("[KEYBOARD] Focus set to component\n");
    } else {
        SERIAL_LOG("[KEYBOARD] Focus set to shell/console\n");
    }
}

void* keyboard_get_focus(void) {
    return keyboard_focus_target;
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


// Shell keyboard event handler - processes keyboard events for shell/console
static void shell_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data) {
    static int shell_log = 0;
    if (shell_log < 5) {
        SERIAL_LOG("[SHELL_HANDLER] Called\n");
        shell_log++;
    }
    
    // Don't process if event was already handled by console or other handler
    if (event->handled) {
        return;
    }
    
    // Only process if shell has focus (focus_target == NULL)
    if (keyboard_focus_target != NULL) {
        if (shell_log < 10) {
            SERIAL_LOG("[SHELL_HANDLER] Shell does not have focus, ignoring\n");
        }
        return;
    }
    
    // Only process KEY_DOWN events
    if (event->type != QARMA_INPUT_EVENT_KEY_DOWN) {
        return;
    }
    
    // Don't process keys if console is hidden
    extern bool console_compositor_is_visible(void);
    if (!console_compositor_is_visible()) {
        SERIAL_LOG("[SHELL_HANDLER] Console hidden, ignoring key\n");
        event->handled = false;  // Let other handlers try
        return;
    }
    
    SERIAL_LOG("[SHELL_HANDLER] Processing key\n");
    
    uint8_t scancode = (uint8_t)event->data.key.scancode;
    char ascii = (char)event->data.key.character;
    bool ctrl = (event->data.key.modifiers & QARMA_MOD_CTRL) != 0;
    
    // Handle special keys
    switch (scancode) {
        case KEY_BACKSPACE:
            if (kb_state.buffer_count > 0) {
                kb_state.buffer_count--;
                kb_state.input_buffer[kb_state.buffer_count] = '\0';
                gfx_print("\b \b");
            }
            event->handled = true;
            return;
            
        case KEY_ENTER:
            kb_state.input_buffer[kb_state.buffer_count] = '\0';
            gfx_print("\n");
            if (kb_state.buffer_count > 0) {
                execute_command(kb_state.input_buffer);
            }
            keyboard_clear_buffer();
            show_prompt("/");
            event->handled = true;
            return;
            
        case KEY_PGUP:
        case KEY_PGDN:
        case KEY_UP:
        case KEY_DOWN:
            event->handled = true;
            return;
    }
    
    // Handle Ctrl combinations
    if (ctrl) {
        switch (scancode) {
            case 0x2E: // Ctrl+C
                keyboard_clear_buffer();
                gfx_print("^C\n");
                show_prompt("/");
                event->handled = true;
                return;
                
            case 0x26: // Ctrl+L
                gfx_clear_screen();
                show_prompt("/");
                event->handled = true;
                return;
        }
    }
    
    // Handle regular character input
    if (ascii != 0 && kb_state.buffer_count < KEYBOARD_BUFFER_SIZE - 1) {
        static int char_log = 0;
        if (char_log < 10) {
            SERIAL_LOG("[SHELL] Printing char: '");
            serial_debug_hex(ascii);
            SERIAL_LOG("'\n");
            char_log++;
        }
        kb_state.input_buffer[kb_state.buffer_count] = ascii;
        kb_state.buffer_count++;
        gfx_putchar(ascii);
        event->handled = true;
    }
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

// Register shell keyboard handler - must be called AFTER qarma_input_events_init()
void keyboard_register_shell_handler(void) {
    extern QARMA_INPUT_EVENT_LISTENER* qarma_input_event_listen(QARMA_INPUT_EVENT_TYPE event_type, QARMA_INPUT_EVENT_HANDLER handler, void* user_data, uint32_t priority);
    qarma_input_event_listen(QARMA_INPUT_EVENT_KEY_DOWN, shell_keyboard_handler, NULL, 100);
    SERIAL_LOG("[KEYBOARD] Shell keyboard handler registered\n");
}



void keyboard_handler(regs_t* regs, uint8_t scancode) {

    // Set global flag for any keypress detection
    if (!(scancode & KEY_RELEASE)) {
        key_pressed = true;
    }

    static uint32_t interrupt_count = 0;
    interrupt_count++;
    
    // Debug: Log ALL keyboard interrupts for now
    if (interrupt_count <= 200) {
        SERIAL_LOG_HEX("[KBD_HANDLER] Interrupt #", interrupt_count);
        SERIAL_LOG_HEX("[KBD_HANDLER] Scancode: ", scancode);
    }
    
    // Send EOI immediately to allow more interrupts
    keyboard_send_eoi(regs->int_no);
    
    // Now process the scancode (this may take longer)
    keyboard_process_scancode(scancode);

}

void keyboard_send_eoi(uint32_t int_no) {
    static int eoi_log = 0;
    if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) outb(0xA0, 0x20); // Slave PIC
        outb(0x20, 0x20);                   // Master PIC
        
        if (eoi_log < 20) {
            SERIAL_LOG("[KBD_EOI] Sent EOI\n");
            eoi_log++;
        }
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
    
    // Update modifier states first
    uint8_t base_scancode = scancode & 0x7F;
    bool is_release = (scancode & KEY_RELEASE) != 0;
    
    switch (base_scancode) {
        case KEY_CTRL:
            kb_state.modifiers.ctrl_left = !is_release;
            break;
        case KEY_LSHIFT:
            kb_state.modifiers.shift_left = !is_release;
            break;
        case KEY_RSHIFT:
            kb_state.modifiers.shift_right = !is_release;
            break;
        case KEY_ALT:
            kb_state.modifiers.alt_left = !is_release;
            break;
        case KEY_CAPS:
            if (!is_release) {
                kb_state.modifiers.caps_lock = !kb_state.modifiers.caps_lock;
            }
            break;
    }
    
    // Build modifier flags for event
    uint32_t modifiers = 0;
    if (kb_state.modifiers.shift_left || kb_state.modifiers.shift_right) {
        modifiers |= QARMA_MOD_SHIFT;
    }
    if (kb_state.modifiers.ctrl_left || kb_state.modifiers.ctrl_right) {
        modifiers |= QARMA_MOD_CTRL;
    }
    if (kb_state.modifiers.alt_left || kb_state.modifiers.alt_right) {
        modifiers |= QARMA_MOD_ALT;
    }
    if (kb_state.modifiers.caps_lock) {
        modifiers |= QARMA_MOD_CAPS;
    }
    
    // Create and dispatch keyboard event through QARMA event system
    extern QARMA_INPUT_EVENT qarma_input_event_create_key(QARMA_INPUT_EVENT_TYPE type, uint32_t scancode, uint32_t keycode, uint32_t modifiers, void* target);
    extern void qarma_input_event_dispatch(QARMA_INPUT_EVENT* event);
    
    QARMA_INPUT_EVENT_TYPE event_type = is_release ? QARMA_INPUT_EVENT_KEY_UP : QARMA_INPUT_EVENT_KEY_DOWN;
    QARMA_INPUT_EVENT kbd_event = qarma_input_event_create_key(event_type, base_scancode, base_scancode, modifiers, keyboard_focus_target);
    
    // Add character if it's printable and not a release
    if (!is_release && !extended_scancode) {
        char ascii = scancode_to_ascii(base_scancode,
            kb_state.modifiers.shift_left || kb_state.modifiers.shift_right,
            kb_state.modifiers.caps_lock);
        kbd_event.data.key.character = (uint32_t)ascii;
    }
    
    static int dispatch_log = 0;
    if (dispatch_log < 20) {
        SERIAL_LOG("[KBD_PROC] Dispatching event, scancode=");
        serial_debug_hex(base_scancode);
        SERIAL_LOG("\n");
        dispatch_log++;
    }
    
    qarma_input_event_dispatch(&kbd_event);
    
    // Reset extended flag
    if (extended_scancode) {
        extended_scancode = false;
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

// Legacy function stubs for compatibility
void keyboard_enable_window_mode(bool enable) {
    // Deprecated - now handled by keyboard_set_focus()
    keyboard_set_focus(enable ? (void*)1 : NULL);
}

bool keyboard_get_window_key_event(key_event_t* out) {
    // Deprecated - keyboard events now go through QARMA event system
    return false;
}

bool keyboard_has_event(void) {
    // Deprecated - check event queue instead
    return false;
}

key_event_t keyboard_poll_event(void) {
    // Deprecated - use QARMA event system
    key_event_t event = {0};
    return event;
}