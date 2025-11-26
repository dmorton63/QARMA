/*
 * QARMA - Boot Messages Window Implementation (New Architecture)
 */

#include "boot_messages.h"
#include "gui/controls/qarma_button.h"
#include "gui/controls/qarma_label.h"
#include "memory/heap.h"
#include "core/string.h"
#include "core/kernel.h"
#include "core/handle_manager.h"
#include "graphics/graphics.h"
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
            // Up arrow
            if (scancode == 0x48) {
                boot_messages_scroll(bmw, -1);
                return 1;
            }
            // Down arrow
            if (scancode == 0x50) {
                boot_messages_scroll(bmw, 1);
                return 1;
            }
            // Page Up
            if (scancode == 0x49) {
                boot_messages_scroll(bmw, -10);
                return 1;
            }
            // Page Down
            if (scancode == 0x51) {
                boot_messages_scroll(bmw, 10);
                return 1;
            }
            // ESC
            if (scancode == 0x01) {
                boot_messages_set_visible(bmw, false);
                if (bmw->on_close) {
                    bmw->on_close(bmw->close_user_data);
                }
                return 1;
            }
            return 0;
        }
        
        case MSG_MOUSEWHEEL: {
            int32_t delta = (int32_t)msg->wparam;
            boot_messages_scroll(bmw, -delta);  // Negative because wheel up = scroll up
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
    bmw->message_count = 0;
    bmw->scroll_offset = 0;
    
    // Calculate visible lines
    int32_t content_height = height - TITLE_BAR_HEIGHT - TEXT_PADDING * 2;
    bmw->visible_lines = content_height / LINE_HEIGHT;
    
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
    
    // Create message labels (initially empty and hidden)
    int32_t label_y = TITLE_BAR_HEIGHT + TEXT_PADDING;
    int32_t label_width = width - (TEXT_PADDING * 2);
    
    for (int32_t i = 0; i < MAX_BOOT_MESSAGES; i++) {
        int32_t y_pos = label_y + (i * LINE_HEIGHT);
        
        bmw->message_labels[i] = label_create(
            bmw->main_frame,
            "message_label",
            TEXT_PADDING, y_pos,
            label_width, LINE_HEIGHT,
            ""
        );
        
        if (bmw->message_labels[i]) {
            // Initially hide all message labels
            bmw->message_labels[i]->state_flags &= ~CONTROL_STATE_VISIBLE;
            frame_add_control(bmw->main_frame, bmw->message_labels[i]);
        }
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
    if (!bmw || !message) return;
    
    if (bmw->message_count >= MAX_BOOT_MESSAGES) {
        // Shift messages up (remove oldest)
        for (int32_t i = 0; i < MAX_BOOT_MESSAGES - 1; i++) {
            strncpy(bmw->messages[i], bmw->messages[i + 1], MAX_MESSAGE_LENGTH - 1);
            bmw->messages[i][MAX_MESSAGE_LENGTH - 1] = '\0';
        }
        bmw->message_count = MAX_BOOT_MESSAGES - 1;
    }
    
    // Add new message
    strncpy(bmw->messages[bmw->message_count], message, MAX_MESSAGE_LENGTH - 1);
    bmw->messages[bmw->message_count][MAX_MESSAGE_LENGTH - 1] = '\0';
    bmw->message_count++;
    
    // Auto-scroll to bottom to show latest message
    if (bmw->message_count > bmw->visible_lines) {
        bmw->scroll_offset = bmw->message_count - bmw->visible_lines;
    }
    
    // Update visible labels
    for (int32_t i = 0; i < bmw->visible_lines && i < bmw->message_count; i++) {
        int32_t msg_index = bmw->scroll_offset + i;
        if (msg_index >= 0 && msg_index < bmw->message_count && bmw->message_labels[i]) {
            label_set_text(bmw->message_labels[i], bmw->messages[msg_index]);
            bmw->message_labels[i]->state_flags |= CONTROL_STATE_VISIBLE;
        }
    }
    
    // Hide unused labels
    for (int32_t i = bmw->message_count; i < MAX_BOOT_MESSAGES; i++) {
        if (bmw->message_labels[i]) {
            bmw->message_labels[i]->state_flags &= ~CONTROL_STATE_VISIBLE;
        }
    }
}

void boot_messages_clear(boot_messages_window_t* bmw) {
    if (!bmw) return;
    
    bmw->message_count = 0;
    bmw->scroll_offset = 0;
    
    // Clear and hide all labels
    for (int32_t i = 0; i < MAX_BOOT_MESSAGES; i++) {
        if (bmw->message_labels[i]) {
            label_set_text(bmw->message_labels[i], "");
            bmw->message_labels[i]->state_flags &= ~CONTROL_STATE_VISIBLE;
        }
    }
}

void boot_messages_scroll(boot_messages_window_t* bmw, int32_t delta) {
    if (!bmw) return;
    
    // Calculate new scroll offset
    int32_t new_offset = bmw->scroll_offset + delta;
    
    // Clamp to valid range
    int32_t max_offset = bmw->message_count - bmw->visible_lines;
    if (max_offset < 0) max_offset = 0;
    
    if (new_offset < 0) new_offset = 0;
    if (new_offset > max_offset) new_offset = max_offset;
    
    // Only update if changed
    if (new_offset != bmw->scroll_offset) {
        bmw->scroll_offset = new_offset;
        
        // Update visible labels
        for (int32_t i = 0; i < bmw->visible_lines; i++) {
            int32_t msg_index = bmw->scroll_offset + i;
            if (msg_index >= 0 && msg_index < bmw->message_count && bmw->message_labels[i]) {
                label_set_text(bmw->message_labels[i], bmw->messages[msg_index]);
                bmw->message_labels[i]->state_flags |= CONTROL_STATE_VISIBLE;
            } else if (bmw->message_labels[i]) {
                label_set_text(bmw->message_labels[i], "");
                bmw->message_labels[i]->state_flags &= ~CONTROL_STATE_VISIBLE;
            }
        }
    }
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
