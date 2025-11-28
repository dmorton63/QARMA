/**
 * QARMA - Graphics and Display Library
 * Unified graphics interface supporting multiple display modes
 */

#ifndef QUANTUM_GRAPHICS_H
#define QUANTUM_GRAPHICS_H

#include "core/kernel.h"

// Display mode types
typedef enum {
    DISPLAY_MODE_UNDEFINED = -1,
    DISPLAY_MODE_TEXT_VGA = 0,      // Standard VGA text mode 80x25
    DISPLAY_MODE_FRAMEBUFFER = 1,   // Linear framebuffer (VESA/GOP)
    DISPLAY_MODE_VGA_GRAPHICS = 2,  // VGA 320x200x256 graphics mode
    DISPLAY_MODE_SERIAL_CONSOLE = 3 // Serial port console
} display_mode_t;

// Color definitions (compatible across modes)
typedef enum {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
    COLOR_CYAN = 3,
    COLOR_RED = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN = 6,
    COLOR_LIGHT_GRAY = 7,
    COLOR_DARK_GRAY = 8,
    COLOR_LIGHT_BLUE = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11,
    COLOR_LIGHT_RED = 12,
    COLOR_LIGHT_MAGENTA = 13,
    COLOR_YELLOW = 14,
    COLOR_WHITE = 15
} color_t;

// RGB color structure for framebuffer mode
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} rgb_color_t;

// Display information structure
typedef struct {
    display_mode_t mode;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;           // Bytes per line (for framebuffer)
    uint32_t bpp;             // Bits per pixel
    uint32_t* framebuffer;    // Linear framebuffer address
    uint32_t red_mask;
    uint16_t green_mask;
    uint16_t blue_mask;
    uint8_t red_pos;
    uint8_t green_pos;
    uint8_t blue_pos;
    uint32_t cursor_x;
    uint32_t cursor_y;
    color_t fg_color;
    color_t bg_color;
} display_info_t;

// Font structure for graphics modes
typedef struct {
    const uint8_t* data;
    uint32_t width;
    uint32_t height;
    uint32_t char_spacing;
    uint32_t line_spacing;
} font_t;

void test_function_call(void);

// Graphics mode functions
void graphics_init(multiboot_info_t* mb_info);
void test_simple_function(int mode);
void alternative_set_mode(display_mode_t mode);
void graphics_set_mode(display_mode_t mode);
display_info_t *graphics_get_info(void);
display_mode_t graphics_get_mode(void);
display_info_t* graphics_get_display_info(void);

// Core printing functions
void gfx_putchar(char c);
void gfx_putchar_renamed(char c);  // Renamed version due to execution issues
void gfx_print(const char* str);
void gfx_printf(const char* format, ...);
void gfx_print_hex(uint32_t value);
void gfx_print_decimal(uint32_t value);
void gfx_print_binary(uint32_t value);

// Cursor and color management
void gfx_set_cursor(uint32_t x, uint32_t y);
void gfx_set_cursor_position(uint32_t x, uint32_t y);
void gfx_get_cursor(uint32_t* x, uint32_t* y);
void gfx_set_colors(color_t fg, color_t bg);
void gfx_clear_screen(void);
void gfx_scroll_up(void);

// Advanced graphics functions (for framebuffer/graphics modes)
void gfx_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color);
void gfx_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, rgb_color_t color);
void gfx_draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color);
void gfx_draw_filled_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color);
void gfx_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg, font_t* font);
void gfx_draw_string(uint32_t x, uint32_t y, const char* str, rgb_color_t fg, rgb_color_t bg, font_t* font);

// Utility functions
rgb_color_t color_to_rgb(color_t color);
color_t rgb_to_color(rgb_color_t rgb);
uint32_t rgb_to_pixel(rgb_color_t color, uint32_t bpp,uint8_t red_pos, uint8_t green_pos, uint8_t blue_pos) ;

// Backend-specific functions (internal)
void vga_text_init(void);
void framebuffer_init(void);
void framebuffer_test(void);
void serial_console_init(void);

// Runtime graphics mode control
bool graphics_force_framebuffer_mode(void);
bool graphics_force_vga_mode(void);

// Font demonstration
void graphics_demo_fonts(void);

// Debug information
void graphics_debug_info(void);

// Printf-style function for compatibility
void gfx_printf(const char* format, ...);

#endif // QUANTUM_GRAPHICS_H