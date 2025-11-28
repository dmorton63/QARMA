/*
 * QARMA - Boot Messages Window Implementation (New Architecture)
 */

#include "boot_messages.h"
#include "controls/controls/qarma_button.h"
#include "controls/controls/qarma_label.h"
#include "controls/controls/qarma_textbox.h"
#include "memory/heap.h"
#include "string.h"
#include "kernel.h"
#include "handle_manager.h"
#include "graphics.h"
#include "config.h"

// Layout constants
#define TITLE_BAR_HEIGHT 30
#define TEXT_PADDING 10
#define LINE_HEIGHT 16
#define CLOSE_BUTTON_WIDTH 60
#define CLOSE_BUTTON_HEIGHT 25

// Colors
#define BG_COLOR 0xFF1E1E1E
#define TITLE_BG_COLOR 0xFF2D2D30
#define TEXT_COLOR 0xFFCCCCCC
#define BORDER_COLOR 0xFF3E3E42

// Message handler for boot messages frame
static int32_t boot_messages_frame_message_handler(qarma_handle_t recipient, qarma_message_t* msg) {
    if (!msg) return 0;
    
    qarma_frame_t* frame = (qarma_frame_t*)handle_get_object(recipient);
    if (!frame) return 0;
    
    boot_messages_window_t* bmw = (boot_messages_window_t*)frame->user_data;
    if (!bmw) return 0;
    
    switch (msg->type) {
        case MSG_PAINT:
            boot_messages_render(bmw);
            return 1;
            
        case MSG_KEYDOWN: {
            uint8_t scancode = (uint8_t)(msg->wparam & 0xFF);
            // ESC to close
            if (scancode == 0x01) {
                boot_messages_set_visible(bmw, false);
                if (bmw->on_close) {
                    bmw->on_close(bmw->close_user_data);
                }
                return 1;
            }
            // Forward other keys to textbox for scrolling
            if (bmw->message_textbox && bmw->message_textbox->message_handler) {
                return bmw->message_textbox->message_handler(bmw->message_textbox->handle, msg);
            }
            return 0;
        }
        
        case MSG_MOUSEWHEEL: {
            // Forward to textbox for scrolling
            if (bmw->message_textbox && bmw->message_textbox->message_handler) {
                return bmw->message_textbox->message_handler(bmw->message_textbox->handle, msg);
            }
            return 1;
        }
        
        default:
            return 0;
    }
}

// Close button click handler
static void on_close_button_click(qarma_control_t* button, void* user_data) {
    boot_messages_window_t* bmw = (boot_messages_window_t*)user_data;
    if (bmw) {
        boot_messages_set_visible(bmw, false);
        if (bmw->on_close) {
            bmw->on_close(bmw->close_user_data);
        }
    }
}

boot_messages_window_t* boot_messages_create(int32_t x, int32_t y, int32_t width, int32_t height) {
    SERIAL_LOG("[BOOT_MSG_NEW] Creating boot messages window\n");
    
    boot_messages_window_t* bmw = (boot_messages_window_t*)heap_alloc(sizeof(boot_messages_window_t));
    if (!bmw) {
        SERIAL_LOG("[BOOT_MSG_NEW] ERROR: Failed to allocate boot_messages_window_t\n");
        return NULL;
    }
    
    memset(bmw, 0, sizeof(boot_messages_window_t));
    bmw->visible = true;
    bmw->active = true;
    
    // Create main frame
    bmw->main_frame = frame_create(
        NULL,
        x, y, width, height,
        FRAME_STYLE_BORDER | FRAME_STYLE_TITLE_BAR,
        "Boot Messages"
    );
    
    if (!bmw->main_frame) {
        SERIAL_LOG("[BOOT_MSG_NEW] ERROR: Failed to create frame\n");
        heap_free(bmw);
        return NULL;
    }
    
    // Set frame colors
    bmw->main_frame->background.red = (BG_COLOR >> 16) & 0xFF;
    bmw->main_frame->background.green = (BG_COLOR >> 8) & 0xFF;
    bmw->main_frame->background.blue = BG_COLOR & 0xFF;
    bmw->main_frame->background.alpha = 255;
    
    bmw->main_frame->border_color.red = (BORDER_COLOR >> 16) & 0xFF;
    bmw->main_frame->border_color.green = (BORDER_COLOR >> 8) & 0xFF;
    bmw->main_frame->border_color.blue = BORDER_COLOR & 0xFF;
    bmw->main_frame->border_color.alpha = 255;
    
    // Set message handler
    bmw->main_frame->message_handler = boot_messages_frame_message_handler;
    bmw->main_frame->user_data = bmw;
    
    // Create title label
    bmw->title_label = label_create(
        bmw->main_frame,
        "title_label",
        TEXT_PADDING, 5,
        width - 80, 20,
        "Boot Messages"
    );
    if (bmw->title_label) {
        frame_add_control(bmw->main_frame, bmw->title_label);
    }
    
    // Create close button
    int32_t close_x = width - CLOSE_BUTTON_WIDTH - 10;
    int32_t close_y = 5;
    bmw->close_button = button_create(
        bmw->main_frame,
        "close_button",
        close_x, close_y,
        CLOSE_BUTTON_WIDTH, CLOSE_BUTTON_HEIGHT,
        "Close"
    );
    if (bmw->close_button) {
        button_data_t* btn_data = (button_data_t*)bmw->close_button->implementation_data;
        if (btn_data) {
            btn_data->on_click = on_close_button_click;
        }
        bmw->close_button->user_data = bmw;
        frame_add_control(bmw->main_frame, bmw->close_button);
    }
    
    // Create message textbox (read-only, multiline)
    int32_t textbox_y = TITLE_BAR_HEIGHT + TEXT_PADDING;
    int32_t textbox_width = width - (TEXT_PADDING * 2);
    int32_t textbox_height = height - textbox_y - TEXT_PADDING;
    
    bmw->message_textbox = textbox_create(
        bmw->main_frame,
        "message_textbox",
        TEXT_PADDING, textbox_y,
        textbox_width, textbox_height
    );
    
    if (bmw->message_textbox) {
        // Make it read-only and multiline
        bmw->message_textbox->state_flags |= CONTROL_STATE_READONLY;
        bmw->message_textbox->style_flags |= CONTROL_STYLE_MULTILINE;
        frame_add_control(bmw->main_frame, bmw->message_textbox);
    }
    
    SERIAL_LOG("[BOOT_MSG_NEW] Boot messages window created successfully\n");
    return bmw;
}

void boot_messages_destroy(boot_messages_window_t* bmw) {
    if (!bmw) return;
    
    SERIAL_LOG("[BOOT_MSG_NEW] Destroying boot messages window\n");
    
    // Controls are owned by frame and will be destroyed with it
    if (bmw->main_frame) {
        frame_destroy(bmw->main_frame);
    }
    
    heap_free(bmw);
}

void boot_messages_add(boot_messages_window_t* bmw, const char* message) {
    if (!bmw || !message || !bmw->message_textbox) return;
    
    textbox_data_t* data = (textbox_data_t*)bmw->message_textbox->implementation_data;
    if (!data) return;
    
    // Get current text length
    size_t current_len = strlen(data->text);
    size_t message_len = strlen(message);
    
    // Check if we have space (leave room for newline and null terminator)
    if (current_len + message_len + 2 < sizeof(data->text)) {
        // Add newline if not first message
        if (current_len > 0) {
            strcat(data->text, "\n");
        }
        // Append message
        strcat(data->text, message);
    }
}

void boot_messages_clear(boot_messages_window_t* bmw) {
    if (!bmw || !bmw->message_textbox) return;
    
    textbox_data_t* data = (textbox_data_t*)bmw->message_textbox->implementation_data;
    if (!data) return;
    
    data->text[0] = '\0';
    data->cursor_pos = 0;
}

void boot_messages_set_close_callback(boot_messages_window_t* bmw, 
                                      void (*callback)(void* user_data), 
                                      void* user_data) {
    if (!bmw) return;
    bmw->on_close = callback;
    bmw->close_user_data = user_data;
}

void boot_messages_handle_event(boot_messages_window_t* bmw, QARMA_INPUT_EVENT* event) {
    if (!bmw || !event || !bmw->active) return;
    
    // Convert event to message and send to frame
    qarma_message_t msg = {0};
    msg.target = bmw->main_frame->handle;
    
    switch (event->type) {
        case QARMA_INPUT_EVENT_KEY_DOWN:
            msg.type = MSG_KEYDOWN;
            msg.wparam = event->data.key.scancode | (event->data.key.modifiers << 8);
            msg.lparam = event->data.key.character;
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_MOVE:
            msg.type = MSG_MOUSEMOVE;
            msg.wparam = event->data.mouse.x | (event->data.mouse.y << 16);
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_DOWN:
            msg.type = MSG_LBUTTONDOWN;
            msg.wparam = event->data.mouse.x | (event->data.mouse.y << 16);
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_SCROLL:
            msg.type = MSG_MOUSEWHEEL;
            msg.wparam = event->data.mouse.scroll_delta;
            break;
            
        default:
            return;
    }
    
    // Send to frame
    if (bmw->main_frame->message_handler) {
        bmw->main_frame->message_handler(bmw->main_frame->handle, &msg);
    }
}

void boot_messages_set_visible(boot_messages_window_t* bmw, bool visible) {
    if (!bmw || !bmw->main_frame) return;
    
    bmw->main_frame->visible = visible;
    bmw->visible = visible;
    bmw->active = visible;
}

bool boot_messages_is_visible(boot_messages_window_t* bmw) {
    if (!bmw) return false;
    return bmw->visible;
}

void boot_messages_update(boot_messages_window_t* bmw) {
    if (!bmw || !bmw->active) return;
    
    // Update for animations, cursor blink, etc.
    // Currently nothing to update
}

void boot_messages_render(boot_messages_window_t* bmw) {
    if (!bmw || !bmw->main_frame || !bmw->main_frame->visible) return;
    
    // Render frame (which will render all child controls)
    frame_render(bmw->main_frame);
}
