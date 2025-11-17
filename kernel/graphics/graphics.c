/**
 * QARMA - Graphics Library Implementation
 * Clean, unified graphics interface with backend abstraction
 */

#include "graphics.h"
#include "font_data.h"
#include "core/multiboot.h"
#include "core/io.h"
#include "config.h"

// External debug functions
extern void serial_debug(const char* msg);
extern void serial_debug_hex(uint32_t value);
// Message box routing (forward-declared in config.h but make explicit here)
extern void message_box_logf(const char* fmt, ...);
extern void message_box_log(const char* msg);

// Function declarations for font rendering (implemented in fonts.c)
extern void draw_bitmap_char(uint32_t fb_x, uint32_t fb_y, char c, rgb_color_t fg_color, rgb_color_t bg_color, font_descriptor_t* font);
extern font_descriptor_t* get_font(font_type_t type);

// Global display state
static display_info_t g_display = {
    .mode = DISPLAY_MODE_TEXT_VGA,
    .width = 80,
    .height = 25,
    .pitch = 160,
    .bpp = 4,
    .framebuffer = (uint32_t*)0xB8000,
    .cursor_x = 0,
    .cursor_y = 0,
    .fg_color = COLOR_WHITE,
    .bg_color = COLOR_BLACK
};

// Function pointers for backend abstraction
static void (*backend_putchar)(char c) = NULL;
static void (*backend_clear)(void) = NULL;
static void (*backend_set_cursor)(uint32_t x, uint32_t y) = NULL;
static void (*backend_scroll)(void) = NULL;

// Backend implementations
extern void vga_text_putchar(char c);
extern void vga_text_clear(void);
extern void vga_text_set_cursor(uint32_t x, uint32_t y);
extern void vga_text_scroll(void);

extern void framebuffer_putchar(char c);
extern void framebuffer_clear(void);
extern void framebuffer_set_cursor(uint32_t x, uint32_t y);
extern void framebuffer_scroll(void);

static bool serial_is_transmit_ready(void);

extern void serial_putchar(char c);

// Graphics debug using centralized config.h macros
// All debug output now controlled by DEBUG_GRAPHICS and verbosity levels

// Test function for basic functionality
void test_function_call(void) {
    SERIAL_LOG("GFX_TEST: Function call works");
}

// Initialize graphics subsystem
void graphics_init(multiboot_info_t* mb_info) {
    SERIAL_LOG_MIN("GFX_INIT: Starting graphics initialization");
    
    if(mb_info && (mb_info->flags && (1 << 12))) {
        /* framebuffer_addr is 64-bit in multiboot structures; cast via
           uintptr_t to avoid pointer-size mismatch warnings on 32-bit
           vs 64-bit builds. */
        g_display.framebuffer = (uint32_t*)(uintptr_t)mb_info->framebuffer_info.framebuffer_addr;
        g_display.width = mb_info->framebuffer_info.framebuffer_width;
        g_display.height = mb_info->framebuffer_info.framebuffer_height;
        g_display.pitch = mb_info->framebuffer_info.framebuffer_pitch;
        g_display.bpp = mb_info->framebuffer_info.framebuffer_bpp;
        g_display.red_mask     = mb_info->framebuffer_info.framebuffer_red_mask_size;
        g_display.green_mask   = mb_info->framebuffer_info.framebuffer_green_mask_size;
        g_display.blue_mask    = mb_info->framebuffer_info.framebuffer_blue_mask_size;
       //g_display.reserved_mask = mb_info->framebuffer_info.framebuffer_reserved_mask_size;

        g_display.red_pos     = mb_info->framebuffer_info.framebuffer_red_field_position;
        g_display.green_pos   = mb_info->framebuffer_info.framebuffer_green_field_position;
        g_display.blue_pos    = mb_info->framebuffer_info.framebuffer_blue_field_position;
        //g_display.reserved_pos = mb_info->framebuffer_info.framebuffer_reserved_field_position;

        SERIAL_LOG_HEX("GFX_INIT: Framebuffer detected at ", (uint32_t)g_display.framebuffer);
        SERIAL_LOG("\n");   
    }
    // Get display info from multiboot
    display_info_t* display = graphics_get_display_info();
    uint32_t* saved_framebuffer = display->framebuffer;
    
    // Check if we have a valid framebuffer
    if (saved_framebuffer && saved_framebuffer != (uint32_t*)0xB8000) {
        SERIAL_LOG_MIN("GFX_INIT: Framebuffer detected, using framebuffer mode");
    
        graphics_set_mode(DISPLAY_MODE_FRAMEBUFFER);

    } else {
        SERIAL_LOG_MIN("GFX_INIT: No framebuffer, using VGA text mode");
        graphics_set_mode(DISPLAY_MODE_TEXT_VGA);
    }
    
    SERIAL_LOG_MIN("GFX_INIT: Graphics system initialized");
}

// Set display mode and configure backends
void graphics_set_mode(display_mode_t mode) {
    g_display.mode = mode;
    
    switch (mode) {
        case DISPLAY_MODE_TEXT_VGA:
            backend_putchar = vga_text_putchar;
            backend_clear = vga_text_clear;
            backend_set_cursor = vga_text_set_cursor;
            backend_scroll = vga_text_scroll;
            g_display.cursor_x = 0;
            g_display.cursor_y = 0;
            if (backend_set_cursor) backend_set_cursor(0, 0);
            break;
            
        case DISPLAY_MODE_FRAMEBUFFER:
            backend_putchar = framebuffer_putchar;
            backend_clear = framebuffer_clear;
            backend_set_cursor = framebuffer_set_cursor;
            backend_scroll = framebuffer_scroll;
            g_display.cursor_x = 0;
            g_display.cursor_y = 0;
            if (backend_set_cursor) backend_set_cursor(0, 0);
            if (backend_clear) backend_clear();
             
            break;
            
        case DISPLAY_MODE_SERIAL_CONSOLE:
        default:
            backend_putchar = serial_putchar;
            backend_clear = NULL;
            backend_set_cursor = NULL;
            backend_scroll = NULL;
            break;
    }
}


// Simple test function
void test_simple_function(int mode) {
    SERIAL_LOG("GFX_TEST: Simple function called with mode");
    // Log mode number using proper macro
    SERIAL_LOG_DEC("Mode: ", mode);
}

// Alternative set mode (for compatibility)
void alternative_set_mode(display_mode_t mode) {
    graphics_set_mode(mode);
}

// Core printing functions
void gfx_putchar(char c) {
    if (backend_putchar) {
        backend_putchar(c);
    }
}

void gfx_print(const char* str) {
    if (!str) return;
    g_display.mode = DISPLAY_MODE_SERIAL_CONSOLE;
    // Always route prints into the message box (so they are visible
    // in the UI). Avoid drawing into the desktop framebuffer/VGA to
    // prevent debug output from overwriting UI; if the system is in
    // serial-only mode, emit characters to the serial backend instead.
    // extern void message_box_log(const char* msg);
    // message_box_log(str);

    if (g_display.mode == DISPLAY_MODE_SERIAL_CONSOLE) {
        while (*str) {
            gfx_putchar(*str++);
        }
    } else {
            while (*str) {
                gfx_putchar(*str++);
            }
        }
    }
 void gfx_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%' && *(format + 1)) {
            format++;
            switch (*format) {
                case 's': {
                    const char* str = va_arg(args, const char*);
                    gfx_print(str);
                    break;
                }
                case 'u': {
                    uint32_t val = va_arg(args, uint32_t);
                    gfx_print_decimal(val);
                    break;
                }
                case 'X': {
                    uint32_t val = va_arg(args, uint32_t);
                    gfx_print("0x");
                    gfx_print_hex(val);  // See below
                    break;
                }
                default:
                    gfx_putchar('%');
                    gfx_putchar(*format);
                    break;
            }
        } else {
            gfx_putchar(*format);
        }
        format++;
    }

    va_end(args);
}   
void gfx_print_hex(uint32_t value) {
    gfx_print("0x");
    for (int i = 7; i >= 0; i--) {
        uint32_t nibble = (value >> (i * 4)) & 0xF;
        char c = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
        gfx_putchar(c);
    }
}

void gfx_print_decimal(uint32_t value) {
    if (value == 0) {
        gfx_putchar('0');
        return;
    }
    
    char buffer[12];
    int pos = 0;
    
    while (value > 0) {
        buffer[pos++] = '0' + (value % 10);
        value /= 10;
    }
    
    for (int i = pos - 1; i >= 0; i--) {
        gfx_putchar(buffer[i]);
    }
}

void gfx_print_binary(uint32_t value) {
    gfx_print("0b");
    for (int i = 31; i >= 0; i--) {
        gfx_putchar(((value >> i) & 1) ? '1' : '0');
    }
}



// Cursor and color management
void gfx_set_cursor(uint32_t x, uint32_t y) {
    g_display.cursor_x = x;
    g_display.cursor_y = y;
    if (backend_set_cursor) {
        backend_set_cursor(x, y);
    }
}

void gfx_set_cursor_position(uint32_t x, uint32_t y) {
    gfx_set_cursor(x, y);
}

void gfx_get_cursor(uint32_t* x, uint32_t* y) {
    if (x) *x = g_display.cursor_x;
    if (y) *y = g_display.cursor_y;
}

void gfx_set_colors(color_t fg, color_t bg) {
    g_display.fg_color = fg;
    g_display.bg_color = bg;
}

void gfx_clear_screen(void) {
    if (backend_clear) {
        backend_clear();
    }
    g_display.cursor_x = 0;
    g_display.cursor_y = 0;
}

void gfx_scroll_up(void) {
    if (backend_scroll) {
        backend_scroll();
    }
}

// Display information accessors
display_info_t* graphics_get_info(void) {
    SERIAL_LOG("graphics_get_info -> GFX_INFO: Current mode requested");
    SERIAL_LOG_DEC("graphics_get_info -> GFX_INFO: Mode value: ", g_display.mode);
    SERIAL_LOG_DEC("graphics_get_info -> GFX Framebuffer Address:", (uint32_t)(uintptr_t)g_display.framebuffer);
    return &g_display;
}

display_mode_t graphics_get_mode(void) {
    SERIAL_LOG("graphics_get_mode -> GFX_INFO: Current mode requested");
    SERIAL_LOG_DEC("graphics_get_mode -> GFX_INFO: Mode value: ", g_display.mode);
    SERIAL_LOG_DEC("graphics_get_mode -> GFX Framebuffer Address:", (uint32_t)g_display.framebuffer);
    return g_display.mode;
}

display_info_t* graphics_get_display_info(void) {
    return &g_display;
}


// Color conversion utilities
rgb_color_t color_to_rgb(color_t color) {
    static const rgb_color_t color_table[16] = {
        {0x00, 0x00, 0x00, 0xFF}, // BLACK
        {0x00, 0x00, 0xAA, 0xFF}, // BLUE
        {0x00, 0xAA, 0x00, 0xFF}, // GREEN
        {0x00, 0xAA, 0xAA, 0xFF}, // CYAN
        {0xAA, 0x00, 0x00, 0xFF}, // RED
        {0xAA, 0x00, 0xAA, 0xFF}, // MAGENTA
        {0xAA, 0x55, 0x00, 0xFF}, // BROWN
        {0xAA, 0xAA, 0xAA, 0xFF}, // LIGHT_GRAY
        {0x55, 0x55, 0x55, 0xFF}, // DARK_GRAY
        {0x55, 0x55, 0xFF, 0xFF}, // LIGHT_BLUE
        {0x55, 0xFF, 0x55, 0xFF}, // LIGHT_GREEN
        {0x55, 0xFF, 0xFF, 0xFF}, // LIGHT_CYAN
        {0xFF, 0x55, 0x55, 0xFF}, // LIGHT_RED
        {0xFF, 0x55, 0xFF, 0xFF}, // LIGHT_MAGENTA
        {0xFF, 0xFF, 0x55, 0xFF}, // YELLOW
        {0xFF, 0xFF, 0xFF, 0xFF}  // WHITE
    };
    
    if (color < 16) {
        return color_table[color];
    }
    return color_table[COLOR_WHITE];
}

color_t rgb_to_color(rgb_color_t rgb) {
    (void)rgb; // Suppress unused parameter warning
    return COLOR_WHITE; // Default fallback
}

uint32_t rgb_to_pixel(rgb_color_t color, uint32_t bpp, uint8_t red_pos, uint8_t green_pos, uint8_t blue_pos) {
    uint32_t pixel = 0;
    
    switch (bpp) {
        case 16: // 5-6-5 RGB (16-bit)  
            pixel = ((color.red >> 3) << 11) |
                    ((color.green >> 2) << 5) |
                    ((color.blue >> 3) << 0);
            break;
            
        case 24: // 8-8-8 RGB (24-bit)
            pixel = (color.red << red_pos) |
                    (color.green << green_pos) |
                    (color.blue << blue_pos);
            break;
            
        case 32: // 8-8-8-8 RGBA (32-bit)
        default:
            pixel = (color.red << 16) |
                    (color.green << 8) |
                    (color.blue << 0) |
                    (color.alpha << 24);
            break;
    }
    
    return pixel;
}

// Advanced graphics functions (stubs for now)
void gfx_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color) {
    extern void fb_draw_rect(int x, int y, int width, int height, uint32_t color);
    uint32_t packed = (color.alpha << 24) | (color.red << 16) | (color.green << 8) | color.blue;
    fb_draw_rect((int)x, (int)y, 1, 1, packed);
}

void gfx_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, rgb_color_t color) {
    // Bresenham's line algorithm
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        gfx_draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void gfx_draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color) {
    // Draw outline only
    gfx_draw_line(x, y, x + width - 1, y, color);                    // Top
    gfx_draw_line(x, y + height - 1, x + width - 1, y + height - 1, color); // Bottom
    gfx_draw_line(x, y, x, y + height - 1, color);                   // Left
    gfx_draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);  // Right
}

void gfx_draw_filled_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color) {
    extern void fb_draw_rect(int x, int y, int width, int height, uint32_t color);
    uint32_t packed = (color.alpha << 24) | (color.red << 16) | (color.green << 8) | color.blue;
    fb_draw_rect((int)x, (int)y, (int)width, (int)height, packed);
}

void gfx_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg, font_t* font) {
    extern void framebuffer_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg);
    if (font == NULL) {
        framebuffer_draw_char(x, y, c, fg, bg);
    }
}

void gfx_draw_string(uint32_t x, uint32_t y, const char* str, rgb_color_t fg, rgb_color_t bg, font_t* font) {
    if (!str) return;
    extern void framebuffer_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg);
    if (font == NULL) {
        uint32_t cx = x;
        for (const char* p = str; *p; p++) {
            framebuffer_draw_char(cx, y, *p, fg, bg);
            cx += 8; // Default 8x8 font width
        }
    }
}

// Backend initialization functions (implemented in their respective backend files)
extern void vga_text_init(void);
extern void framebuffer_init(void);
extern void framebuffer_test(void);
extern void serial_console_init(void);

// Runtime graphics mode control
bool graphics_force_framebuffer_mode(void) {
    gfx_print("Switching to framebuffer mode...\n");
    graphics_set_mode(DISPLAY_MODE_FRAMEBUFFER);
    gfx_print("Framebuffer mode activated\n");
    return true;
}

bool graphics_force_vga_mode(void) {
    gfx_print("Switching to VGA text mode...\n");
    graphics_set_mode(DISPLAY_MODE_TEXT_VGA);
    gfx_print("VGA text mode activated\n");
    return true;
}

// Font demonstration
void graphics_demo_fonts(void) {
    gfx_print("=== QARMA Font Demo ===\n");
    gfx_print("Current mode: ");
    gfx_print_decimal(g_display.mode);
    gfx_print("\n");
    gfx_print("Font system integration test\n");
    gfx_print("ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
    gfx_print("abcdefghijklmnopqrstuvwxyz\n");
    gfx_print("0123456789!@#$%^&*()\n");
    gfx_print("Font demo completed\n");
}

// Debug information
void graphics_debug_info(void) {
    gfx_print("=== Graphics Debug Info ===\n");
    gfx_print("Mode: ");
    gfx_print_decimal(g_display.mode);
    gfx_print("\nResolution: ");
    gfx_print_decimal(g_display.width);
    gfx_print("x");
    gfx_print_decimal(g_display.height);
    gfx_print("\nBPP: ");
    gfx_print_decimal(g_display.bpp);
    gfx_print("\nFramebuffer: 0x");
    gfx_print_hex((uint32_t)g_display.framebuffer);
    gfx_print("\n");
}
