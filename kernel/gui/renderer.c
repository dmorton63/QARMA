#include "renderer.h"

// External VGA font data (8x8 bitmap font)
extern const uint8_t vga_font[128][8];

// ============================================================================
// GUI Rendering Utilities Implementation
// ============================================================================

void draw_filled_rect(uint32_t* buffer, int buf_width, int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < buf_width && py >= 0) {
                buffer[py * buf_width + px] = color;
            }
        }
    }
}

void draw_rect_border(uint32_t* buffer, int buf_width, int x, int y, int w, int h, uint32_t color, int thickness) {
    // Top and bottom
    for (int i = 0; i < w; i++) {
        for (int t = 0; t < thickness; t++) {
            if (x + i >= 0 && x + i < buf_width) {
                if (y + t >= 0) buffer[(y + t) * buf_width + (x + i)] = color;
                if (y + h - 1 - t >= 0) buffer[(y + h - 1 - t) * buf_width + (x + i)] = color;
            }
        }
    }
    // Left and right
    for (int i = 0; i < h; i++) {
        for (int t = 0; t < thickness; t++) {
            if (y + i >= 0) {
                if (x + t >= 0 && x + t < buf_width) buffer[(y + i) * buf_width + (x + t)] = color;
                if (x + w - 1 - t >= 0 && x + w - 1 - t < buf_width) buffer[(y + i) * buf_width + (x + w - 1 - t)] = color;
            }
        }
    }
}

void draw_char_to_buffer(uint32_t* buffer, int buf_width, int x, int y, char c, uint32_t color) {
    // Use VGA font data for proper character rendering
    if (c < 0 || c >= 128) c = '?';
    
    const uint8_t* glyph = vga_font[(uint8_t)c];
    
    for (int dy = 0; dy < 8; dy++) {
        uint8_t row_bits = glyph[dy];
        for (int dx = 0; dx < 8; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < buf_width && py >= 0) {
                // Check if this pixel should be drawn (bit set in font data)
                // Read bits from right to left (LSB to MSB)
                if (row_bits & (1 << dx)) {
                    buffer[py * buf_width + px] = color;
                }
            }
        }
    }
}

void draw_string_to_buffer(uint32_t* buffer, int buf_width, int x, int y, const char* str, uint32_t color) {
    int cursor_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char_to_buffer(buffer, buf_width, cursor_x, y, str[i], color);
        cursor_x += 8;
    }
}

void draw_vertical_gradient(uint32_t* buffer, int buf_width, int buf_height, uint32_t color_top, uint32_t color_bottom) {
    // Extract RGB components
    uint8_t r_top = (color_top >> 16) & 0xFF;
    uint8_t g_top = (color_top >> 8) & 0xFF;
    uint8_t b_top = color_top & 0xFF;
    
    uint8_t r_bottom = (color_bottom >> 16) & 0xFF;
    uint8_t g_bottom = (color_bottom >> 8) & 0xFF;
    uint8_t b_bottom = color_bottom & 0xFF;
    
    // Draw gradient line by line
    for (int y = 0; y < buf_height; y++) {
        // Calculate interpolation factor (0.0 at top, 1.0 at bottom)
        int t = (y * 256) / buf_height;  // Fixed-point: 0-256
        
        // Interpolate each color component
        uint8_t r = r_top + ((r_bottom - r_top) * t) / 256;
        uint8_t g = g_top + ((g_bottom - g_top) * t) / 256;
        uint8_t b = b_top + ((b_bottom - b_top) * t) / 256;
        
        uint32_t color = (r << 16) | (g << 8) | b;
        
        // Draw horizontal line
        for (int x = 0; x < buf_width; x++) {
            buffer[y * buf_width + x] = color;
        }
    }
}
