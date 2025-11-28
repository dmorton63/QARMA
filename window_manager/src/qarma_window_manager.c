#include "qarma_window_manager.h"
#include "qarma_input_events.h"
#include "window_compositor.h"
#include "console_compositor.h"
#include "panic.h"
#include "graphics.h"
#include "input/mouse.h"
#include "config.h"
#include "desktop_toolbar.h"
#include "qarma_control.h"
#include "message_system.h"

// For SERIAL_LOG macro
extern void serial_debug(const char* msg);

QARMA_WINDOW_MANAGER qarma_window_manager;

void add_window(QARMA_WINDOW_MANAGER* mgr, QARMA_WIN_HANDLE* win, const char* caller) {
    // Skip gfx_printf during early boot - it might hang
    (void)caller;
    
    SERIAL_LOG("[WINMGR] add_window called\n");
    
    if (!mgr || !win) {
        SERIAL_LOG("[WINMGR] ERROR: manager or window is NULL\n");
        panic("add_window: manager or window is NULL");
        return;
    }
    
    SERIAL_LOG("[WINMGR] Checking vtable\n");
    if (!win->vtable) {
        SERIAL_LOG("[WINMGR] ERROR: window vtable is NULL\n");
        panic("add_window: window vtable is NULL");
        return;
    }

    SERIAL_LOG("[WINMGR] Checking traits\n");
    if ((win->type == QARMA_WIN_TYPE_SPLASH || (win->flags & QARMA_FLAG_FADE_OUT)) && !win->traits) {
        SERIAL_LOG("[WINMGR] ERROR: splash window missing traits\n");
        panic("add_window: splash window missing traits");
        return;
    }

    SERIAL_LOG("[WINMGR] Checking window count\n");
    if (mgr->count >= QARMA_MAX_WINDOWS) {
        SERIAL_LOG("[WINMGR] ERROR: window manager overflow\n");
        panic("add_window: window manager overflow");
        return;
    }
    
    SERIAL_LOG("[WINMGR] Adding window to array\n");
    mgr->windows[mgr->count++] = win;
    SERIAL_LOG("[WINMGR] Window added successfully\n");
}

static void update_all(QARMA_WINDOW_MANAGER* mgr, QARMA_TICK_CONTEXT* ctx) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        QARMA_WIN_HANDLE* win = mgr->windows[i];
        if (win && win->vtable && win->vtable->update) {
            win->vtable->update(win, ctx);
        }
    }
}

static void render_all(QARMA_WINDOW_MANAGER* mgr) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        QARMA_WIN_HANDLE* win = mgr->windows[i];
        if (win && (win->flags & QARMA_FLAG_VISIBLE) && win->vtable && win->vtable->render) {
            win->vtable->render(win);
        }
    }
}

static void destroy_all(QARMA_WINDOW_MANAGER* mgr) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        QARMA_WIN_HANDLE* win = mgr->windows[i];
        if (win && win->vtable && win->vtable->destroy) {
            win->vtable->destroy(win);
        }
        mgr->windows[i] = NULL;
    }
    mgr->count = 0;
}

static void remove_window(QARMA_WINDOW_MANAGER* mgr, uint32_t id) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        QARMA_WIN_HANDLE* win = mgr->windows[i];
        if (win && win->id == id) {
            if (win->vtable && win->vtable->destroy) {
                win->vtable->destroy(win);
            }
            for (uint32_t j = i; j < mgr->count - 1; j++) {
                mgr->windows[j] = mgr->windows[j + 1];
            }
            mgr->windows[--mgr->count] = NULL;
            break;
        }
    }
}

// Hit test: Find topmost window at given coordinates
// Windows are stored front-to-back, so we search in reverse order
static QARMA_WIN_HANDLE* hit_test(QARMA_WINDOW_MANAGER* mgr, int x, int y) {
    // Search from top to bottom (reverse order)
    for (int i = mgr->count - 1; i >= 0; i--) {
        QARMA_WIN_HANDLE* win = mgr->windows[i];
        if (!win || !(win->flags & QARMA_FLAG_VISIBLE)) {
            continue;
        }
        
        // Check if point is inside window bounds
        if (x >= win->x && x < win->x + win->size.width &&
            y >= win->y && y < win->y + win->size.height) {
            return win;
        }
    }
    
    return NULL;  // No window hit, desktop/background
}

// Helper: Convert QARMA_INPUT_EVENT to mouse_state_t for compositor
static void convert_event_to_mouse_state(QARMA_INPUT_EVENT* event, mouse_state_t* mouse) {
    mouse->x = event->data.mouse.x;
    mouse->y = event->data.mouse.y;
    mouse->dx = event->data.mouse.delta_x;
    mouse->dy = event->data.mouse.delta_y;
    
    // Convert button bitfield to individual flags
    uint8_t buttons = event->data.mouse.buttons;
    mouse->left_pressed = (buttons & 0x01) != 0;
    mouse->right_pressed = (buttons & 0x02) != 0;
    mouse->middle_pressed = (buttons & 0x04) != 0;
}

// Global mouse event handler - routes events to correct window
void qarma_window_manager_handle_mouse_event(QARMA_INPUT_EVENT* event) {
    if (!event) return;
    
    // Only handle mouse events
    if (event->type < QARMA_INPUT_EVENT_MOUSE_MOVE || 
        event->type > QARMA_INPUT_EVENT_MOUSE_LEAVE) {
        return;
    }
    
    // TODO: Toolbar disabled - needs redesign
    // Check if mouse is over toolbar first (toolbar is at bottom of screen)
    // extern uint32_t fb_height;
    // if (event->data.mouse.y >= (int32_t)(fb_height - 50)) {
    //     ... toolbar mouse handling code removed ...
    // }
    
    // Convert unified event to mouse_state_t for compositor
    mouse_state_t mouse_state;
    convert_event_to_mouse_state(event, &mouse_state);
    
    // Handle window compositor mouse interaction (dragging, close buttons, etc.)
    extern void compositor_handle_mouse(mouse_state_t* mouse);
    compositor_handle_mouse(&mouse_state);
    
    // Render windows on significant events (not on every mouse move)
    // Render when: dragging, clicking buttons, or scrolling
    bool should_render_all = false;
    if (event->type == QARMA_INPUT_EVENT_MOUSE_DOWN ||
        event->type == QARMA_INPUT_EVENT_MOUSE_UP ||
        event->type == QARMA_INPUT_EVENT_MOUSE_SCROLL) {
        should_render_all = true;
    }
    
    // Also render if actively dragging (compositor will have dragging_window set)
    extern window_compositor_t* get_compositor(void);
    window_compositor_t* comp = get_compositor();
    if (comp != NULL && comp->dragging_window != NULL) {
        // Throttle renders during dragging MORE aggressively - every 4th event
        // This prevents the compositor from blocking USB event processing
        static uint32_t drag_render_skip = 0;
        if (++drag_render_skip % 4 == 0) {
            should_render_all = true;
        }
    }
    
    if (should_render_all && comp != NULL) {
        extern void compositor_render_all(void);
        compositor_render_all();
    } else if (event->type == QARMA_INPUT_EVENT_MOUSE_MOVE && comp != NULL && comp->dragging_window == NULL) {
        // Aggressively throttle cursor-only rendering to prevent keyboard lag
        // Only render every 5th mouse move to ensure keyboard responsiveness
        static uint32_t cursor_render_skip = 0;
        if (++cursor_render_skip % 5 == 0) {
            extern void compositor_render_cursor_only(int x, int y);
            compositor_render_cursor_only(mouse_state.x, mouse_state.y);
        }
    }
    
    // Hit test to find target window in QARMA window manager
    QARMA_WIN_HANDLE* target = qarma_window_manager.hit_test(&qarma_window_manager, 
                                                               event->data.mouse.x, 
                                                               event->data.mouse.y);
    
    if (target) {
        // Set target for this event
        event->target = target;
        
        // Dispatch to window's controls
        if (target->flags & QARMA_FLAG_INTERACTIVE) {
            extern bool qarma_win_dispatch_event(QARMA_WIN_HANDLE* win, QARMA_INPUT_EVENT* event);
            qarma_win_dispatch_event(target, event);
        }
    } else {
        // Desktop/background handler could go here
        event->target = NULL;
    }
}

// Global keyboard event handler for console
void qarma_console_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data) {
    // LOG IMMEDIATELY - before ANY checks
    extern void serial_debug(const char* msg);
    serial_debug("[CONSOLE_HANDLER] Called\n");
    
    (void)user_data;
    
    if (!event || event->type != QARMA_INPUT_EVENT_KEY_DOWN) {
        serial_debug("[CONSOLE_HANDLER] Not KEY_DOWN\n");
        return;
    }
    
    uint8_t scancode = event->data.key.scancode;
    char character = event->data.key.character;
    
    // Ctrl+T - toggle console (ALWAYS handle this, even if console hidden)
    if (scancode == 0x14 && (event->data.key.modifiers & QARMA_MOD_CTRL)) {  // Ctrl+T
        serial_debug("[CONSOLE_HANDLER] Ctrl+T - toggle\n");
        console_compositor_toggle();
        event->handled = true;
        return;
    }
    
    // Only process other keys if console is visible AND has keyboard focus
    extern bool console_compositor_is_visible(void);
    extern void* keyboard_get_focus(void);
    extern struct compositor_window_t* console_compositor_get_window(void);
    
    if (!console_compositor_is_visible()) {
        // Console is hidden - don't handle any keys except Ctrl+T (already handled above)
        event->handled = false;  // Let other handlers try
        return;
    }
    
    void* current_focus = keyboard_get_focus();
    void* console_window = console_compositor_get_window();
    
    if (current_focus != console_window) {
        // Console is visible but doesn't have focus - let other handlers process
        event->handled = false;
        return;
    }
    
    // Console is visible and has focus - process the key
    serial_debug("[CONSOLE_HANDLER] Console has focus, processing key\n");
    console_compositor_handle_key(scancode, character);
    event->handled = true;  // Mark as handled to stop propagation
}

void qarma_window_manager_init() {
    qarma_window_manager.count = 0;
    qarma_window_manager.add_window = add_window;
    qarma_window_manager.remove_window = remove_window;
    qarma_window_manager.update_all = update_all;
    qarma_window_manager.render_all = render_all;
    qarma_window_manager.destroy_all = destroy_all;
    qarma_window_manager.hit_test = hit_test;
}

