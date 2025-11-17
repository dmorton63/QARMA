/**
 * QARMA - Console Output Implementation
 * Basic VGA text mode console for kernel output
 */

#include "core/kernel.h"
#include "io.h"

#define SERIAL_PORT_A 0x3F8

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

static void serial_putchar(char c) {
    outb(SERIAL_PORT_A, c);
}

void console_init(void) {
    // Clear screen
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_column = 0;
}

static void console_putchar(char c) {
    // Send to both VGA and serial
    serial_putchar(c);
    
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            vga_row = 0; // Simple wrap-around
        }
        return;
    }
    
    if (c == '\r') {
        vga_column = 0;
        return;
    }
    
    const size_t index = vga_row * VGA_WIDTH + vga_column;
    vga_buffer[index] = vga_entry(c, vga_color);
    
    if (++vga_column == VGA_WIDTH) {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            vga_row = 0; // Simple wrap-around
        }
    }
}

void console_print(const char* str) {
    while (*str) {
        console_putchar(*str++);
    }
}

void console_print_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11] = "0x";
    
    for (int i = 7; i >= 0; i--) {
        buffer[i + 2] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    buffer[10] = '\0';
    
    console_print(buffer);
}