#pragma once

#include "../control_base.h"

// ============================================================================
// Button - Clickable button control
// ============================================================================

typedef struct {
    ControlBase base;
    char label[64];
    bool is_hovered;
    bool is_pressed;
    bool has_focus;
    void (*on_click)(void* user_data);
    void* user_data;
} Button;

// ============================================================================
// Button API
// ============================================================================

void button_init(Button* btn, int x, int y, int width, int height, const char* label);
void button_render(Button* btn, uint32_t* buffer, int buf_width, int buf_height);
void button_handle_mouse_move(Button* btn, int mouse_x, int mouse_y);
void button_handle_click(Button* btn, int click_x, int click_y);
void button_set_focus(Button* btn, bool focused);
void button_activate(Button* btn);
void button_set_label(Button* btn, const char* label);
