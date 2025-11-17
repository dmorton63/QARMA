/**
 * QARMA - Framebuffer Graphics Backend
 * Linear framebuffer support for VESA/GOP modes
 */
#include "framebuffer.h"
#include "graphics/graphics.h"
#include "font_data.h"
#include "core/io.h"
#include "core/boot_log.h"
#include "config.h"
#include "core/math.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "qarma_win_handle/qarma_win_handle.h"
#include "qarma_win_handle/qarma_window_manager.h"

// Debug functions now handled by config.h macros

// Forward declarations
void framebuffer_scroll(void);
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg);
void draw_scaled_text_centered(int cx, int y, const char* text, int scale, rgb_color_t fg, rgb_color_t bg);

// framebuffer.c

const rgb_color_t COLOR_ORBIT   = { .red = 0xFF, .green = 0xFF, .blue = 0xFF, .alpha = 0xFF };
const rgb_color_t COLOR_NUCLEUS = { .red = 0xFF, .green = 0xD7, .blue = 0x00, .alpha = 0xFF };
const rgb_color_t COLOR_DEEP_BLUE = { .red = 0x00, .green = 0x33, .blue = 0x66, .alpha = 0xFF };

static uint32_t* framebuffer_ptr = NULL;
// Expose a raw pointer used by fb_* helpers
uint32_t* fb_ptr = NULL;
// Backing store for composition
static uint32_t* backing_store = NULL;
static bool fb_dirty = false;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;
uint32_t fb_bpp = 0;
static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

// Framebuffer info structure instances
static FramebufferInfo fb_info_instance = {0};
FramebufferInfo* fb_info = &fb_info_instance;
FramebufferInfo* fbinfo = &fb_info_instance;

// Use vid_layer font system (8x8 fonts)
// External font functions from fonts.c
extern font_descriptor_t* get_font(font_type_t type);
extern void draw_bitmap_char(uint32_t fb_x, uint32_t fb_y, char c, rgb_color_t fg_color, rgb_color_t bg_color, font_descriptor_t* font);

// Use vid_layer 8x8 fonts instead of basic_font
#define FONT_WIDTH 8
#define FONT_HEIGHT 8

void framebuffer_init(void) {
    display_info_t* info = graphics_get_display_info();

    // Get framebuffer info from multiboot (stored during init)
    // The graphics system should have preserved the real framebuffer info
    if (info->framebuffer != NULL) {
    framebuffer_ptr = info->framebuffer;
    // Also set fb_ptr used by fb_* helpers
    fb_ptr = (uint32_t*)framebuffer_ptr;

        // Store the PIXEL dimensions from multiboot detection
        // Note: At this point info->width/height are still in PIXELS from multiboot
        uint32_t pixel_width = info->width;
        uint32_t pixel_height = info->height;

        // Use actual framebuffer parameters from multiboot
        fb_width = pixel_width;
        fb_height = pixel_height;
        fb_bpp = info->bpp;
        fb_pitch = info->pitch;
  
        // Boot log framebuffer detection
        BOOT_LOG("Framebuffer detected and configured\n");
        BOOT_LOG_HEX("FB Address: ", (uint32_t)framebuffer_ptr);
        BOOT_LOG_DEC("FB Resolution: ", fb_width);
        BOOT_LOG_DEC("x", fb_height);
        BOOT_LOG_DEC(" BPP: ", fb_bpp);
        BOOT_LOG_DEC(" Pitch: ", fb_pitch);
        SERIAL_LOG_MIN("FB_INIT: Framebuffer available\n");
    } else {
        // No framebuffer available
        framebuffer_ptr = NULL;

        SERIAL_LOG_MIN("FB_INIT: No framebuffer available!\n");
        return;
    }

    cursor_x = 0;
    cursor_y = 0;

    // Update display info for TEXT operations (characters, not pixels)
    info->width = fb_width / FONT_WIDTH;    // Characters per line
    info->height = fb_height / FONT_HEIGHT; // Lines per screen
    info->pitch = fb_pitch;
    info->bpp = fb_bpp;

    
      fbinfo->width = fb_width;
      fbinfo->height = fb_height;
      fbinfo->bpp = fb_bpp;
      fbinfo->pitch = fb_pitch;
      fbinfo->address = (uint8_t*)framebuffer_ptr;

    // Debug: Show text area dimensions
    BOOT_LOG_DEC("Text cols: ", info->width);
    BOOT_LOG_DEC("Text rows: ", info->height);
    // Keep framebuffer pointer for graphics operations
    info->cursor_x = 0;
    info->cursor_y = 0;

    // Allocate backing store for composition
    size_t pixels = fb_width * fb_height;
    size_t backing_store_size = pixels * sizeof(uint32_t);
    SERIAL_LOG_DEC("FB_INIT: Need ", backing_store_size);
    SERIAL_LOG_DEC(" bytes for ", pixels);
    SERIAL_LOG(" pixels\n");
    
    backing_store = (uint32_t*)heap_alloc(backing_store_size);
    SERIAL_LOG_DEC("FB_INIT: Backing store allocated at ", (uint32_t)(uintptr_t)backing_store);
    if(!backing_store) {
        SERIAL_LOG_MIN("FB_INIT: Backing store allocation failed!\n");
        SERIAL_LOG_DEC("FB_INIT: Requested ", backing_store_size);
        SERIAL_LOG(" bytes\n");
        __asm__ volatile("hlt");
        return;

    }
    if (backing_store) {
        // Initialize backing store with current framebuffer content
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                uint32_t offset = (y * fb_pitch + x * (fb_bpp / 8)) / 4;
                backing_store[offset] = framebuffer_ptr[offset];
            }
        }
    }

    // DEBUG: Force draw some test characters directly to verify framebuffer works
    framebuffer_draw_char(0, 0, 'T', (rgb_color_t){255,255,255,255}, (rgb_color_t){0,0,0,255});
    framebuffer_draw_char(8, 0, 'E', (rgb_color_t){255,255,255,255}, (rgb_color_t){0,0,0,255});
    framebuffer_draw_char(16, 0, 'S', (rgb_color_t){255,255,255,255}, (rgb_color_t){0,0,0,255});
    framebuffer_draw_char(24, 0, 'T', (rgb_color_t){255,255,255,255}, (rgb_color_t){0,0,0,255});
}

void framebuffer_putchar(char c) {
    display_info_t* info = graphics_get_display_info();
    if (!framebuffer_ptr || !info) return;

    static int call_count = 0;
    if (call_count < 10) {

        call_count++;
    }

    uint32_t max_cols = info->width;   // info->width is already in characters
    uint32_t max_rows = info->height;  // info->height is already in characters

    switch (c) {
        case '\n':
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= max_rows) {
                framebuffer_scroll();
                cursor_y = max_rows - 1;
            }
            break;

        case '\r':
            cursor_x = 0;
            break;

        case '\b':
            if (cursor_x > 0) {
                cursor_x--;
                rgb_color_t bg = color_to_rgb(info->bg_color);
                framebuffer_draw_char(cursor_x * FONT_WIDTH,
                                      cursor_y * FONT_HEIGHT,
                                      ' ', bg, bg);
            }
            break;

        case '\t':
            cursor_x = (cursor_x + 4) & ~3;
            if (cursor_x >= max_cols) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= max_rows) {
                    framebuffer_scroll();
                    cursor_y = max_rows - 1;
                }
            }
            break;

        default:
            if (c >= 32 && c <= 126) {
                rgb_color_t fg = {255, 255, 255, 255}; // white
                rgb_color_t bg = {0, 0, 0, 255};       // black

                framebuffer_draw_char(cursor_x * FONT_WIDTH,
                                      cursor_y * FONT_HEIGHT,
                                      c, fg, bg);

                cursor_x++;
                if (cursor_x >= max_cols) {
                    cursor_x = 0;
                    cursor_y++;
                    if (cursor_y >= max_rows) {
                        framebuffer_scroll();
                        cursor_y = max_rows - 1;
                    }
                }
            }
            break;
    }

    framebuffer_set_cursor(cursor_x, cursor_y);
    info->cursor_x = cursor_x;
    info->cursor_y = cursor_y;
}

void framebuffer_clear(void) {
    display_info_t* info = graphics_get_display_info();

    if (!framebuffer_ptr) return;

    rgb_color_t bg = color_to_rgb(info->bg_color);
    uint32_t pixel = rgb_to_pixel(bg, fb_bpp, 16, 8, 0); // R=16, G=8, B=0 for RGBA

    // Clear entire framebuffer
    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t offset = (y * fb_pitch) / 4 + x;
            framebuffer_ptr[offset] = pixel;
        }
    }

    cursor_x = 0;
    cursor_y = 0;
    info->cursor_x = 0;
    info->cursor_y = 0;
}

void fb_draw_rect_to_buffer(uint32_t* buffer, QARMA_DIMENSION size, int x, int y, QARMA_DIMENSION buffSize, QARMA_COLOR color) {
    for (int j = 0; j < buffSize.height; ++j) {
        for (int i = 0; i < buffSize.width; ++i) {
            int px = x + i;
            int py = y + j;

            if (IN_BOUNDS(px, size.width) && IN_BOUNDS(py, size.height)) {
                uint32_t offset = py * size.width + px;

                // Optional: add alpha blending here if needed
                buffer[offset] = color.r | (color.g << 8) | (color.b << 16) | (color.a << 24);
            }
        }
    }
}

void framebuffer_blit_window(QARMA_WIN_HANDLE* win) {
    for (int y = 0; y < win->size.height; ++y) {
        for (int x = 0; x < win->size.width; ++x) {
            int dst_x = win->x + x;
            int dst_y = win->y + y;
            if (IN_BOUNDS(dst_x, fb_width) && IN_BOUNDS(dst_y, fb_height)) {
                uint32_t src = win->pixel_buffer[y * win->size.width + x];
                framebuffer_blend_pixel(dst_x, dst_y, src);
            }
        }
    }
}


void fb_compose_all(void) {
    if (!framebuffer_ptr || !backing_store) return;

    // Step 1: Copy backing store to framebuffer
    memcpy(framebuffer_ptr, backing_store, fb_height * fb_pitch);

    // Step 2: Composite each window
    for (uint32_t i = 0; i < qarma_window_manager.count; ++i) {
        QARMA_WIN_HANDLE* win = qarma_window_manager.windows[i];
        if (!win || !(win->flags & QARMA_FLAG_VISIBLE)) continue;

        if (win->vtable && win->vtable->render) {
            win->vtable->render(win);  // Draw into win->pixel_buffer
        }

        framebuffer_blit_window(win);  // Composite onto framebuffer
    }

    fb_dirty = false;
}

void framebuffer_blend_pixel(int x, int y, uint32_t src) {
    if (!IN_BOUNDS(x, fb_width) || !IN_BOUNDS(y, fb_height)) return;

    uint32_t pitch_pixels = fb_pitch / 4;
    uint32_t offset = y * pitch_pixels + x;

    uint32_t dst = framebuffer_ptr[offset];

    // Extract RGBA from source
    uint8_t src_r = src & 0xFF;
    uint8_t src_g = (src >> 8) & 0xFF;
    uint8_t src_b = (src >> 16) & 0xFF;
    uint8_t src_a = (src >> 24) & 0xFF;

    // Extract RGB from destination
    uint8_t dst_r = dst & 0xFF;
    uint8_t dst_g = (dst >> 8) & 0xFF;
    uint8_t dst_b = (dst >> 16) & 0xFF;

    float alpha = src_a / 255.0f;

    // Blend
    uint8_t out_r = (uint8_t)(src_r * alpha + dst_r * (1 - alpha));
    uint8_t out_g = (uint8_t)(src_g * alpha + dst_g * (1 - alpha));
    uint8_t out_b = (uint8_t)(src_b * alpha + dst_b * (1 - alpha));

    framebuffer_ptr[offset] = out_r | (out_g << 8) | (out_b << 16) | (0xFF << 24);
}

void splash_clear(rgb_color_t bg) {
    uint32_t pixel = (bg.blue << 0) | (bg.green << 8) | (bg.red << 16) | (bg.alpha << 24);
    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t offset = (y * fb_pitch + x * (fb_bpp / 8)) / 4;
            if (backing_store) backing_store[offset] = pixel;
            framebuffer_ptr[offset] = pixel;
        }
    }
}

uint32_t box_top_y = 0;

void splash_box(uint32_t w, uint32_t h, rgb_color_t color) {
    uint32_t x0 = (fb_width - w) / 2;
    uint32_t y0 = (fb_height - h) / 2;
    uint32_t box_x0 = (fb_width - w) / 2;
    uint32_t box_y0 = (fb_height - h) / 2;

    uint32_t atom_cx = box_x0 + w / 2;
    uint32_t atom_cy = box_y0 + h / 2;

    box_top_y = y0;
    uint32_t pixel = (color.blue << 0) | (color.green << 8) | (color.red << 16) | (color.alpha << 24);
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t px = x0 + x;
            uint32_t py = y0 + y;
            uint32_t offset = (py * fb_pitch + px * (fb_bpp / 8)) / 4;
            if (backing_store) backing_store[offset] = pixel;
            framebuffer_ptr[offset] = pixel;
        }
    }
        draw_atom(atom_cx, atom_cy);
}

void splash_title(const char *text, rgb_color_t fg, rgb_color_t bg) {
    uint32_t y0 = box_top_y + 10;  // 10px below box top
    draw_scaled_text_centered(fb_width / 2, y0, text, 2, fg, bg);
}


void framebuffer_set_cursor(uint32_t x, uint32_t y)
{
    display_info_t* info = graphics_get_display_info();

    if (x < info->width && y < info->height) {
        cursor_x = x;
        cursor_y = y;
        info->cursor_x = x;
        info->cursor_y = y;
    }
}

void framebuffer_scroll(void) {
    display_info_t* info = graphics_get_display_info();

    if (!framebuffer_ptr) return;

    uint32_t line_height = FONT_HEIGHT;
    (void)fb_bpp; // Suppress unused warning

    // Move all lines up by one character line
    for (uint32_t y = 0; y < fb_height - line_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t src_offset = ((y + line_height) * fb_pitch) / 4 + x;
            uint32_t dst_offset = (y * fb_pitch) / 4 + x;
            framebuffer_ptr[dst_offset] = framebuffer_ptr[src_offset];
        }
    }

    // Clear the last line
    rgb_color_t bg = color_to_rgb(info->bg_color);
    uint32_t pixel = rgb_to_pixel(bg, fb_bpp, 16, 8, 0);

    for (uint32_t y = fb_height - line_height; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t offset = (y * fb_pitch) / 4 + x;
            framebuffer_ptr[offset] = pixel;
        }
    }
    if (cursor_x * FONT_WIDTH >= fb_width) {
    cursor_x = 0;
    cursor_y++;
    }

    info->cursor_x = cursor_x;
    info->cursor_y = cursor_y;
}

// Framebuffer-specific drawing functions
void framebuffer_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color) {
    if (!framebuffer_ptr || x >= fb_width || y >= fb_height) return;

    // Convert rgb_color_t to packed 32-bit pixel (BGRA format)
    uint32_t pixel = (color.blue << 0) | (color.green << 8) | (color.red << 16) | (color.alpha << 24);

    // Calculate offset in bytes, then divide by 4 to index into uint32_t*
    uint32_t offset = (y * fb_pitch + x * (fb_bpp / 8)) / 4;

    framebuffer_ptr[offset] = pixel;
}

extern const uint8_t vga_font[128][8];

void framebuffer_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg) {
    if (!framebuffer_ptr || x >= fb_width || y >= fb_height) return;
    const uint8_t* glyph = vga_font[(uint8_t)c];
    uint32_t fg_pixel = (fg.blue << 0) | (fg.green << 8) | (fg.red << 16) | (fg.alpha << 24);
    uint32_t bg_pixel = (bg.blue << 0) | (bg.green << 8) | (bg.red << 16) | (bg.alpha << 24);

    for (uint32_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        //SERIAL_LOG_HEX(" 0x", glyph[row]);
        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            uint32_t pixel_x = x + col;
            uint32_t pixel_y = y + row;
            if (pixel_x < fb_width && pixel_y < fb_height) {
                uint32_t offset = (pixel_y * fb_pitch + pixel_x * (fb_bpp / 8)) / 4;
                framebuffer_ptr[offset] = (bits & (1 << col)) ? fg_pixel : bg_pixel;
            }
        }
    }
}

void fb_draw_text(uint32_t x, uint32_t y, const char *text, rgb_color_t color) {
    while (*text) {
        framebuffer_draw_char(x, y, *text, color, (rgb_color_t){0, 0, 0, 0});
        x += 8;  // Advance by character width
        text++;
    }
}

void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text,
                          rgb_color_t fg, rgb_color_t bg) {
    while (*text) {
        framebuffer_draw_char(x, y, *text, fg, bg);
        x += 8;
        text++;
    }
}
// VESA/GOP detection and initialization (placeholder)
bool framebuffer_detect_vesa(void) {
    // TODO: Implement VESA mode detection
    // This would involve calling VESA BIOS functions
    return false;
}

bool framebuffer_detect_gop(void) {
    // TODO: Implement UEFI GOP detection
    // This would involve UEFI protocol calls
    return false;
}

void framebuffer_set_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    // TODO: Implement mode setting via VESA/GOP
    fb_width = width;
    fb_height = height;
    fb_bpp = bpp;
    fb_pitch = width * (bpp / 8);

    display_info_t* info = graphics_get_display_info();
    info->width = width / FONT_WIDTH;
    info->height = height / FONT_HEIGHT;
    info->pitch = fb_pitch;
    info->bpp = bpp;
}

// Test function to verify framebuffer is accessible
void framebuffer_test(void) {
    if (!framebuffer_ptr) {
        SERIAL_LOG("FB_TEST: No framebuffer available!\n");
        return;
    }


    // Draw some test pixels to verify framebuffer works
    rgb_color_t red = {0xFF, 0x00, 0x00, 0xFF};
    rgb_color_t green = {0x00, 0xFF, 0x00, 0xFF};
    rgb_color_t blue = {0x00, 0x00, 0xFF, 0xFF};
    for (uint32_t y = 0; y < 100; y++) {
        for (uint32_t x = 0; x < 100; x++) {
            framebuffer_draw_pixel(x, y, red); // Magenta
        }
    }
        // Draw a few colored pixels in top-left corner
    framebuffer_draw_pixel(0, 0, red);
    framebuffer_draw_pixel(1, 0, green);
    framebuffer_draw_pixel(2, 0, blue);
    framebuffer_draw_pixel(0, 1, green);
    framebuffer_draw_pixel(1, 1, blue);
    framebuffer_draw_pixel(2, 1, red);
}

// Compose the screen from backing store + windows
void fb_compose(void) {
    if (!framebuffer_ptr) return;
    if (!fb_dirty) return;

    
extern QARMA_WINDOW_MANAGER qarma_window_manager;

    if (backing_store) {
        // Copy backing store to framebuffer
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                uint32_t offset = (y * fb_pitch + x * (fb_bpp / 8)) / 4;
                framebuffer_ptr[offset] = backing_store[offset];
            }
        }

        // Render windows onto the framebuffer
        for (uint32_t i = 0; i < qarma_window_manager.count; ++i) {
            QARMA_WIN_HANDLE *win = qarma_window_manager.windows[i];
            if (win && (win->flags & QARMA_FLAG_VISIBLE) && win->vtable && win->vtable->render) {
                if (win->vtable->render) {
                    win->vtable->render(win);  // render into win->pixel_buffer
                }
                framebuffer_blit_window(win);  // composite onto framebuffer
            }
        }
    }   

    fb_dirty = false;
}

void fb_mark_dirty(void) {
    fb_dirty = true;
}

// Simple rectangle drawing functions for popup support

// static int alpha_count = 0;
// static int solid_count = 0;

void fb_draw_rect(int x, int y, int width, int height, uint32_t color) {
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int px = x + i;
            int py = y + j;
            if (IN_BOUNDS(px,fb_width) && IN_BOUNDS(py,fb_height)) {
                uint32_t offset = (py * fb_pitch + px * (fb_bpp / 8)) / 4;
                fb_ptr[offset] = color;
            }
        }
    }
}


void fb_draw_rect_alpha(int x, int y, int width, int height, QARMA_COLOR color) {
    uint32_t pitch_pixels = fb_pitch / 4;

    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
            int px = x + i;
            int py = y + j;

            if (IN_BOUNDS(px, fb_width) && IN_BOUNDS(py, fb_height)) {
                uint32_t offset = py * pitch_pixels + px;
                uint32_t dst = fb_ptr[offset];

                // Extract destination RGB
                uint8_t dst_r = dst & 0xFF;
                uint8_t dst_g = (dst >> 8) & 0xFF;
                uint8_t dst_b = (dst >> 16) & 0xFF;

                // Blend
                float alpha = color.a / 255.0f;
                uint8_t out_r = (uint8_t)(color.r * alpha + dst_r * (1 - alpha));
                uint8_t out_g = (uint8_t)(color.g * alpha + dst_g * (1 - alpha));
                uint8_t out_b = (uint8_t)(color.b * alpha + dst_b * (1 - alpha));

                fb_ptr[offset] = out_r | (out_g << 8) | (out_b << 16) | (0xFF << 24);
            }
        }
    }
}

void fb_draw_rect_outline(int x, int y, int width, int height, uint32_t color) {
    // Top and bottom
    for (int i = 0; i < width; ++i) {
        if (IN_BOUNDS(x + i, (int)fb_width)) {
            if (IN_BOUNDS(y, (int)fb_height))
                fb_ptr[(y * fb_pitch + (x + i) * (fb_bpp / 8)) / 4] = color;
            if (IN_BOUNDS(y + height - 1, (int)fb_height))
                fb_ptr[((y + height - 1) * (int)fb_pitch + (x + i) * (fb_bpp / 8)) / 4] = color;
        }
    }

    // Left and right
    for (int j = 0; j < height; ++j) {
        if (IN_BOUNDS(y + j, fb_height)) {
            if (IN_BOUNDS(x, fb_width))
                fb_ptr[((y + j) * fb_pitch + x * (fb_bpp / 8)) / 4] = color;
            if (IN_BOUNDS(x + width - 1, fb_width))
                fb_ptr[((y + j) * fb_pitch + (x + width - 1) * (fb_bpp / 8)) / 4] = color;
        }
    }
}

uint32_t fb_get_pixel(int x, int y) {
    if (IN_BOUNDS(x, fb_width) && IN_BOUNDS(y, fb_height)) {
        uint32_t offset = (y * fb_pitch + x * (fb_bpp / 8)) / 4;
        return fb_ptr[offset];
    }
    return 0; // Default or error color
}


void draw_circle(int cx, int cy, int radius, rgb_color_t color, rgb_color_t bg) {
    //int r2 = radius * radius;
    int margin = 1; // how far beyond the radius to sample

    for (int y = -radius - margin; y <= radius + margin; y++) {
        for (int x = -radius - margin; x <= radius + margin; x++) {
            int dx = x;
            int dy = y;
            float dist = sqrtf(dx * dx + dy * dy);
            float alpha = 1.0f - fabsf(dist - radius); // fade near edge

            if (alpha > 0.0f && alpha <= 1.0f) {
                rgb_color_t blended = blend_color(color, bg, alpha);
                framebuffer_draw_pixel(cx + dx, cy + dy, blended);
            }
        }
    }
}

rgb_color_t blend_color(rgb_color_t fg, rgb_color_t bg, float alpha) {
    rgb_color_t out;
    out.red   = (uint8_t)(fg.red   * alpha + bg.red   * (1.0f - alpha));
    out.green = (uint8_t)(fg.green * alpha + bg.green * (1.0f - alpha));
    out.blue  = (uint8_t)(fg.blue  * alpha + bg.blue  * (1.0f - alpha));
    out.alpha = 0xFF;
    return out;
}

void draw_ellipse(int cx, int cy, int rx, int ry, float angle, rgb_color_t color) {
    int last_px = -9999, last_py = -9999;

    for (float theta = 0.0f; theta < QARMA_TAU; theta += 0.001f) {
        float x = rx * cosf(theta);
        float y = ry * sinf(theta);

        float xr = x * cosf(angle) - y * sinf(angle);
        float yr = x * sinf(angle) + y * cosf(angle);

        int px = (int)(cx + xr + 0.5f);
        int py = (int)(cy + yr + 0.5f);

        if (px != last_px || py != last_py) {
            framebuffer_draw_pixel(px, py, color);
            last_px = px;
            last_py = py;
        }
    }
}

void draw_atom(int cx, int cy) {
    // rgb_color_t orbit_color = { .red = 0xFF, .green = 0xFF, .blue = 0xFF, .alpha = 0xFF };
    // rgb_color_t nucleus_color = { .red = 0xFF, .green = 0xD7, .blue = 0x00, .alpha = 0xFF };
    // rgb_color_t deep_blue = { .red = 0x00, .green = 0x33, .blue = 0x66 };
    draw_circle(cx, cy, 10, COLOR_NUCLEUS, COLOR_NUCLEUS); // nucleus
    draw_ellipse(cx, cy, 60, 30, 0.0f, COLOR_ORBIT);
    draw_ellipse(cx, cy, 60, 30, 1.0f, COLOR_ORBIT);
    draw_ellipse(cx, cy, 60, 30, 2.0f, COLOR_ORBIT);
}



void draw_line(int x0, int y0, int x1, int y1, rgb_color_t color) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        framebuffer_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_scaled_char(int x, int y, char c, int scale, rgb_color_t fg, rgb_color_t bg)
{
    for (int i = 0; i < 8; i++) {
        uint8_t row = vga_font[(uint8_t)c][i];
        for (int j = 0; j < 8; j++) {
            rgb_color_t color = (row & (1 << j)) ? fg : bg;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    framebuffer_draw_pixel(x + j * scale + sx, y + i * scale + sy, color);
                }
            }
        }
    }
}

void draw_scaled_text_centered(int cx, int y, const char* text, int scale, rgb_color_t fg, rgb_color_t bg) {
    int len = strlen(text);  // Just the number of characters
    int total_width = len * 8 * scale;
    int x0 = cx - total_width / 2;

    for (int i = 0; i < len; i++) {
        draw_scaled_char(x0 + i * 8 * scale, y, text[i], scale, fg, bg);
    }
}
