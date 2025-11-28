#include "qarma_win_handle.h"
#include "gui/control_base.h"

static uint32_t next_window_id = 1;

uint32_t qarma_generate_window_id(void) {
    return next_window_id++;
}

void qarma_win_assign_vtable(QARMA_WIN_HANDLE* win, QARMA_WIN_VTABLE* vtable) {
    win->vtable = vtable;
}

// ============================================================================
// Control Management
// ============================================================================

bool qarma_win_add_control(QARMA_WIN_HANDLE* win, struct ControlBase* control) {
    if (!win || !control) return false;
    if (win->control_count >= QARMA_MAX_CONTROLS_PER_WINDOW) return false;
    
    win->controls[win->control_count++] = control;
    win->dirty = true;
    return true;
}

bool qarma_win_remove_control(QARMA_WIN_HANDLE* win, struct ControlBase* control) {
    if (!win || !control) return false;
    
    for (int i = 0; i < win->control_count; i++) {
        if (win->controls[i] == control) {
            // Shift remaining controls down
            for (int j = i; j < win->control_count - 1; j++) {
                win->controls[j] = win->controls[j + 1];
            }
            win->control_count--;
            win->dirty = true;
            return true;
        }
    }
    return false;
}

struct ControlBase* qarma_win_get_control(QARMA_WIN_HANDLE* win, uint32_t control_id) {
    if (!win) return NULL;
    
    for (int i = 0; i < win->control_count; i++) {
        if (win->controls[i]->id == control_id) {
            return win->controls[i];
        }
    }
    return NULL;
}

void qarma_win_render_controls(QARMA_WIN_HANDLE* win) {
    if (!win || !win->pixel_buffer) return;
    
    for (int i = 0; i < win->control_count; i++) {
        struct ControlBase* control = win->controls[i];
        if (control->visible && control->render) {
            control->render(control->instance, win->pixel_buffer, 
                          win->size.width, win->size.height);
        }
    }
}

bool qarma_win_dispatch_event(QARMA_WIN_HANDLE* win, QARMA_INPUT_EVENT* event) {
    if (!win || !event) return false;
    
    // Iterate controls in reverse order (top to bottom in z-order)
    for (int i = win->control_count - 1; i >= 0; i--) {
        struct ControlBase* control = win->controls[i];
        if (control->visible && control->enabled && control->handle_event) {
            if (control->handle_event(control->instance, event)) {
                return true;  // Event was handled
            }
        }
    }
    return false;  // Event not handled
}