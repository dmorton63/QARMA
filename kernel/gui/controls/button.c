#include "button.h"
#include "../renderer.h"
#include "core/string.h"

// ============================================================================
// Button Implementation
// ============================================================================

void button_init(Button* btn, int x, int y, int width, int height, const char* label) {
    if (!btn) return;
    
    btn->base.x = x;
    btn->base.y = y;
    btn->base.width = width;
    btn->base.height = height;
    btn->base.visible = true;
    btn->base.enabled = true;
    btn->base.id = control_generate_id();
    
    if (label) {
        strncpy(btn->label, label, 63);
        btn->label[63] = '\0';
    } else {
        btn->label[0] = '\0';
    }
    
    btn->is_hovered = false;
    btn->is_pressed = false;
    btn->has_focus = false;
    btn->on_click = NULL;
    btn->user_data = NULL;
}

void button_render(Button* btn, uint32_t* buffer, int buf_width, int buf_height) {
    if (!btn || !buffer || !btn->base.visible) return;
    
    int x = btn->base.x;
    int y = btn->base.y;
    int w = btn->base.width;
    int h = btn->base.height;
    
    // Choose color based on state
    uint32_t bg_color = BUTTON_BG_COLOR;
    if (btn->is_pressed) bg_color = BUTTON_PRESSED_COLOR;
    else if (btn->is_hovered) bg_color = BUTTON_HOVER_COLOR;
    else if (btn->has_focus) bg_color = BUTTON_HOVER_COLOR;
    
    // Draw background
    draw_filled_rect(buffer, buf_width, x, y, w, h, bg_color);
    
    // Draw border
    draw_rect_border(buffer, buf_width, x, y, w, h, TEXTBOX_BORDER_COLOR, 1);
    
    // Draw label (centered)
    int label_len = strlen(btn->label);
    int text_x = x + (w - label_len * 8) / 2;
    int text_y = y + (h - 8) / 2;
    draw_string_to_buffer(buffer, buf_width, text_x, text_y, btn->label, 0xFFFFFF);
}

void button_handle_mouse_move(Button* btn, int mouse_x, int mouse_y) {
    if (!btn) return;
    
    btn->is_hovered = control_point_in_bounds(&btn->base, mouse_x, mouse_y);
}

void button_handle_click(Button* btn, int click_x, int click_y) {
    if (!btn) return;
    
    if (control_point_in_bounds(&btn->base, click_x, click_y)) {
        if (btn->on_click) {
            btn->on_click(btn->user_data);
        }
    }
}

void button_set_label(Button* btn, const char* label) {
    if (btn && label) {
        strncpy(btn->label, label, 63);
        btn->label[63] = '\0';
    }
}

void button_set_focus(Button* btn, bool focused) {
    if (btn) {
        btn->has_focus = focused;
    }
}

void button_activate(Button* btn) {
    if (btn && btn->base.enabled && btn->on_click) {
        btn->on_click(btn->user_data);
    }
}
