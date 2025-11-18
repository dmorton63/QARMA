#include "main_window.h"
#include "core/memory.h"
#include "graphics/framebuffer.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_win_factory.h"
#include "core/memory/heap.h"
#include "gui/controls/close_button.h"
#include "gui/controls/label.h"
#include "gui/status_bar.h"
#include "quantum/quantum_register_example.h"

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

    // Initialize controls array
    win->control_count = 0;
    for (int i = 0; i < QARMA_MAX_CONTROLS_PER_WINDOW; i++) {
        win->controls[i] = NULL;
    }

    // Create title bar label (10px from left, 8px from top)
    label_init(&mw->title_label, 10, 8, win->title, 0xE0E0E0);
    mw->title_label.base.visible = true;
    mw->title_label.base.enabled = false;  // Title is not interactive
    qarma_win_add_control(win, &mw->title_label.base);

    // Create close button in top-right corner (20x20 button, 5px margin)
    int close_btn_size = 20;
    int close_btn_x = screen_w - close_btn_size - 5;
    int close_btn_y = 5;
    close_button_init(&mw->close_btn, close_btn_x, close_btn_y, close_btn_size);
    mw->close_btn.on_click = on_close_clicked;
    mw->close_btn.userdata = mw;
    mw->close_btn.base.visible = true;
    mw->close_btn.base.enabled = true;
    mw->close_btn.focused = true;  // Give initial focus
    qarma_win_add_control(win, &mw->close_btn.base);

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

    // Draw title bar background
    #define TITLE_BAR_HEIGHT 30
    #define TITLE_BG_COLOR 0x2A2A2E
    
    extern void draw_filled_rect(uint32_t* buffer, int buf_width, int x, int y, int width, int height, uint32_t color);
    draw_filled_rect(mw->win->pixel_buffer, w, 0, 0, w, TITLE_BAR_HEIGHT, TITLE_BG_COLOR);

    // Render all controls (title label, close button, etc.)
    qarma_win_render_controls(mw->win);
    
    // Draw help text at bottom of screen
    extern void draw_string_to_buffer(uint32_t* buffer, int buf_width, int x, int y, 
                                      const char* str, uint32_t color);
    const char* help_text = "Press Q: Quantum Examples | ESC: Exit";
    int text_y = h - 20;
    int text_x = 10;
    draw_string_to_buffer(mw->win->pixel_buffer, w, text_x, text_y, help_text, 0xCCCCCC);
}

void main_window_handle_event(MainWindow* mw, QARMA_INPUT_EVENT* event) {
    if (!mw || !event) return;

    // Try dispatching to controls first
    if (qarma_win_dispatch_event(mw->win, event)) {
        return;  // Control handled it
    }

    // Handle window-level keyboard shortcuts
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
        // Q key - Run quantum register examples
        else if (event->data.key.scancode == 0x10) {  // Q
            // Clear the window content first
            for (int i = 0; i < mw->win->size.width * mw->win->size.height; i++) {
                mw->win->pixel_buffer[i] = 0x000000;  // Black background
            }
            
            // Run examples (output goes to GFX_LOG which draws to screen)
            quantum_register_run_examples();
            
            // Wait for user to read output (about 5 seconds)
            extern void sleep_ms(uint32_t ms);
            sleep_ms(5000);
            
            // Window will redraw on next render cycle
        }
    }
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
