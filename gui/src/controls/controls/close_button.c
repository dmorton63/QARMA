#include "controls/controls/close_button.h"
#include "renderer.h"
#include "qarma_input_events.h"

static bool close_button_handle_event_impl(CloseButton* cb, QARMA_INPUT_EVENT* event);

void close_button_init(CloseButton* cb, int x, int y, int size) {
    cb->base.x = x;
    cb->base.y = y;
    cb->base.width = size;
    cb->base.height = size;
    cb->base.visible = true;
    cb->base.enabled = true;
    cb->base.id = control_generate_id();
    cb->base.instance = cb;
    cb->base.render = (void (*)(void*, uint32_t*, int, int))close_button_render;
    cb->base.handle_event = (bool (*)(void*, QARMA_INPUT_EVENT*))close_button_handle_event_impl;
    cb->hovered = false;
    cb->pressed = false;
    cb->focused = false;
    cb->on_click = NULL;
    cb->userdata = NULL;
}

static bool close_button_handle_event_impl(CloseButton* cb, QARMA_INPUT_EVENT* event) {
    if (!cb || !event || !cb->base.enabled) return false;
    
    if (event->type == QARMA_INPUT_EVENT_MOUSE_MOVE) {
        bool was_hovered = cb->hovered;
        cb->hovered = control_point_in_bounds(&cb->base, event->data.mouse.x, event->data.mouse.y);
        return cb->hovered != was_hovered;
    }
    else if (event->type == QARMA_INPUT_EVENT_MOUSE_DOWN) {
        if (control_point_in_bounds(&cb->base, event->data.mouse.x, event->data.mouse.y)) {
            cb->pressed = true;
            return true;
        }
    }
    else if (event->type == QARMA_INPUT_EVENT_MOUSE_UP) {
        if (cb->pressed && control_point_in_bounds(&cb->base, event->data.mouse.x, event->data.mouse.y)) {
            cb->pressed = false;
            if (cb->on_click) {
                cb->on_click(cb->userdata);
            }
            return true;
        }
        cb->pressed = false;
    }
    else if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        if (cb->focused && (event->data.key.scancode == 0x1C || event->data.key.scancode == 0x39)) {  // Enter or Space
            if (cb->on_click) {
                cb->on_click(cb->userdata);
            }
            return true;
        }
    }
    
    return false;
}

void close_button_render(CloseButton* cb, uint32_t* buffer, int buf_width, int buf_height) {
    if (!cb->base.visible) return;

    int x = cb->base.x;
    int y = cb->base.y;
    int w = cb->base.width;
    int h = cb->base.height;

    // Background color based on state
    uint32_t bg_color;
    if (cb->pressed) {
        bg_color = COLOR_BUTTON_PRESSED;
    } else if (cb->focused) {
        bg_color = COLOR_BUTTON_BG;  // Brighter when focused
    } else if (cb->hovered) {
        bg_color = COLOR_BUTTON_HOVER;
    } else {
        bg_color = 0xFF2D2D30;  // Darker gray when not focused
    }

    // Draw button background
    draw_filled_rect(buffer, buf_width, x, y, w, h, bg_color);
    draw_rect_border(buffer, buf_width, x, y, w, h, COLOR_BORDER, 1);

    // Draw X in the center
    uint32_t x_color = COLOR_TEXT;
    int center_x = x + w / 2;
    int center_y = y + h / 2;
    int x_size = w / 3; // Size of the X

    // Draw X as two diagonal lines (simplified - just pixels)
    for (int i = -x_size; i <= x_size; i++) {
        // Top-left to bottom-right diagonal
        int px1 = center_x + i;
        int py1 = center_y + i;
        if (px1 >= x && px1 < x + w && py1 >= y && py1 < y + h) {
            if (py1 >= 0 && py1 < buf_height && px1 >= 0 && px1 < buf_width) {
                buffer[py1 * buf_width + px1] = x_color;
            }
        }

        // Top-right to bottom-left diagonal
        int px2 = center_x + i;
        int py2 = center_y - i;
        if (px2 >= x && px2 < x + w && py2 >= y && py2 < y + h) {
            if (py2 >= 0 && py2 < buf_height && px2 >= 0 && px2 < buf_width) {
                buffer[py2 * buf_width + px2] = x_color;
            }
        }
    }

    // Focus indicator (thin border inside)
    if (cb->focused) {
        draw_rect_border(buffer, buf_width, x + 2, y + 2, w - 4, h - 4, COLOR_FOCUS, 1);
    }
}

void close_button_update(CloseButton* cb, int mouse_x, int mouse_y, bool mouse_down) {
    if (!cb->base.enabled) return;

    bool was_hovered = cb->hovered;
    cb->hovered = control_point_in_bounds(&cb->base, mouse_x, mouse_y);

    if (cb->hovered && mouse_down) {
        cb->pressed = true;
    } else if (!mouse_down && cb->pressed && cb->hovered) {
        cb->pressed = false;
        // Click happened
        if (cb->on_click) {
            cb->on_click(cb->userdata);
        }
    } else if (!mouse_down) {
        cb->pressed = false;
    }
}

void close_button_handle_click(CloseButton* cb, int mouse_x, int mouse_y) {
    if (!cb->base.enabled) return;
    if (control_point_in_bounds(&cb->base, mouse_x, mouse_y)) {
        if (cb->on_click) {
            cb->on_click(cb->userdata);
        }
    }
}

void close_button_activate(CloseButton* cb) {
    if (!cb->base.enabled || !cb->focused) return;
    if (cb->on_click) {
        cb->on_click(cb->userdata);
    }
}

void close_button_set_focus(CloseButton* cb, bool focused) {
    cb->focused = focused;
}
