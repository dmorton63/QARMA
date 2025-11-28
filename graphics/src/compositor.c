#include "compositor.h"
#include "framebuffer.h"
#include "../../window_manager/headers/qarma_window_manager.h"
#include "memory/heap.h"
#include "config.h"
#include "string.h"

static Compositor g_compositor = {0};

// Simple cursor bitmap (8x12 arrow)
static const uint32_t cursor_bitmap[12][8] = {
    {0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0x00000000},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
    {0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}
};

bool cursor_compositor_init(void) {
    extern FramebufferInfo* fb_info;
    
    if (!fb_info || !fb_info->valid || fb_info->virt_addr == 0) {
        SERIAL_LOG("[COMPOSITOR] Error: Framebuffer not initialized\n");
        return false;
    }
    
    g_compositor.width = fb_info->width;
    g_compositor.height = fb_info->height;
    g_compositor.buffer_size = fb_info->width * fb_info->height * 4;  // 32bpp
    g_compositor.front_buffer = (uint32_t*)(uintptr_t)fb_info->virt_addr;
    
    // Allocate back buffer
    g_compositor.back_buffer = (uint32_t*)heap_alloc(g_compositor.buffer_size);
    if (!g_compositor.back_buffer) {
        SERIAL_LOG("[COMPOSITOR] Error: Failed to allocate back buffer\n");
        return false;
    }
    
    // For now, we'll use memcpy (page flipping requires hardware support)
    // TODO: Check if we can reprogram the display controller
    g_compositor.page_flip_supported = false;
    
    // Initialize cursor state
    g_compositor.cursor_visible = true;
    g_compositor.cursor_x = g_compositor.width / 2;
    g_compositor.cursor_y = g_compositor.height / 2;
    g_compositor.cursor_width = 8;
    g_compositor.cursor_height = 12;
    
    g_compositor.initialized = true;
    
    SERIAL_LOG("[COMPOSITOR] Initialized: ");
    SERIAL_LOG_DEC("", g_compositor.width);
    SERIAL_LOG("x");
    SERIAL_LOG_DEC("", g_compositor.height);
    SERIAL_LOG(", buffer size=");
    SERIAL_LOG_DEC("", g_compositor.buffer_size);
    SERIAL_LOG(" bytes\n");
    
    return true;
}

void compositor_shutdown(void) {
    if (g_compositor.back_buffer) {
        heap_free(g_compositor.back_buffer);
        g_compositor.back_buffer = NULL;
    }
    g_compositor.initialized = false;
}

// Draw diagonal rainbow gradient background
static void compositor_draw_gradient_background(void) {
    if (!g_compositor.initialized) return;
    
    uint32_t width = g_compositor.width;
    uint32_t height = g_compositor.height;
    
    // Rainbow colors: Red -> Orange -> Yellow -> Green -> Cyan -> Blue -> Purple
    // We'll create a diagonal gradient using the sum of x+y coordinates
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Diagonal coordinate (0 to width+height)
            uint32_t diag = x + y;
            uint32_t max_diag = width + height;
            
            // Map diagonal position to hue (0-360 degrees)
            uint32_t hue = (diag * 360) / max_diag;
            
            // Convert HSV to RGB (S=100%, V=80% for softer colors)
            uint8_t r, g, b;
            
            uint32_t h_sector = hue / 60;
            uint32_t h_remainder = (hue % 60) * 255 / 60;
            
            uint8_t v = 204;  // 80% brightness (255 * 0.8)
            uint8_t p = 0;    // Since S=100%, p=0
            uint8_t q = (v * (255 - h_remainder)) / 255;
            uint8_t t = (v * h_remainder) / 255;
            
            switch(h_sector) {
                case 0: r = v; g = t; b = p; break;
                case 1: r = q; g = v; b = p; break;
                case 2: r = p; g = v; b = t; break;
                case 3: r = p; g = q; b = v; break;
                case 4: r = t; g = p; b = v; break;
                case 5: r = v; g = p; b = q; break;
                default: r = v; g = 0; b = 0; break;
            }
            
            uint32_t pixel = 0xFF000000 | (r << 16) | (g << 8) | b;
            g_compositor.back_buffer[y * width + x] = pixel;
        }
    }
}

void compositor_begin_frame(void) {
    if (!g_compositor.initialized) return;
    
    // Draw gradient background instead of solid black
    compositor_draw_gradient_background();
}

void compositor_render_windows(void) {
    if (!g_compositor.initialized) return;
    
    extern QARMA_WINDOW_MANAGER qarma_window_manager;
    
    // Render all visible windows to back buffer
    for (uint32_t i = 0; i < qarma_window_manager.count; i++) {
        QARMA_WIN_HANDLE* win = qarma_window_manager.windows[i];
        
        if (!win || !(win->flags & QARMA_FLAG_VISIBLE)) {
            continue;
        }
        
        // Blit window pixel buffer to back buffer
        if (win->pixel_buffer) {
            for (int y = 0; y < win->size.height; y++) {
                for (int x = 0; x < win->size.width; x++) {
                    int screen_x = win->x + x;
                    int screen_y = win->y + y;
                    
                    // Bounds check
                    if (screen_x < 0 || screen_x >= (int)g_compositor.width ||
                        screen_y < 0 || screen_y >= (int)g_compositor.height) {
                        continue;
                    }
                    
                    uint32_t pixel = win->pixel_buffer[y * win->size.width + x];
                    
                    // Alpha blending (if needed)
                    uint8_t alpha = (pixel >> 24) & 0xFF;
                    if (alpha > 0) {
                        g_compositor.back_buffer[screen_y * g_compositor.width + screen_x] = pixel;
                    }
                }
            }
        }
    }
}

void compositor_draw_cursor(void) {
    if (!g_compositor.initialized || !g_compositor.cursor_visible) {
        return;
    }
    
    int cx = g_compositor.cursor_x;
    int cy = g_compositor.cursor_y;
    
    // Draw cursor bitmap
    for (int y = 0; y < g_compositor.cursor_height; y++) {
        for (int x = 0; x < g_compositor.cursor_width; x++) {
            int screen_x = cx + x;
            int screen_y = cy + y;
            
            // Bounds check
            if (screen_x < 0 || screen_x >= (int)g_compositor.width ||
                screen_y < 0 || screen_y >= (int)g_compositor.height) {
                continue;
            }
            
            uint32_t pixel = cursor_bitmap[y][x];
            
            // 0x00000000 = transparent, skip
            if (pixel != 0x00000000) {
                g_compositor.back_buffer[screen_y * g_compositor.width + screen_x] = pixel;
            }
        }
    }
}

void compositor_present(void) {
    if (!g_compositor.initialized) return;
    
    if (g_compositor.page_flip_supported) {
        // TODO: Page flip implementation
        // Would need to:
        // 1. Swap front/back buffer pointers
        // 2. Update display controller register to point to new front buffer
        // 3. Wait for VSync
        
        // For now, this path is not used
    } else {
        // Fallback: memcpy entire buffer
        memcpy(g_compositor.front_buffer, g_compositor.back_buffer, g_compositor.buffer_size);
    }
}

void cursor_compositor_render_frame(void) {
    if (!g_compositor.initialized) return;
    
    compositor_begin_frame();
    compositor_render_windows();
    compositor_draw_cursor();
    compositor_present();
}

void cursor_compositor_set_cursor_position(int x, int y) {
    g_compositor.cursor_x = x;
    g_compositor.cursor_y = y;
}

void compositor_set_cursor_visible(bool visible) {
    g_compositor.cursor_visible = visible;
}

bool compositor_get_cursor_visible(void) {
    return g_compositor.cursor_visible;
}

Compositor* compositor_get(void) {
    return &g_compositor;
}
