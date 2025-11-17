#include "main_window.h"
#include "core/memory.h"
#include "graphics/framebuffer.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_win_factory.h"
#include "core/memory/heap.h"
#include "gui/controls/close_button.h"
#include "gui/status_bar.h"

// Forward declarations for vtable
static void main_window_vtable_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx);
static void main_window_vtable_render(QARMA_WIN_HANDLE* self);
static void main_window_vtable_destroy(QARMA_WIN_HANDLE* self);

// Vtable for main window
static QARMA_WIN_VTABLE main_window_vtable = {
    .init = NULL,
    .update = main_window_vtable_update,
    .render = main_window_vtable_render,
    .destroy = main_window_vtable_destroy
};

// Vtable implementation
static void main_window_vtable_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx) {
    (void)self;
    (void)ctx;
    // Nothing to do - main window doesn't animate
}

static void main_window_vtable_render(QARMA_WIN_HANDLE* self) {
    MainWindow* mw = (MainWindow*)self->traits;
    if (mw) {
        main_window_render(mw);
    }
}

static void main_window_vtable_destroy(QARMA_WIN_HANDLE* self) {
    // Don't actually destroy through vtable - use main_window_destroy instead
    (void)self;
}

// Close button click callback
static void on_close_clicked(void* userdata) {
    MainWindow* mw = (MainWindow*)userdata;
    mw->should_exit = true;
}

MainWindow* main_window_create(void) {
    MainWindow* mw = (MainWindow*)malloc(sizeof(MainWindow));
    if (!mw) return NULL;

    // Get framebuffer dimensions
    extern FramebufferInfo* fb_info;
    int screen_w = fb_info ? fb_info->width : 1024;
    int screen_h = fb_info ? fb_info->height : 768;

    // Create QARMA window handle manually
    QARMA_WIN_HANDLE* win = (QARMA_WIN_HANDLE*)malloc(sizeof(QARMA_WIN_HANDLE));
    if (!win) {
        free(mw);
        return NULL;
    }
    
    win->id = qarma_generate_window_id();
    win->type = QARMA_WIN_TYPE_GENERIC;
    win->flags = 0;
    win->x = 0;
    win->y = 0;
    win->size = (QARMA_DIMENSION){screen_w, screen_h};
    win->alpha = 1.0f;
    win->title = "QARMA Desktop";
    win->background = (QARMA_COLOR){0, 0, 0, 255};
    win->vtable = &main_window_vtable;
    win->traits = mw;  // Store MainWindow pointer in traits
    win->buffer_size = win->size;
    // Allocate pixel buffer using heap_alloc for large allocation
    size_t buffer_bytes = screen_w * screen_h * sizeof(uint32_t);
    win->pixel_buffer = (uint32_t*)heap_alloc(buffer_bytes);
    if (!win->pixel_buffer) {
        free(win);
        free(mw);
        return NULL;
    }
    
    // Clear buffer
    for (int i = 0; i < screen_w * screen_h; i++) {
        win->pixel_buffer[i] = 0;
    }
    
    // Register with window manager
    qarma_window_manager.add_window(&qarma_window_manager, win, "Main Desktop");

    mw->win = win;
    mw->should_exit = false;

    // Create close button in top-right corner (20x20 button, 5px margin)
    int close_btn_size = 20;
    int close_btn_x = screen_w - close_btn_size - 5;
    int close_btn_y = 5;
    close_button_init(&mw->close_btn, close_btn_x, close_btn_y, close_btn_size);
    mw->close_btn.on_click = on_close_clicked;
    mw->close_btn.userdata = mw;
    
    // Give button initial focus so it's visible
    mw->close_btn.focused = true;

    return mw;
}

void main_window_update(MainWindow* mw) {
    if (!mw || !mw->win) return;
    // Nothing to update for now
}

void main_window_render(MainWindow* mw) {
    if (!mw || !mw->win || !mw->win->pixel_buffer) return;

    int w = mw->win->size.width;
    int h = mw->win->size.height;

    // Draw gradient background
    extern void draw_vertical_gradient(uint32_t* buffer, int buf_width, int buf_height, 
                                      uint32_t color_top, uint32_t color_bottom);
    
    // Nice blue-purple gradient
    uint32_t top_color = 0x003366;      // Dark blue
    uint32_t bottom_color = 0x660066;   // Dark purple
    
    draw_vertical_gradient(mw->win->pixel_buffer, w, h, top_color, bottom_color);

    // Render close button
    close_button_render(&mw->close_btn, mw->win->pixel_buffer, w, h);
}

void main_window_handle_event(MainWindow* mw, QARMA_INPUT_EVENT* event) {
    if (!mw || !event) return;

    if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        // Tab key - toggle focus on close button
        if (event->data.key.scancode == 0x0F) {  // Tab
            close_button_set_focus(&mw->close_btn, !mw->close_btn.focused);
        }
        // Enter key - activate close button if focused
        else if (event->data.key.scancode == 0x1C) {  // Enter
            if (mw->close_btn.focused) {
                close_button_activate(&mw->close_btn);
            }
        }
        // Escape key - quick exit
        else if (event->data.key.scancode == 0x01) {  // ESC
            mw->should_exit = true;
        }
    }
    // TODO: Handle mouse events for clicking close button
}

void main_window_destroy(MainWindow* mw) {
    if (!mw) return;
    
    if (mw->win) {
        // Remove from window manager
        qarma_window_manager.remove_window(&qarma_window_manager, mw->win->id);
        
        // Free pixel buffer
        if (mw->win->pixel_buffer) {
            heap_free(mw->win->pixel_buffer);
            mw->win->pixel_buffer = NULL;
        }
        
        free(mw->win);
        mw->win = NULL;
    }
    
    free(mw);
}

bool main_window_should_exit(MainWindow* mw) {
    return mw ? mw->should_exit : false;
}
