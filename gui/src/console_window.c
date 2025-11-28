/*
 * QARMA - Console Window Implementation (New Architecture)
 */

#include "console_window.h"
#include "renderer.h"
#include "memory/heap.h"
#include "core/string.h"
#include "core/kernel.h"
#include "core/handle_manager.h"
#include "config.h"
#include "keyboard/command.h"
#include "controls/qarma_textbox.h"
#include "qarma_win_handle/window_compositor.h"

#define CONSOLE_BG_COLOR 0xFF000000
#define CONSOLE_TEXT_COLOR 0xFF00FF00
#define CONSOLE_BORDER_COLOR 0xFF00AA00
#define CONSOLE_TITLE_BG 0xFF003300
#define CONSOLE_PROMPT_COLOR 0xFF00FFFF
#define PADDING 10
#define TITLE_HEIGHT 24

// Message handler for console frame
static int32_t console_frame_message_handler(qarma_handle_t recipient, qarma_message_t* msg) {
    if (!msg) return 0;
    
    // Get frame from handle
    qarma_frame_t* frame = (qarma_frame_t*)handle_get_object(recipient);
    if (!frame) return 0;
    
    console_window_t* cw = (console_window_t*)frame->user_data;
    if (!cw) return 0;
    
    switch (msg->type) {
        case MSG_PAINT:
            console_window_render(cw);
            return 1;
            
        case MSG_KEYDOWN:
        case MSG_CHAR: {
            QARMA_INPUT_EVENT event = {0};
            event.type = (msg->type == MSG_KEYDOWN) ? QARMA_INPUT_EVENT_KEY_DOWN : QARMA_INPUT_EVENT_KEY_PRESS;
            event.timestamp = 0; // TODO: Add timestamp
            event.data.key.scancode = (uint8_t)(msg->wparam & 0xFF);
            event.data.key.keycode = event.data.key.scancode;
            event.data.key.character = (char)(msg->lparam & 0xFF);
            event.data.key.modifiers = (msg->wparam >> 8) & 0xFF;
            
            console_window_handle_event(cw, &event);
            return 1;
        }
        
        case MSG_LBUTTONDOWN:
        case MSG_LBUTTONUP:
        case MSG_MOUSEMOVE:
            // Handle mouse events if needed
            return 1;
            
        default:
            return 0;
    }
}

console_window_t* console_window_create(int x, int y, int width, int height) {
    SERIAL_LOG("[CONSOLE_NEW] Creating console window\n");
    
    console_window_t* cw = (console_window_t*)heap_alloc(sizeof(console_window_t));
    if (!cw) {
        SERIAL_LOG("[CONSOLE_NEW] ERROR: Failed to allocate console_window_t\n");
        return NULL;
    }
    
    // Initialize state
    memset(cw, 0, sizeof(console_window_t));
    cw->line_count = 0;
    cw->scroll_offset = 0;
    cw->visible = false;
    cw->history_count = 0;
    cw->history_pos = -1;
    
    // Create main frame
    cw->main_frame = frame_create(
        NULL,  // No parent
        x, y, width, height,
        FRAME_STYLE_BORDER | FRAME_STYLE_TITLE_BAR,
        "QARMA Console"
    );
    if (!cw->main_frame) {
        SERIAL_LOG("[CONSOLE_NEW] ERROR: Failed to create frame\n");
        heap_free(cw);
        return NULL;
    }
    
    // Set frame colors
    cw->main_frame->background.red = 0;
    cw->main_frame->background.green = 0;
    cw->main_frame->background.blue = 0;
    cw->main_frame->background.alpha = 255;
    
    cw->main_frame->border_color.red = 0;
    cw->main_frame->border_color.green = 170;
    cw->main_frame->border_color.blue = 0;
    cw->main_frame->border_color.alpha = 255;
    
    // Set message handler
    cw->main_frame->message_handler = console_frame_message_handler;
    cw->main_frame->user_data = cw;
    
    // Create output textbox (multi-line, read-only)
    int output_height = height - TITLE_HEIGHT - 40 - (PADDING * 2);
    cw->output_textbox = textbox_create(
        cw->main_frame,
        "console_output",
        PADDING,
        TITLE_HEIGHT + PADDING,
        width - (PADDING * 2),
        output_height
    );
    
    if (cw->output_textbox) {
        textbox_data_t* output = (textbox_data_t*)cw->output_textbox->implementation_data;
        if (output) {
            // Set properties via control state
            cw->output_textbox->state_flags |= CONTROL_STATE_READONLY;
            // TODO: Add multiline support to textbox_data_t
        }
        frame_add_control(cw->main_frame, cw->output_textbox);
    }
    
    // Create input textbox (single-line, editable)
    cw->input_textbox = textbox_create(
        cw->main_frame,
        "console_input",
        PADDING,
        height - 40,
        width - (PADDING * 2),
        30
    );
    
    if (cw->input_textbox) {
        textbox_data_t* input = (textbox_data_t*)cw->input_textbox->implementation_data;
        if (input) {
            input->max_length = CONSOLE_INPUT_LENGTH - 1;
        }
        frame_add_control(cw->main_frame, cw->input_textbox);
    }
    
    // Add welcome messages
    console_window_print(cw, "QARMA Console v2.0 (New Architecture)");
    console_window_print(cw, "Type 'help' for commands");
    console_window_print(cw, "");
    
    SERIAL_LOG("[CONSOLE_NEW] Console window created successfully\n");
    return cw;
}

void console_window_destroy(console_window_t* cw) {
    if (!cw) return;
    
    SERIAL_LOG("[CONSOLE_NEW] Destroying console window\n");
    
    // Controls are owned by frame and will be destroyed with it
    if (cw->main_frame) {
        frame_destroy(cw->main_frame);
    }
    
    heap_free(cw);
}

void console_window_set_visible(console_window_t* cw, bool visible) {
    if (!cw || !cw->main_frame) return;
    
    cw->visible = visible;
    cw->main_frame->visible = visible;
}

void console_window_print(console_window_t* cw, const char* text) {
    if (!cw || !text) return;
    
    // Add to line buffer
    if (cw->line_count < CONSOLE_MAX_LINES) {
        strncpy(cw->lines[cw->line_count], text, CONSOLE_LINE_LENGTH - 1);
        cw->lines[cw->line_count][CONSOLE_LINE_LENGTH - 1] = '\0';
        cw->line_count++;
    } else {
        // Scroll up - shift all lines
        for (int i = 0; i < CONSOLE_MAX_LINES - 1; i++) {
            strcpy(cw->lines[i], cw->lines[i + 1]);
        }
        strncpy(cw->lines[CONSOLE_MAX_LINES - 1], text, CONSOLE_LINE_LENGTH - 1);
        cw->lines[CONSOLE_MAX_LINES - 1][CONSOLE_LINE_LENGTH - 1] = '\0';
    }
    
    // Update output textbox
    console_window_update(cw);
}

void console_window_update(console_window_t* cw) {
    if (!cw || !cw->output_textbox) return;
    
    // Build combined output text
    static char output_buffer[CONSOLE_MAX_LINES * CONSOLE_LINE_LENGTH];
    output_buffer[0] = '\0';
    
    int start_line = (cw->line_count > 30) ? (cw->line_count - 30) : 0;
    
    for (int i = start_line; i < cw->line_count; i++) {
        strcat(output_buffer, cw->lines[i]);
        strcat(output_buffer, "\n");
    }
    
    // Set text in output textbox
    textbox_set_text(cw->output_textbox, output_buffer);
}

void console_window_handle_event(console_window_t* cw, QARMA_INPUT_EVENT* event) {
    if (!cw || !event || !cw->input_textbox) return;
    
    if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        // Check for Enter key
        if (event->data.key.scancode == 0x1C) { // Enter
            console_window_execute_command(cw);
            return;
        }
        
        // Check for Up/Down arrows for history
        if (event->data.key.scancode == 0x48) { // Up arrow
            if (cw->history_count > 0) {
                if (cw->history_pos < cw->history_count - 1) {
                    cw->history_pos++;
                    int idx = cw->history_count - 1 - cw->history_pos;
                    textbox_set_text(cw->input_textbox, cw->history[idx]);
                }
            }
            return;
        }
        
        if (event->data.key.scancode == 0x50) { // Down arrow
            if (cw->history_pos > 0) {
                cw->history_pos--;
                int idx = cw->history_count - 1 - cw->history_pos;
                textbox_set_text(cw->input_textbox, cw->history[idx]);
            } else if (cw->history_pos == 0) {
                cw->history_pos = -1;
                textbox_set_text(cw->input_textbox, "");
            }
            return;
        }
    }
    
    // Forward event to input textbox as message
    qarma_message_t msg = {0};
    msg.target = cw->input_textbox->handle;
    msg.type = (event->type == QARMA_INPUT_EVENT_KEY_DOWN) ? MSG_KEYDOWN : MSG_CHAR;
    msg.wparam = event->data.key.scancode | (event->data.key.modifiers << 8);
    msg.lparam = event->data.key.character;
    
    control_send_message(cw->input_textbox, &msg);
}

void console_window_execute_command(console_window_t* cw) {
    if (!cw || !cw->input_textbox) return;
    
    textbox_data_t* input = (textbox_data_t*)cw->input_textbox->implementation_data;
    if (!input || input->text[0] == '\0') return;
    
    // Echo command
    char echo_line[CONSOLE_LINE_LENGTH];
    snprintf(echo_line, CONSOLE_LINE_LENGTH, "> %s", input->text);
    console_window_print(cw, echo_line);
    
    // Add to history
    if (cw->history_count < 20) {
        strncpy(cw->history[cw->history_count], input->text, CONSOLE_INPUT_LENGTH - 1);
        cw->history[cw->history_count][CONSOLE_INPUT_LENGTH - 1] = '\0';
        cw->history_count++;
    } else {
        // Shift history
        for (int i = 0; i < 19; i++) {
            strcpy(cw->history[i], cw->history[i + 1]);
        }
        strncpy(cw->history[19], input->text, CONSOLE_INPUT_LENGTH - 1);
        cw->history[19][CONSOLE_INPUT_LENGTH - 1] = '\0';
    }
    cw->history_pos = -1;
    
    // Execute command
    extern command_result_t execute_command(const char* cmd);
    execute_command(input->text);
    
    // Clear input
    textbox_clear(cw->input_textbox);
}

void console_window_render(console_window_t* cw) {
    if (!cw || !cw->main_frame || !cw->visible) return;
    
    // Render frame (which will render all child controls)
    frame_render(cw->main_frame);
}
