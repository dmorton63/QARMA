#include "qarma_window_manager.h"
#include "panic.h"
#include "graphics/graphics.h"
#include "config.h"

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

void qarma_window_manager_init() {
    qarma_window_manager.count = 0;
    qarma_window_manager.add_window = add_window;
    qarma_window_manager.remove_window = remove_window;
    qarma_window_manager.update_all = update_all;
    qarma_window_manager.render_all = render_all;
    qarma_window_manager.destroy_all = destroy_all;
}

