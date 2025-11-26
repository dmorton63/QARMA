/*
 * QARMA - Console Window Implementation
 */

#include "console_window.h"
#include "renderer.h"
#include "qarma_win_handle/qarma_input_events.h"
#include "memory/heap.h"
#include "core/string.h"
#include "config.h"
#include "keyboard/command.h"

#define CONSOLE_BG_COLOR 0xFF000000
#define CONSOLE_TEXT_COLOR 0xFF00FF00
#define CONSOLE_BORDER_COLOR 0xFF00AA00
#define CONSOLE_TITLE_BG 0xFF003300
#define CONSOLE_PROMPT_COLOR 0xFF00FFFF
#define LINE_HEIGHT 14
#define CHAR_WIDTH 8
#define PADDING 10
#define TITLE_HEIGHT 24

// Capture output for console display
static ConsoleWindow* g_current_console = NULL;

// Callback for gfx_print redirection
static void console_capture_output(const char* text) {
    if (g_current_console && text) {
        // Strip newlines and add each line separately
        char line[CONSOLE_LINE_LENGTH];
        int line_pos = 0;
        
        for (int i = 0; text[i] != '\0'; i++) {
            if (text[i] == '\n') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    console_window_print(g_current_console, line);
                    line_pos = 0;
                } else {
                    console_window_print(g_current_console, "");
                }
            } else if (line_pos < CONSOLE_LINE_LENGTH - 1) {
                line[line_pos++] = text[i];
            }
        }
        
        // Add remaining text
        if (line_pos > 0) {
            line[line_pos] = '\0';
            console_window_print(g_current_console, line);
        }
    }
}

extern void gfx_set_console_callback(void (*callback)(const char*));

ConsoleWindow* console_window_create(int x, int y, int width, int height) {
    ConsoleWindow* cw = (ConsoleWindow*)heap_alloc(sizeof(ConsoleWindow));
    if (!cw) return NULL;
    
    cw->window = qarma_win_create(x, y, width, height);
    if (!cw->window) {
        heap_free(cw);
        return NULL;
    }
    
    cw->window->title = "QARMA Console";
    
    // Initialize console state
    for (int i = 0; i < CONSOLE_MAX_LINES; i++) {
        cw->lines[i][0] = '\0';
    }
    
    cw->line_count = 0;
    cw->scroll_offset = 0;
    cw->input_buffer[0] = '\0';
    cw->input_pos = 0;
    cw->cursor_pos = 0;
    cw->visible = false;
    
    // Add welcome message
    console_window_print(cw, "QARMA Console v1.0");
    console_window_print(cw, "Type 'help' for commands");
    console_window_print(cw, "");
    
    return cw;
}

void console_window_destroy(ConsoleWindow* cw) {
    if (!cw) return;
    
    if (cw->window) {
        // Free pixel buffer
        if (cw->window->pixel_buffer) {
            heap_free(cw->window->pixel_buffer);
        }
        heap_free(cw->window);
    }
    
    heap_free(cw);
}

void console_window_set_visible(ConsoleWindow* cw, bool visible) {
    if (cw) {
        cw->visible = visible;
    }
}

void console_window_print(ConsoleWindow* cw, const char* text) {
    if (!cw || !text) return;
    
    // Scroll if at capacity
    if (cw->line_count >= CONSOLE_MAX_LINES) {
        for (int i = 0; i < CONSOLE_MAX_LINES - 1; i++) {
            strcpy(cw->lines[i], cw->lines[i + 1]);
        }
        cw->line_count = CONSOLE_MAX_LINES - 1;
    }
    
    // Add new line
    strncpy(cw->lines[cw->line_count], text, CONSOLE_LINE_LENGTH - 1);
    cw->lines[cw->line_count][CONSOLE_LINE_LENGTH - 1] = '\0';
    cw->line_count++;
}

void console_window_execute_command(ConsoleWindow* cw, const char* command) {
    if (!cw || !command || command[0] == '\0') return;
    
    // Echo command
    char echo[CONSOLE_LINE_LENGTH];
    snprintf(echo, CONSOLE_LINE_LENGTH, "> %s", command);
    console_window_print(cw, echo);
    
    // Handle built-in console commands
    if (strcmp(command, "clear") == 0) {
        cw->line_count = 0;
        cw->scroll_offset = 0;
        return;
    }
    else if (strcmp(command, "exit") == 0) {
        console_window_set_visible(cw, false);
        return;
    }
    
    // Set global console for output capture
    g_current_console = cw;
    gfx_set_console_callback(console_capture_output);
    
    // Execute using the real command system
    command_result_t result = execute_command(command);
    
    if (result == CMD_ERROR_UNKNOWN_COMMAND) {
        console_window_print(cw, "Unknown command. Type 'help' for available commands.");
    }
    
    // Clear global console and callback
    gfx_set_console_callback(NULL);
    g_current_console = NULL;
    
    console_window_print(cw, "");
}

void console_window_handle_event(ConsoleWindow* cw, QARMA_INPUT_EVENT* event) {
    if (!cw || !event || !cw->visible) return;
    
    if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        uint8_t scancode = event->data.key.scancode;
        char c = event->data.key.character;
        
        // ESC - close console
        if (scancode == 0x01) {
            console_window_set_visible(cw, false);
            return;
        }
        
        // Enter - execute command
        if (scancode == 0x1C) {
            cw->input_buffer[cw->input_pos] = '\0';
            console_window_execute_command(cw, cw->input_buffer);
            cw->input_buffer[0] = '\0';
            cw->input_pos = 0;
            cw->cursor_pos = 0;
            return;
        }
        
        // Backspace
        if (scancode == 0x0E && cw->input_pos > 0) {
            cw->input_pos--;
            cw->cursor_pos--;
            cw->input_buffer[cw->input_pos] = '\0';
            return;
        }
        
        // Printable characters
        if (c >= 32 && c <= 126 && cw->input_pos < CONSOLE_INPUT_LENGTH - 1) {
            cw->input_buffer[cw->input_pos] = c;
            cw->input_pos++;
            cw->cursor_pos++;
            cw->input_buffer[cw->input_pos] = '\0';
        }
    }
}

void console_window_update(ConsoleWindow* cw) {
    if (!cw) return;
    // Nothing to update for now
}

void console_window_render(ConsoleWindow* cw) {
    if (!cw || !cw->window || !cw->visible) return;
    
    uint32_t* buffer = cw->window->pixel_buffer;
    int width = cw->window->size.width;
    int height = cw->window->size.height;
    
    // Draw background
    draw_filled_rect(buffer, width, 0, 0, width, height, CONSOLE_BG_COLOR);
    
    // Draw title bar
    draw_filled_rect(buffer, width, 0, 0, width, TITLE_HEIGHT, CONSOLE_TITLE_BG);
    draw_string_to_buffer(buffer, width, 10, 5, "QARMA Console [ESC to close]", CONSOLE_PROMPT_COLOR);
    
    // Draw border
    draw_rect_border(buffer, width, 0, 0, width, height, CONSOLE_BORDER_COLOR, 2);
    
    // Calculate visible area
    int content_y = TITLE_HEIGHT + PADDING;
    int content_height = height - TITLE_HEIGHT - PADDING * 2 - LINE_HEIGHT - 10;
    int max_visible_lines = content_height / LINE_HEIGHT;
    
    // Draw output lines
    int y = content_y;
    int start_line = cw->line_count > max_visible_lines ? cw->line_count - max_visible_lines : 0;
    
    for (int i = start_line; i < cw->line_count && y < height - LINE_HEIGHT - 20; i++) {
        draw_string_to_buffer(buffer, width, PADDING, y, cw->lines[i], CONSOLE_TEXT_COLOR);
        y += LINE_HEIGHT;
    }
    
    // Draw prompt and input line
    int input_y = height - LINE_HEIGHT - 10;
    draw_string_to_buffer(buffer, width, PADDING, input_y, "> ", CONSOLE_PROMPT_COLOR);
    
    if (cw->input_pos > 0) {
        draw_string_to_buffer(buffer, width, PADDING + 16, input_y, cw->input_buffer, CONSOLE_TEXT_COLOR);
    }
    
    // Draw cursor (blinking effect based on tick)
    extern uint32_t get_ticks(void);
    if ((get_ticks() / 30) % 2 == 0) {
        int cursor_x = PADDING + 16 + (cw->cursor_pos * CHAR_WIDTH);
        draw_filled_rect(buffer, width, cursor_x, input_y, 8, LINE_HEIGHT, CONSOLE_TEXT_COLOR);
    }
}
