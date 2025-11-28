/**
 * QARMA - Hardened Button Control Implementation
 */

#include "gui/controls/qarma_button.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "config.h"

// ============================================================================
// Message Handler
// ============================================================================

static int32_t button_message_handler(qarma_control_t* control, qarma_message_t* msg) {
    if (!control || !msg) {
        return -1;
    }
    
    button_data_t* data = (button_data_t*)control->implementation_data;
    if (!data) {
        return -1;
    }
    
    switch (msg->type) {
        case MSG_CREATE:
            // Button created
            break;
            
        case MSG_DESTROY:
            // Button being destroyed
            break;
            
        case MSG_PAINT:
            // Paint request - actual rendering done by render function
            control_invalidate(control);
            break;
            
        case MSG_LBUTTONDOWN:
            control->state_flags |= CONTROL_STATE_PRESSED;
            control_invalidate(control);
            break;
            
        case MSG_LBUTTONUP:
            if (control->state_flags & CONTROL_STATE_PRESSED) {
                control->state_flags &= ~CONTROL_STATE_PRESSED;
                control_invalidate(control);
                
                // Fire click event
                if (data->on_click) {
                    data->on_click(control, control->user_data);
                }
            }
            break;
            
        case MSG_MOUSEMOVE:
            // Check if mouse is over button
            {
                int32_t mouse_x = (int32_t)(msg->wparam & 0xFFFF);
                int32_t mouse_y = (int32_t)((msg->wparam >> 16) & 0xFFFF);
                
                bool was_hovered = (control->state_flags & CONTROL_STATE_HOVERED) != 0;
                bool is_hovered = control_contains_point(control, mouse_x, mouse_y);
                
                if (is_hovered && !was_hovered) {
                    control->state_flags |= CONTROL_STATE_HOVERED;
                    control_invalidate(control);
                } else if (!is_hovered && was_hovered) {
                    control->state_flags &= ~CONTROL_STATE_HOVERED;
                    control->state_flags &= ~CONTROL_STATE_PRESSED;
                    control_invalidate(control);
                }
            }
            break;
            
        case MSG_KEYDOWN:
            // Enter or Space activates button
            if (control_has_focus(control)) {
                uint32_t key = (uint32_t)msg->wparam;
                if (key == 0x1C || key == 0x39) {  // Enter or Space
                    if (data->on_click) {
                        data->on_click(control, control->user_data);
                    }
                }
            }
            break;
            
        case MSG_SETFOCUS:
            control_invalidate(control);
            break;
            
        case MSG_KILLFOCUS:
            control->state_flags &= ~CONTROL_STATE_PRESSED;
            control_invalidate(control);
            break;
            
        default:
            break;
    }
    
    return 0;
}

// ============================================================================
// Render Function
// ============================================================================

static void button_render(qarma_control_t* control, uint32_t* buffer,
                         int32_t buf_width, int32_t buf_height) {
    if (!control || !buffer) {
        return;
    }
    
    button_data_t* data = (button_data_t*)control->implementation_data;
    if (!data) {
        return;
    }
    
    // Get absolute position
    int32_t abs_x, abs_y;
    control_get_absolute_position(control, &abs_x, &abs_y);
    
    // Determine colors based on state
    uint32_t bg_color;
    uint32_t fg_color = 0xFF000000;  // Black text
    uint32_t border_color;
    
    if (!(control->state_flags & CONTROL_STATE_ENABLED)) {
        bg_color = 0xFFC0C0C0;  // Gray
        fg_color = 0xFF808080;  // Dark gray text
        border_color = 0xFF808080;
    } else if (control->state_flags & CONTROL_STATE_PRESSED) {
        bg_color = 0xFF0078D7;  // Pressed blue
        fg_color = 0xFFFFFFFF;  // White text
        border_color = 0xFF0053BA;
    } else if (control->state_flags & CONTROL_STATE_HOVERED) {
        bg_color = 0xFFE5F1FB;  // Light blue
        border_color = 0xFF0078D7;
    } else {
        bg_color = 0xFFE1E1E1;  // Default gray
        border_color = 0xFF707070;
    }
    
    // Draw button background
    for (int32_t y = 0; y < control->height; y++) {
        for (int32_t x = 0; x < control->width; x++) {
            int32_t screen_x = abs_x + x;
            int32_t screen_y = abs_y + y;
            
            if (screen_x >= 0 && screen_x < buf_width &&
                screen_y >= 0 && screen_y < buf_height) {
                
                // Border
                if (x == 0 || x == control->width - 1 ||
                    y == 0 || y == control->height - 1) {
                    buffer[screen_y * buf_width + screen_x] = border_color;
                } else {
                    buffer[screen_y * buf_width + screen_x] = bg_color;
                }
            }
        }
    }
    
    // Draw focus rectangle if focused
    if (control_has_focus(control)) {
        for (int32_t x = 2; x < control->width - 2; x++) {
            int32_t screen_x = abs_x + x;
            buffer[(abs_y + 2) * buf_width + screen_x] = 0xFF000000;
            buffer[(abs_y + control->height - 3) * buf_width + screen_x] = 0xFF000000;
        }
        for (int32_t y = 2; y < control->height - 2; y++) {
            int32_t screen_y = abs_y + y;
            buffer[screen_y * buf_width + (abs_x + 2)] = 0xFF000000;
            buffer[screen_y * buf_width + (abs_x + control->width - 3)] = 0xFF000000;
        }
    }
    
    // Draw text (centered)
    if (data->text[0] != '\0') {
        extern void draw_string_to_buffer(uint32_t* buffer, int buf_width, int x, int y, const char* str, uint32_t color);
        
        // Calculate text position (centered)
        // Assume 8x12 font, approximate text width
        int text_len = 0;
        for (const char* p = data->text; *p != '\0'; p++) text_len++;
        
        int text_width = text_len * 8;  // 8 pixels per character
        int text_height = 12;
        
        int text_x = abs_x + (control->width - text_width) / 2;
        int text_y = abs_y + (control->height - text_height) / 2;
        
        draw_string_to_buffer(buffer, buf_width, text_x, text_y, data->text, fg_color);
    }
}

// ============================================================================
// Cleanup Function
// ============================================================================

static void button_cleanup(qarma_control_t* control) {
    if (!control) {
        return;
    }
    
    // Button data will be freed by control_destroy
}

// ============================================================================
// Public API
// ============================================================================

qarma_control_t* button_create(qarma_frame_t* parent_frame, const char* name,
                               int32_t x, int32_t y, int32_t width, int32_t height,
                               const char* text) {
    // Create control
    qarma_control_t* control = control_create(parent_frame, CONTROL_TYPE_BUTTON,
                                              name, x, y, width, height);
    if (!control) {
        return NULL;
    }
    
    // Allocate button data
    button_data_t* data = (button_data_t*)heap_alloc(sizeof(button_data_t));
    if (!data) {
        control_destroy(control);
        return NULL;
    }
    
    memset(data, 0, sizeof(button_data_t));
    
    if (text) {
        strncpy(data->text, text, sizeof(data->text) - 1);
        data->text[sizeof(data->text) - 1] = '\0';
    }
    
    data->is_default = false;
    data->on_click = NULL;
    
    // Set implementation data
    control->implementation_data = data;
    
    // Set handlers
    control->message_handler = button_message_handler;
    control->render_func = button_render;
    control->cleanup_func = button_cleanup;
    
    // Set default colors
    control->background.red = 225;
    control->background.green = 225;
    control->background.blue = 225;
    control->background.alpha = 255;
    
    control->foreground.red = 0;
    control->foreground.green = 0;
    control->foreground.blue = 0;
    control->foreground.alpha = 255;
    
    return control;
}

void button_set_text(qarma_control_t* button, const char* text) {
    if (!button || button->type != CONTROL_TYPE_BUTTON) {
        return;
    }
    
    button_data_t* data = (button_data_t*)button->implementation_data;
    if (!data || !text) {
        return;
    }
    
    strncpy(data->text, text, sizeof(data->text) - 1);
    data->text[sizeof(data->text) - 1] = '\0';
    
    control_invalidate(button);
}

const char* button_get_text(qarma_control_t* button) {
    if (!button || button->type != CONTROL_TYPE_BUTTON) {
        return NULL;
    }
    
    button_data_t* data = (button_data_t*)button->implementation_data;
    if (!data) {
        return NULL;
    }
    
    return data->text;
}

void button_set_click_handler(qarma_control_t* button,
                              void (*handler)(qarma_control_t*, void*)) {
    if (!button || button->type != CONTROL_TYPE_BUTTON) {
        return;
    }
    
    button_data_t* data = (button_data_t*)button->implementation_data;
    if (!data) {
        return;
    }
    
    data->on_click = handler;
}

void button_set_default(qarma_control_t* button, bool is_default) {
    if (!button || button->type != CONTROL_TYPE_BUTTON) {
        return;
    }
    
    button_data_t* data = (button_data_t*)button->implementation_data;
    if (!data) {
        return;
    }
    
    data->is_default = is_default;
    control_invalidate(button);
}

void button_click(qarma_control_t* button) {
    if (!button || button->type != CONTROL_TYPE_BUTTON) {
        return;
    }
    
    button_data_t* data = (button_data_t*)button->implementation_data;
    if (!data) {
        return;
    }
    
    if (data->on_click) {
        data->on_click(button, button->user_data);
    }
}
