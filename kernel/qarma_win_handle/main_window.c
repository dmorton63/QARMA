#include "main_window.h"
#include "core/memory.h"
#include "graphics/framebuffer.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_win_factory.h"
#include "core/memory/heap.h"
#include "gui/controls/close_button.h"
// #include "gui/controls/label.h"  // Legacy - disabled
// #include "gui/status_bar.h"  // Legacy - disabled
// #include "gui/console_window.h"  // Legacy - disabled
#include "quantum/quantum_register_example.h"
#include "config.h"

// Old console window (DISABLED - using compositor console instead)
// static ConsoleWindow* g_console_old = NULL;  // Legacy - disabled

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
    SERIAL_LOG("[MAIN_WIN] Close button callback invoked!\n");
    MainWindow* mw = (MainWindow*)userdata;
    mw->should_exit = true;
    SERIAL_LOG("[MAIN_WIN] should_exit set to true\n");
}

MainWindow* main_window_create(void) {
    MainWindow* mw = (MainWindow*)malloc(sizeof(MainWindow));
    if (!mw) return NULL;

    // Get framebuffer dimensions
    extern FramebufferInfo* fb_info;
    int screen_w = fb_info ? fb_info->width : 1024;
    int screen_h = fb_info ? fb_info->height : 768;

    // Define title bar height
    #define TITLE_BAR_HEIGHT 30

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
    win->size = (QARMA_DIMENSION){screen_w, TITLE_BAR_HEIGHT};
    win->alpha = 1.0f;
    win->title = "QARMA Desktop";
    win->background = (QARMA_COLOR){0, 0, 0, 255};
    win->vtable = &main_window_vtable;
    win->traits = mw;  // Store MainWindow pointer in traits
    
    // Old console system disabled - using compositor console instead
    // g_console_old = NULL;  // Legacy - disabled
    win->buffer_size = win->size;
    // Allocate pixel buffer for title bar only
    size_t buffer_bytes = screen_w * TITLE_BAR_HEIGHT * sizeof(uint32_t);
    win->pixel_buffer = (uint32_t*)heap_alloc(buffer_bytes);
    if (!win->pixel_buffer) {
        free(win);
        free(mw);
        return NULL;
    }
    
    // Clear buffer with dark background
    for (int i = 0; i < screen_w * TITLE_BAR_HEIGHT; i++) {
        win->pixel_buffer[i] = 0x2A2A2E;  // Dark gray title bar
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
    // Legacy label disabled - TODO: Use hardened label control
    // mw->title_label.base.visible = true;
    // mw->title_label.base.enabled = false;
    // qarma_win_add_control(win, &mw->title_label.base);

    // Create close button in top-right corner (20x20 button, 5px margin)
    int close_btn_size = 20;
    int close_btn_x = screen_w - close_btn_size - 5;
    int close_btn_y = 5;
    close_button_init(&mw->close_btn, close_btn_x, close_btn_y, close_btn_size);
    mw->close_btn.on_click = on_close_clicked;
    mw->close_btn.userdata = mw;
    mw->close_btn.base.visible = true;
    mw->close_btn.base.enabled = true;
    mw->close_btn.focused = false;  // Start unfocused - user must TAB to focus first
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
    int h = mw->win->size.height;  // This is now just TITLE_BAR_HEIGHT (30px)

    // Fill title bar with solid color
    #define TITLE_BG_COLOR 0x2A2A2E
    
    for (int i = 0; i < w * h; i++) {
        mw->win->pixel_buffer[i] = TITLE_BG_COLOR;
    }

    // Render all controls (title label, close button, etc.)
    qarma_win_render_controls(mw->win);
}

void main_window_handle_event(MainWindow* mw, QARMA_INPUT_EVENT* event) {
    if (!mw || !event) return;
    
    SERIAL_LOG("[MAIN_WIN] Event received, type=");
    extern void serial_debug_hex(uint32_t value);
    serial_debug_hex(event->type);
    SERIAL_LOG(" scancode=");
    serial_debug_hex(event->data.key.scancode);
    SERIAL_LOG("\n");
    
    // Old console disabled - using compositor console
    // If console is visible, it gets priority for events
    // if (g_console_old && g_console_old->visible) {  // Legacy - disabled
    //     console_window_handle_event(g_console_old, event);
    //     return;
    // }

    // Try dispatching to controls first
    if (qarma_win_dispatch_event(mw->win, event)) {
        SERIAL_LOG("[MAIN_WIN] Event handled by control\n");
        return;  // Control handled it
    }

    // Handle window-level keyboard shortcuts
    if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        SERIAL_LOG("[MAIN_WIN] Handling key down event\n");
        // Tab key - toggle focus on close button
        if (event->data.key.scancode == 0x0F) {  // Tab
            SERIAL_LOG("[MAIN_WIN] Tab pressed, current focus=");
            serial_debug_hex(mw->close_btn.focused);
            SERIAL_LOG(", toggling focus\n");
            close_button_set_focus(&mw->close_btn, !mw->close_btn.focused);
            SERIAL_LOG("[MAIN_WIN] New focus=");
            serial_debug_hex(mw->close_btn.focused);
            SERIAL_LOG("\n");
            // Re-render window to show focus change
            main_window_render(mw);
            // Blit to framebuffer
            extern FramebufferInfo* fb_info;
            uint32_t* fb = (uint32_t*)(uintptr_t)fb_info->virt_addr;
            uint32_t* win_buffer = mw->win->pixel_buffer;
            int win_w = mw->win->size.width;
            int win_h = mw->win->size.height;
            for (int y = 0; y < win_h; y++) {
                for (int x = 0; x < win_w; x++) {
                    if (x < (int)fb_info->width && y < (int)fb_info->height) {
                        fb[y * fb_info->width + x] = win_buffer[y * win_w + x];
                    }
                }
            }
        }
        // Enter key - activate close button if focused
        else if (event->data.key.scancode == 0x1C) {  // Enter
            SERIAL_LOG("[MAIN_WIN] Enter pressed\n");
            if (mw->close_btn.focused) {
                SERIAL_LOG("[MAIN_WIN] Close button focused, activating\n");
                close_button_activate(&mw->close_btn);
            } else {
                SERIAL_LOG("[MAIN_WIN] Close button not focused\n");
            }
        }
        // Escape key - quick exit
        else if (event->data.key.scancode == 0x01) {  // ESC
            SERIAL_LOG("[MAIN_WIN] ESC pressed, exiting\n");
            mw->should_exit = true;
        }
        // Ctrl+T - Toggle console (handled by compositor console handler now)
        else if (event->data.key.scancode == 0x14 && (event->data.key.modifiers & QARMA_MOD_CTRL)) {  // Ctrl+T
            // if (g_console_old) {  // Legacy - disabled
            //     console_window_set_visible(g_console_old, !g_console_old->visible);
            // }
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
