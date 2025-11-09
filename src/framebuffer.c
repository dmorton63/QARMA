#include "framebuffer.h"

#define FB_WIDTH 80
#define FB_HEIGHT 25
#define FB_ADDRESS 0xB8000

static uint16_t* const fb = (uint16_t*)FB_ADDRESS;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t color = 0x07; // light grey on black

static uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void fb_clear(void) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = make_vga_entry(' ', color);
    }
    cursor_x = 0;
    cursor_y = 0;
}

void fb_write_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        fb[cursor_y * FB_WIDTH + cursor_x] = make_vga_entry(c, color);
        cursor_x++;
        if (cursor_x >= FB_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= FB_HEIGHT) {
        fb_clear(); // crude scroll
    }
}

void fb_write_string(const char* str) {
    while (*str) {
        fb_write_char(*str++);
    }
}

void fb_set_color(uint8_t fg, uint8_t bg) {
    color = (bg << 4) | (fg & 0x0F);
}