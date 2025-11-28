#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "stdtools.h"
#include "framebuffer.h"

// Compositor for double-buffered rendering with page flipping
typedef struct {
    uint32_t* back_buffer;       // Back buffer for rendering
    uint32_t* front_buffer;      // Front buffer (actual framebuffer)
    uint32_t width;
    uint32_t height;
    uint32_t buffer_size;        // Size in bytes
    bool initialized;
    bool page_flip_supported;    // If true, use page flip; else memcpy
    
    // Cursor state
    bool cursor_visible;
    int cursor_x;
    int cursor_y;
    int cursor_width;
    int cursor_height;
} Compositor;

// Initialize compositor with double buffering
bool cursor_compositor_init(void);

// Shutdown and free resources
void compositor_shutdown(void);

// Begin a new frame (clear back buffer or copy background)
void compositor_begin_frame(void);

// Render all windows to back buffer
void compositor_render_windows(void);

// Draw cursor on back buffer
void compositor_draw_cursor(void);

// Flip buffers (page flip or memcpy)
void compositor_present(void);

// Complete frame rendering (calls render_windows + draw_cursor + present)
void cursor_compositor_render_frame(void);

// Cursor management
void cursor_compositor_set_cursor_position(int x, int y);
void compositor_set_cursor_visible(bool visible);
bool compositor_get_cursor_visible(void);

// Get compositor instance
Compositor* compositor_get(void);

#endif // COMPOSITOR_H
