#include "qarma_splash_window.h"
#include "graphics/framebuffer.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "graphics/png_decoder.h"

// Extended splash glyph
typedef struct QARMA_SPLASH_HANDLE {
    QARMA_WIN_HANDLE base;
    float fade_speed;
    png_image_t* splash_image;
} QARMA_SPLASH_HANDLE;

// Forward declarations
static void splash_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx);
static void splash_render(QARMA_WIN_HANDLE* self);
static void splash_destroy(QARMA_WIN_HANDLE* self);

// Behavior scroll
static QARMA_WIN_VTABLE splash_vtable = {
    .init = NULL,
    .update = splash_update,
    .render = splash_render,
    .destroy = splash_destroy
};

// Glyph summoner
QARMA_SPLASH_HANDLE* splash_window_create(const char* title, uint32_t flags) {
    QARMA_SPLASH_HANDLE* splash = malloc(sizeof(QARMA_SPLASH_HANDLE));
    if (!splash) return NULL;

    QARMA_WIN_HANDLE* win = &splash->base;
    win->id = qarma_generate_window_id();
    win->type = QARMA_WIN_TYPE_SPLASH;
    win->flags = flags;
    win->x = 30;
    win->y = 10;
    win->size = (QARMA_DIMENSION){640, 480};
    win->alpha = 1.0f;
    win->title = title;
    win->background = (QARMA_COLOR){0, 0, 0, 255};
    win->vtable = &splash_vtable;
    win->traits = splash;
    win->buffer_size = win->size;
    
    // Use heap_alloc for large pixel buffer (640x480x4 = ~1.2MB, too big for malloc)
    size_t buffer_bytes = win->buffer_size.width * win->buffer_size.height * sizeof(uint32_t);
    win->pixel_buffer = (uint32_t*)heap_alloc(buffer_bytes);
    if (!win->pixel_buffer) {
        free(splash);
        return NULL;
    }
    memset(win->pixel_buffer, 0, buffer_bytes);

    splash->fade_speed = 0.5f;  // Slower fade for better visibility
    splash->splash_image = load_splash_image();
    
    qarma_window_manager.add_window(&qarma_window_manager, win,"Splash Window");
    return splash;
}

// Fade logic
static void splash_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx) {
    QARMA_SPLASH_HANDLE* splash = (QARMA_SPLASH_HANDLE*) self;
    int decay = (int)(ctx->delta_time * 255 * splash->fade_speed);
    uint8_t a = self->background.a;

    if (a > 0) {
        self->background.a = (a > decay) ? a - decay : 0;
        self->dirty = true;
    } else {
        self->vtable->destroy(self);
    }
}

// Render logic
static void splash_render(QARMA_WIN_HANDLE* self) {
    QARMA_SPLASH_HANDLE* splash = (QARMA_SPLASH_HANDLE*)self;
    
    if (splash->splash_image && splash->splash_image->pixels) {
        // Draw the PNG image (simplified version)
        uint32_t img_w = splash->splash_image->width;
        uint32_t img_h = splash->splash_image->height;
        uint32_t win_w = self->buffer_size.width;
        uint32_t win_h = self->buffer_size.height;
        
        // Clear window first
        memset(self->pixel_buffer, 0, win_w * win_h * sizeof(uint32_t));
        
        // Center the image in the window
        int32_t offset_x = (int32_t)(win_w - img_w) / 2;
        int32_t offset_y = (int32_t)(win_h - img_h) / 2;
        
        // Draw image pixels (simplified - no alpha blending for now)
        for (uint32_t y = 0; y < img_h && y + offset_y < win_h; y++) {
            for (uint32_t x = 0; x < img_w && x + offset_x < win_w; x++) {
                int32_t dst_x = x + offset_x;
                int32_t dst_y = y + offset_y;
                
                if (dst_x >= 0 && dst_y >= 0 && dst_x < (int32_t)win_w && dst_y < (int32_t)win_h) {
                    uint32_t src_pixel = splash->splash_image->pixels[y * img_w + x];
                    uint32_t dst_offset = dst_y * win_w + dst_x;
                    self->pixel_buffer[dst_offset] = src_pixel;
                }
            }
        }
    } else {
        // Fallback to colored rectangle if image fails - make it obvious
        QARMA_COLOR bright_red = {255, 0, 0, 255}; // Bright red to be obvious
        fb_draw_rect_to_buffer(
            self->pixel_buffer,
            self->buffer_size,
            0, 0,
            self->buffer_size,
            bright_red
        );
    }
}

// Destruction ritual
static void splash_destroy(QARMA_WIN_HANDLE* self) {
    QARMA_SPLASH_HANDLE* splash = (QARMA_SPLASH_HANDLE*)self;
    
    qarma_window_manager.remove_window(&qarma_window_manager, self->id);

    if (self->pixel_buffer) {
        heap_free(self->pixel_buffer);
        self->pixel_buffer = NULL;
    }
    
    // Free the PNG image
    if (splash->splash_image) {
        png_free(splash->splash_image);
        splash->splash_image = NULL;
    }

    free(self);
    fb_mark_dirty();
}

