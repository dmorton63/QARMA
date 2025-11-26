#pragma once

#include "../control_base.h"

// ============================================================================
// TextBox - Editable text input control
// ============================================================================

typedef struct {
    ControlBase base;
    char text[128];
    uint32_t max_length;
    bool is_password;
    bool has_focus;
    bool show_cursor;
    uint32_t cursor_blink_tick;
    void (*on_change)(void* user_data, const char* text);
    void (*on_enter)(void* user_data);
    void* user_data;
} TextBox;

// ============================================================================
// TextBox API
// ============================================================================

void textbox_init(TextBox* tb, int x, int y, int width, int height, bool is_password);
void textbox_render(TextBox* tb, uint32_t* buffer, int buf_width, int buf_height);
void textbox_update(TextBox* tb);
void textbox_handle_key(TextBox* tb, uint32_t keycode);
void textbox_handle_char(TextBox* tb, char c);
void textbox_handle_click(TextBox* tb, int click_x, int click_y);
void textbox_set_focus(TextBox* tb, bool focused);
void textbox_set_text(TextBox* tb, const char* text);
const char* textbox_get_text(TextBox* tb);
