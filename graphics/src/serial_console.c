/**
 * QARMA - Serial Console Backend
 * Serial port output for debugging and remote console access
 */
#include "serial_console.h"
#include "core/io.h"

#define SERIAL_PORT_A 0x3F8
#define SERIAL_PORT_B 0x2F8

static uint32_t serial_x = 0;
static uint32_t serial_y = 0;

// Initialize serial port
void serial_console_init(void) {
    display_info_t* info = graphics_get_info();
    
    // Configure serial port A (COM1)
    outb(SERIAL_PORT_A + 1, 0x00);  // Disable all interrupts
    outb(SERIAL_PORT_A + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    outb(SERIAL_PORT_A + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_PORT_A + 1, 0x00);  // (hi byte)
    outb(SERIAL_PORT_A + 3, 0x03);  // 8 bits, no parity, one stop bit
    outb(SERIAL_PORT_A + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
    outb(SERIAL_PORT_A + 4, 0x0B);  // IRQs enabled, RTS/DSR set
    
    serial_x = 0;
    serial_y = 0;
    
    // Update global display info
    info->width = 80;    // Virtual width
    info->height = 25;   // Virtual height
    info->cursor_x = 0;
    info->cursor_y = 0;
}

// Check if serial port is ready to transmit
static bool serial_is_transmit_ready(void) {
    return inb(SERIAL_PORT_A + 5) & 0x20;
}

// Send a single character to serial port
static void serial_write_char(char c) {
    while (!serial_is_transmit_ready()) {
        // Wait for transmit buffer to be empty
    }
    outb(SERIAL_PORT_A, c);
}

void serial_putchar(char c) {
    display_info_t* info = graphics_get_info();
    
    switch (c) {
        case '\n':
            serial_write_char('\r'); // Send carriage return before newline
            serial_write_char('\n');
            serial_x = 0;
            serial_y++;
            if (serial_y >= info->height) {
                serial_y = info->height - 1; // Stay at bottom
            }
            break;
            
        case '\r':
            serial_write_char('\r');
            serial_x = 0;
            break;
            
        case '\b':
            if (serial_x > 0) {
                serial_write_char('\b');
                serial_write_char(' ');
                serial_write_char('\b');
                serial_x--;
            }
            break;
            
        case '\t':
            // Convert tab to spaces
            do {
                serial_write_char(' ');
                serial_x++;
            } while ((serial_x % 4) != 0 && serial_x < info->width);
            
            if (serial_x >= info->width) {
                serial_x = 0;
                serial_y++;
                if (serial_y >= info->height) {
                    serial_y = info->height - 1;
                }
            }
            break;
            
        default:
            serial_write_char(c);
            serial_x++;
            if (serial_x >= info->width) {
                serial_x = 0;
                serial_y++;
                if (serial_y >= info->height) {
                    serial_y = info->height - 1;
                }
            }
            break;
    }
    
    info->cursor_x = serial_x;
    info->cursor_y = serial_y;
}

void serial_clear(void) {
    display_info_t* info = graphics_get_info();
    
    // Send ANSI escape sequence to clear screen if terminal supports it
    serial_write_char('\033');  // ESC
    serial_write_char('[');
    serial_write_char('2');
    serial_write_char('J');
    
    // Reset cursor position
    serial_write_char('\033');  // ESC
    serial_write_char('[');
    serial_write_char('H');
    
    serial_x = 0;
    serial_y = 0;
    info->cursor_x = 0;
    info->cursor_y = 0;
}

void serial_set_cursor(uint32_t x, uint32_t y) {
    display_info_t* info = graphics_get_info();
    
    if (x < info->width && y < info->height) {
        serial_x = x;
        serial_y = y;
        
        // Send ANSI escape sequence to position cursor
        // Format: ESC[row;colH (1-based indexing)
        serial_write_char('\033');  // ESC
        serial_write_char('[');
        
        // Convert y+1 to string and send
        if (y + 1 >= 10) {
            serial_write_char('0' + ((y + 1) / 10));
        }
        serial_write_char('0' + ((y + 1) % 10));
        
        serial_write_char(';');
        
        // Convert x+1 to string and send
        if (x + 1 >= 10) {
            serial_write_char('0' + ((x + 1) / 10));
        }
        serial_write_char('0' + ((x + 1) % 10));
        
        serial_write_char('H');
        
        info->cursor_x = x;
        info->cursor_y = y;
    }
}

void serial_scroll(void) {
    // For serial console, scrolling is handled by the terminal
    // Just send a newline to advance
    serial_write_char('\n');
    
    display_info_t* info = graphics_get_info();
    serial_x = 0;
    if (serial_y < info->height - 1) {
        serial_y++;
    }
    info->cursor_x = serial_x;
    info->cursor_y = serial_y;
}

// Serial-specific functions
void serial_print_color(const char* str, color_t fg, color_t bg) {
    // Send ANSI color codes if terminal supports them
    // Foreground colors: 30-37, background: 40-47
    
    serial_write_char('\033');  // ESC
    serial_write_char('[');
    
    // Foreground color
    if (fg < 8) {
        serial_write_char('3');
        serial_write_char('0' + fg);
    } else {
        serial_write_char('9');
        serial_write_char('0' + (fg - 8));
    }
    
    serial_write_char(';');
    
    // Background color
    if (bg < 8) {
        serial_write_char('4');
        serial_write_char('0' + bg);
    } else {
        serial_write_char('1');
        serial_write_char('0');
        serial_write_char('0' + (bg - 8));
    }
    
    serial_write_char('m');
    
    // Print the string
    while (*str) {
        serial_putchar(*str++);
    }
    
    // Reset colors
    serial_write_char('\033');
    serial_write_char('[');
    serial_write_char('0');
    serial_write_char('m');
}

void serial_set_colors(color_t fg, color_t bg) {
    serial_write_char('\033');  // ESC
    serial_write_char('[');
    
    // Foreground color
    if (fg < 8) {
        serial_write_char('3');
        serial_write_char('0' + fg);
    } else {
        serial_write_char('9');
        serial_write_char('0' + (fg - 8));
    }
    
    serial_write_char(';');
    
    // Background color
    if (bg < 8) {
        serial_write_char('4');
        serial_write_char('0' + bg);
    } else {
        serial_write_char('1');
        serial_write_char('0');
        serial_write_char('0' + (bg - 8));
    }
    
    serial_write_char('m');
}

// Check if serial port has received data
bool serial_has_data(void) {
    return inb(SERIAL_PORT_A + 5) & 1;
}

// Read a character from serial port (non-blocking)
char serial_read_char(void) {
    if (serial_has_data()) {
        return inb(SERIAL_PORT_A);
    }
    return 0; // No data available
}