#ifndef CLOSE_BUTTON_H
#define CLOSE_BUTTON_H

#include "../control_base.h"

// Close button - small X button for window title bars
typedef struct {
    ControlBase base;
    bool hovered;
    bool pressed;
    bool focused;
    void (*on_click)(void* userdata);
    void* userdata;
} CloseButton;

// Initialize a close button (typically 16x16 or 20x20)
void close_button_init(CloseButton* cb, int x, int y, int size);

// Render the close button
void close_button_render(CloseButton* cb, uint32_t* buffer, int buf_width, int buf_height);

// Update hover/pressed state based on mouse position
void close_button_update(CloseButton* cb, int mouse_x, int mouse_y, bool mouse_down);

// Handle mouse click
void close_button_handle_click(CloseButton* cb, int mouse_x, int mouse_y);

// Handle keyboard activation (Enter/Space when focused)
void close_button_activate(CloseButton* cb);

// Set focus state
void close_button_set_focus(CloseButton* cb, bool focused);

#endif // CLOSE_BUTTON_H
