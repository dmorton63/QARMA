#include "textbox.h"
#include "../renderer.h"
#include "core/string.h"

// ============================================================================
// TextBox Implementation
// ============================================================================

void textbox_init(TextBox* tb, int x, int y, int width, int height, bool is_password) {
    if (!tb) return;
    
    tb->base.x = x;
    tb->base.y = y;
    tb->base.width = width;
    tb->base.height = height;
    tb->base.visible = true;
    tb->base.enabled = true;
    tb->base.id = control_generate_id();
    
    tb->text[0] = '\0';
    tb->max_length = 127;
    tb->is_password = is_password;
    tb->has_focus = false;
    tb->show_cursor = true;
    tb->cursor_blink_tick = 0;
    tb->on_change = NULL;
    tb->on_enter = NULL;
    tb->user_data = NULL;
}

void textbox_render(TextBox* tb, uint32_t* buffer, int buf_width, int buf_height) {
    if (!tb || !buffer || !tb->base.visible) return;
    
    int x = tb->base.x;
    int y = tb->base.y;
    int w = tb->base.width;
    int h = tb->base.height;
    
    // Draw background
    draw_filled_rect(buffer, buf_width, x, y, w, h, TEXTBOX_BG_COLOR);
    
    // Draw border
    uint32_t border_color = tb->has_focus ? TEXTBOX_FOCUSED_BORDER : TEXTBOX_BORDER_COLOR;
    int border_thickness = tb->has_focus ? 2 : 1;
    draw_rect_border(buffer, buf_width, x, y, w, h, border_color, border_thickness);
    
    // Draw text
    char display_text[129];
    if (tb->is_password) {
        int len = strlen(tb->text);
        for (int i = 0; i < len && i < 128; i++) {
            display_text[i] = '*';
        }
        display_text[len] = '\0';
    } else {
        strncpy(display_text, tb->text, 128);
        display_text[128] = '\0';
    }
    
    draw_string_to_buffer(buffer, buf_width, x + 5, y + h / 2 - 4, display_text, TEXT_COLOR);
    
    // Draw cursor
    if (tb->has_focus && tb->show_cursor) {
        int text_len = strlen(display_text);
        int cursor_x = x + 5 + text_len * 8;
        int cursor_y = y + 5;
        draw_filled_rect(buffer, buf_width, cursor_x, cursor_y, 2, h - 10, CURSOR_COLOR);
    }
}

void textbox_update(TextBox* tb) {
    if (!tb) return;
    
    tb->cursor_blink_tick++;
    if (tb->cursor_blink_tick >= 30) {
        tb->cursor_blink_tick = 0;
        tb->show_cursor = !tb->show_cursor;
    }
}

void textbox_handle_key(TextBox* tb, uint32_t keycode) {
    if (!tb || !tb->has_focus) return;
    
    // Backspace
    if (keycode == 0x0E) {
        int len = strlen(tb->text);
        if (len > 0) {
            tb->text[len - 1] = '\0';
            if (tb->on_change) tb->on_change(tb->user_data, tb->text);
        }
    }
    // Enter
    else if (keycode == 0x1C) {
        if (tb->on_enter) tb->on_enter(tb->user_data);
    }
}

void textbox_handle_char(TextBox* tb, char c) {
    if (!tb || !tb->has_focus) return;
    
    if (c < 32 || c > 126) return;
    
    int len = strlen(tb->text);
    if (len < (int)tb->max_length) {
        tb->text[len] = c;
        tb->text[len + 1] = '\0';
        if (tb->on_change) tb->on_change(tb->user_data, tb->text);
    }
    
    tb->cursor_blink_tick = 0;
    tb->show_cursor = true;
}

void textbox_handle_click(TextBox* tb, int click_x, int click_y) {
    if (!tb) return;
    
    if (control_point_in_bounds(&tb->base, click_x, click_y)) {
        textbox_set_focus(tb, true);
    }
}

void textbox_set_focus(TextBox* tb, bool focused) {
    if (tb) {
        tb->has_focus = focused;
        tb->cursor_blink_tick = 0;
        tb->show_cursor = true;
    }
}

void textbox_set_text(TextBox* tb, const char* text) {
    if (tb && text) {
        strncpy(tb->text, text, tb->max_length);
        tb->text[tb->max_length] = '\0';
    }
}

const char* textbox_get_text(TextBox* tb) {
    return tb ? tb->text : "";
}
