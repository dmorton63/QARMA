/**
 * QARMA - Hardened TextBox Control Implementation
 */

#include "controls/controls/qarma_textbox.h"
#include "memory/heap.h"
#include "string.h"
#include "config.h"

// ============================================================================
// Message Handler
// ============================================================================

static int32_t textbox_message_handler(qarma_control_t* control, qarma_message_t* msg) {
    if (!control || !msg) {
        return -1;
    }
    
    textbox_data_t* data = (textbox_data_t*)control->implementation_data;
    if (!data) {
        return -1;
    }
    
    switch (msg->type) {
        case MSG_CREATE:
            // TextBox created
            break;
            
        case MSG_DESTROY:
            // TextBox being destroyed
            break;
            
        case MSG_PAINT:
            // Paint request
            control_invalidate(control);
            break;
            
        case MSG_LBUTTONDOWN:
            // Set focus on click
            control_set_focus(control);
            // TODO: Set cursor position based on click location
            control_invalidate(control);
            break;
            
        case MSG_CHAR:
            // Character input
            if (control_has_focus(control)) {
                uint32_t ch = (uint32_t)msg->wparam;
                uint32_t len = strlen(data->text);
                
                if (ch >= 32 && ch < 127) {  // Printable ASCII
                    if (len < data->max_length && len < sizeof(data->text) - 1) {
                        // Insert character at cursor
                        if (data->cursor_pos < len) {
                            // Shift text right
                            for (uint32_t i = len; i > data->cursor_pos; i--) {
                                data->text[i] = data->text[i - 1];
                            }
                        }
                        data->text[data->cursor_pos] = (char)ch;
                        data->text[len + 1] = '\0';
                        data->cursor_pos++;
                        
                        control_invalidate(control);
                        
                        // Fire changed event
                        if (data->on_changed) {
                            data->on_changed(control, control->user_data);
                        }
                    }
                }
            }
            break;
            
        case MSG_KEYDOWN:
            if (control_has_focus(control)) {
                uint32_t key = (uint32_t)msg->wparam;
                uint32_t len = strlen(data->text);
                bool changed = false;
                
                if (key == 0x0E) {  // Backspace
                    if (data->cursor_pos > 0) {
                        // Shift text left
                        for (uint32_t i = data->cursor_pos - 1; i < len; i++) {
                            data->text[i] = data->text[i + 1];
                        }
                        data->cursor_pos--;
                        changed = true;
                    }
                } else if (key == 0x53) {  // Delete
                    if (data->cursor_pos < len) {
                        // Shift text left
                        for (uint32_t i = data->cursor_pos; i < len; i++) {
                            data->text[i] = data->text[i + 1];
                        }
                        changed = true;
                    }
                } else if (key == 0x4B) {  // Left arrow
                    if (data->cursor_pos > 0) {
                        data->cursor_pos--;
                        control_invalidate(control);
                    }
                } else if (key == 0x4D) {  // Right arrow
                    if (data->cursor_pos < len) {
                        data->cursor_pos++;
                        control_invalidate(control);
                    }
                } else if (key == 0x47) {  // Home
                    data->cursor_pos = 0;
                    control_invalidate(control);
                } else if (key == 0x4F) {  // End
                    data->cursor_pos = len;
                    control_invalidate(control);
                }
                
                if (changed) {
                    control_invalidate(control);
                    if (data->on_changed) {
                        data->on_changed(control, control->user_data);
                    }
                }
            }
            break;
            
        case MSG_SETFOCUS:
            control_invalidate(control);
            break;
            
        case MSG_KILLFOCUS:
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

static void textbox_render(qarma_control_t* control, uint32_t* buffer,
                          int32_t buf_width, int32_t buf_height) {
    if (!control || !buffer) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)control->implementation_data;
    if (!data) {
        return;
    }
    
    // Get absolute position
    int32_t abs_x, abs_y;
    control_get_absolute_position(control, &abs_x, &abs_y);
    
    // Background color
    uint32_t bg_color = 0xFFFFFFFF;  // White
    uint32_t border_color = control_has_focus(control) ? 0xFF0078D7 : 0xFF707070;
    
    if (!(control->state_flags & CONTROL_STATE_ENABLED)) {
        bg_color = 0xFFF0F0F0;  // Light gray
        border_color = 0xFFC0C0C0;
    }
    
    // Draw textbox background and border
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
    
    // Draw cursor if focused
    if (control_has_focus(control)) {
        int32_t cursor_x = abs_x + 4 + (data->cursor_pos * 8);  // Approximate character width
        if (cursor_x < abs_x + control->width - 2) {
            for (int32_t y = 4; y < control->height - 4; y++) {
                int32_t screen_y = abs_y + y;
                if (screen_y >= 0 && screen_y < buf_height &&
                    cursor_x >= 0 && cursor_x < buf_width) {
                    buffer[screen_y * buf_width + cursor_x] = 0xFF000000;
                }
            }
        }
    }
    
    // Render text
    extern void draw_string_to_buffer(uint32_t* buffer, int buf_width, int x, int y, 
                                       const char* str, uint32_t color);
    
    uint32_t text_color = (control->state_flags & CONTROL_STATE_ENABLED) ? 0xFF000000 : 0xFF808080;
    
    if (control->style_flags & CONTROL_STYLE_MULTILINE) {
        // Multiline: render line by line
        int32_t text_y = abs_y + 4;
        int32_t line_height = 10;
        const char* ptr = data->text;
        char line_buffer[256];
        
        while (*ptr && text_y < abs_y + control->height - 4) {
            // Find end of line
            int line_len = 0;
            while (*ptr && *ptr != '\n' && line_len < 255) {
                line_buffer[line_len++] = *ptr++;
            }
            line_buffer[line_len] = '\0';
            
            // Draw line
            if (line_len > 0) {
                draw_string_to_buffer(buffer, buf_width, abs_x + 4, text_y, 
                                     line_buffer, text_color);
            }
            
            text_y += line_height;
            
            // Skip newline
            if (*ptr == '\n') {
                ptr++;
            }
        }
    } else {
        // Single line
        if (data->text[0] != '\0') {
            draw_string_to_buffer(buffer, buf_width, abs_x + 4, abs_y + 4,
                                 data->text, text_color);
        }
    }
}

// ============================================================================
// Cleanup Function
// ============================================================================

static void textbox_cleanup(qarma_control_t* control) {
    if (!control) {
        return;
    }
    
    // TextBox data will be freed by control_destroy
}

// ============================================================================
// Public API
// ============================================================================

qarma_control_t* textbox_create(qarma_frame_t* parent_frame, const char* name,
                                int32_t x, int32_t y, int32_t width, int32_t height) {
    // Create control
    qarma_control_t* control = control_create(parent_frame, CONTROL_TYPE_TEXTBOX,
                                              name, x, y, width, height);
    if (!control) {
        return NULL;
    }
    
    // Allocate textbox data
    textbox_data_t* data = (textbox_data_t*)heap_alloc(sizeof(textbox_data_t));
    if (!data) {
        control_destroy(control);
        return NULL;
    }
    
    memset(data, 0, sizeof(textbox_data_t));
    
    data->cursor_pos = 0;
    data->selection_start = 0;
    data->selection_end = 0;
    data->password_mode = false;
    data->password_char = '*';
    data->max_length = sizeof(data->text) - 1;
    data->on_changed = NULL;
    
    // Set implementation data
    control->implementation_data = data;
    
    // Set handlers
    control->message_handler = textbox_message_handler;
    control->render_func = textbox_render;
    control->cleanup_func = textbox_cleanup;
    
    // Set default colors
    control->background.red = 255;
    control->background.green = 255;
    control->background.blue = 255;
    control->background.alpha = 255;
    
    control->style_flags |= CONTROL_STYLE_BORDER;
    
    return control;
}

void textbox_set_text(qarma_control_t* textbox, const char* text) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data || !text) {
        return;
    }
    
    strncpy(data->text, text, sizeof(data->text) - 1);
    data->text[sizeof(data->text) - 1] = '\0';
    
    data->cursor_pos = strlen(data->text);
    
    control_invalidate(textbox);
}

const char* textbox_get_text(qarma_control_t* textbox) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return NULL;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data) {
        return NULL;
    }
    
    return data->text;
}

void textbox_set_password_mode(qarma_control_t* textbox, bool password, char password_char) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data) {
        return;
    }
    
    data->password_mode = password;
    data->password_char = password_char;
    
    control_invalidate(textbox);
}

void textbox_set_max_length(qarma_control_t* textbox, uint32_t max_length) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data) {
        return;
    }
    
    data->max_length = max_length < sizeof(data->text) ? max_length : sizeof(data->text) - 1;
}

void textbox_set_changed_handler(qarma_control_t* textbox,
                                 void (*handler)(qarma_control_t*, void*)) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data) {
        return;
    }
    
    data->on_changed = handler;
}

void textbox_clear(qarma_control_t* textbox) {
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) {
        return;
    }
    
    textbox_data_t* data = (textbox_data_t*)textbox->implementation_data;
    if (!data) {
        return;
    }
    
    data->text[0] = '\0';
    data->cursor_pos = 0;
    data->selection_start = 0;
    data->selection_end = 0;
    
    control_invalidate(textbox);
    
    if (data->on_changed) {
        data->on_changed(textbox, textbox->user_data);
    }
}
