/**
 * QARMA - VGA Text Mode Backend
 * 80x25 character text display implementation
 */

#include "graphics/graphics.h"
#include "core/io.h"

// Forward declaration
void vga_text_scroll(void);

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static size_t vga_row = 0;
static size_t vga_column = 0;
static uint8_t vga_color = 0x07; // Light gray on black

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void vga_text_init(void) {
    display_info_t* info = graphics_get_info();
    
    // Clear screen
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
    
    vga_row = 0;
    vga_column = 0;
    
    // Update global display info
    info->width = VGA_WIDTH;
    info->height = VGA_HEIGHT;
    info->cursor_x = 0;
    info->cursor_y = 0;
}

void vga_text_putchar(char c) {
    display_info_t* info = graphics_get_info();
    
    // Update color from global state
    vga_color = vga_entry_color(info->fg_color, info->bg_color);
    
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            vga_text_scroll();
        }
        info->cursor_x = vga_column;
        info->cursor_y = vga_row;
        return;
    }
    
    if (c == '\r') {
        vga_column = 0;
        info->cursor_x = vga_column;
        return;
    }
    
    if (c == '\b') { // Backspace
        if (vga_column > 0) {
            vga_column--;
            const size_t index = vga_row * VGA_WIDTH + vga_column;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
        info->cursor_x = vga_column;
        return;
    }
    
    if (c == '\t') { // Tab - align to next 4-character boundary
        vga_column = (vga_column + 4) & ~3;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row == VGA_HEIGHT) {
                vga_text_scroll();
            }
        }
        info->cursor_x = vga_column;
        info->cursor_y = vga_row;
        return;
    }
    
    // Regular character
    const size_t index = vga_row * VGA_WIDTH + vga_column;
    vga_buffer[index] = vga_entry(c, vga_color);
    
    if (++vga_column == VGA_WIDTH) {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            vga_text_scroll();
        }
    }
    
    info->cursor_x = vga_column;
    info->cursor_y = vga_row;
}

void vga_text_clear(void) {
    display_info_t* info = graphics_get_info();
    vga_color = vga_entry_color(info->fg_color, info->bg_color);
    
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
    
    vga_row = 0;
    vga_column = 0;
    info->cursor_x = 0;
    info->cursor_y = 0;
}

void vga_text_set_cursor(uint32_t x, uint32_t y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_column = x;
        vga_row = y;
        
        // Update hardware cursor
        uint16_t pos = y * VGA_WIDTH + x;
        
        // Output cursor position to VGA controller
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

void vga_text_scroll(void) {
    display_info_t* info = graphics_get_info();
    vga_color = vga_entry_color(info->fg_color, info->bg_color);
    
    // Move all lines up by one
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t src_index = (y + 1) * VGA_WIDTH + x;
            const size_t dst_index = y * VGA_WIDTH + x;
            vga_buffer[dst_index] = vga_buffer[src_index];
        }
    }
    
    // Clear the last line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        vga_buffer[index] = vga_entry(' ', vga_color);
    }
    
    // Move cursor to last line
    vga_row = VGA_HEIGHT - 1;
    vga_column = 0;
    info->cursor_x = vga_column;
    info->cursor_y = vga_row;
}

// VGA text mode specific functions
void vga_text_set_color(color_t fg, color_t bg) {
    vga_color = vga_entry_color(fg, bg);
}

void vga_text_put_char_at(char c, uint32_t x, uint32_t y, color_t fg, color_t bg) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        const size_t index = y * VGA_WIDTH + x;
        uint8_t color = vga_entry_color(fg, bg);
        vga_buffer[index] = vga_entry(c, color);
    }
}

void vga_text_draw_box(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t fg, color_t bg) {
    // Draw a simple ASCII box
    if (x + width > VGA_WIDTH || y + height > VGA_HEIGHT) return;
    
    uint8_t color = vga_entry_color(fg, bg);
    
    // Top and bottom borders
    for (uint32_t i = 0; i < width; i++) {
        char top_char = (i == 0) ? '+' : (i == width - 1) ? '+' : '-';
        char bottom_char = (i == 0) ? '+' : (i == width - 1) ? '+' : '-';
        
        vga_buffer[y * VGA_WIDTH + x + i] = vga_entry(top_char, color);
        vga_buffer[(y + height - 1) * VGA_WIDTH + x + i] = vga_entry(bottom_char, color);
    }
    
    // Side borders and interior
    for (uint32_t j = 1; j < height - 1; j++) {
        for (uint32_t i = 0; i < width; i++) {
            char ch = (i == 0 || i == width - 1) ? '|' : ' ';
            vga_buffer[(y + j) * VGA_WIDTH + x + i] = vga_entry(ch, color);
        }
    }
}