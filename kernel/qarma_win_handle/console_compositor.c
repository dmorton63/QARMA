/**
 * QARMA - Console Window for Compositor
 * 
 * A windowed console that integrates with the window compositor system.
 */

#include "window_compositor.h"
#include "graphics/graphics.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "keyboard/command.h"

#define CONSOLE_MAX_LINES 100
#define CONSOLE_LINE_LENGTH 80
#define CONSOLE_INPUT_LENGTH 80

typedef struct {
    compositor_window_t* window;
    char lines[CONSOLE_MAX_LINES][CONSOLE_LINE_LENGTH];
    int line_count;
    int scroll_offset;
    char input_buffer[CONSOLE_INPUT_LENGTH];
    int input_pos;
    bool capturing_output;
} console_compositor_t;

static console_compositor_t* g_console = NULL;

// Callback for gfx_print redirection
static void console_capture_output(const char* text) {
    if (!g_console || !text) return;
    
    // Parse text into lines
    char line[CONSOLE_LINE_LENGTH];
    int line_pos = 0;
    
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            if (line_pos > 0) {
                line[line_pos] = '\0';
                // Add line to console
                if (g_console->line_count >= CONSOLE_MAX_LINES) {
                    // Scroll up
                    for (int j = 0; j < CONSOLE_MAX_LINES - 1; j++) {
                        strcpy(g_console->lines[j], g_console->lines[j + 1]);
                    }
                    g_console->line_count = CONSOLE_MAX_LINES - 1;
                }
                strncpy(g_console->lines[g_console->line_count], line, CONSOLE_LINE_LENGTH - 1);
                g_console->lines[g_console->line_count][CONSOLE_LINE_LENGTH - 1] = '\0';
                g_console->line_count++;
                line_pos = 0;
            }
        } else if (line_pos < CONSOLE_LINE_LENGTH - 1) {
            line[line_pos++] = text[i];
        }
    }
    
    // Add any remaining text
    if (line_pos > 0) {
        line[line_pos] = '\0';
        if (g_console->line_count >= CONSOLE_MAX_LINES) {
            for (int j = 0; j < CONSOLE_MAX_LINES - 1; j++) {
                strcpy(g_console->lines[j], g_console->lines[j + 1]);
            }
            g_console->line_count = CONSOLE_MAX_LINES - 1;
        }
        strncpy(g_console->lines[g_console->line_count], line, CONSOLE_LINE_LENGTH - 1);
        g_console->lines[g_console->line_count][CONSOLE_LINE_LENGTH - 1] = '\0';
        g_console->line_count++;
    }
}

// Console content renderer
void console_render_content(QARMA_WIN_HANDLE* win, int x, int y, int w, int h) {
    (void)win;
    
    if (!g_console) return;
    
    rgb_color_t text_color = {0, 255, 0, 255};  // Green text
    rgb_color_t prompt_color = {0, 255, 255, 255};  // Cyan prompt
    rgb_color_t bg = {0, 0, 0, 255};  // Black background
    
    // Fill background
    gfx_draw_filled_rectangle(x, y, w, h, bg);
    
    // Calculate how many lines we can display
    int line_height = 12;
    int max_visible_lines = (h - 30) / line_height;  // Reserve space for input line
    int start_line = (g_console->line_count > max_visible_lines) 
                     ? g_console->line_count - max_visible_lines 
                     : 0;
    
    // Render output lines
    int text_y = y + 5;
    for (int i = start_line; i < g_console->line_count && text_y < y + h - 25; i++) {
        gfx_draw_string(x + 5, text_y, g_console->lines[i], text_color, bg, NULL);
        text_y += line_height;
    }
    
    // Render input prompt and buffer at bottom
    int input_y = y + h - 20;
    gfx_draw_string(x + 5, input_y, "> ", prompt_color, bg, NULL);
    
    if (g_console->input_pos > 0) {
        gfx_draw_string(x + 20, input_y, g_console->input_buffer, text_color, bg, NULL);
    }
    
    // Draw cursor (blinking)
    extern uint32_t get_ticks(void);
    if ((get_ticks() / 30) % 2 == 0) {
        int cursor_x = x + 20 + (g_console->input_pos * 8);
        rgb_color_t cursor_color = {0, 255, 0, 255};
        gfx_draw_filled_rectangle(cursor_x, input_y, 8, 12, cursor_color);
    }
}

// Get console window handle for event targeting
compositor_window_t* console_compositor_get_window(void) {
    return g_console ? g_console->window : NULL;
}

void console_compositor_init(void) {
    extern void serial_debug(const char* msg);
    serial_debug("[CONSOLE] console_compositor_init called\n");
    
    if (g_console) {
        serial_debug("[CONSOLE] Already initialized\n");
        return;  // Already initialized
    }
    
    g_console = (console_compositor_t*)heap_alloc(sizeof(console_compositor_t));
    if (!g_console) {
        serial_debug("[CONSOLE] ERROR: heap_alloc failed\n");
        return;
    }
    
    memset(g_console, 0, sizeof(console_compositor_t));
    
    // Create console window (centered, 70% of screen width/height)
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    
    int width = (fb_width * 7) / 10;
    int height = (fb_height * 7) / 10;
    int x = (fb_width - width) / 2;
    int y = (fb_height - height) / 2;
    
    serial_debug("[CONSOLE] Creating window\n");
    
    g_console->window = compositor_create_window("QARMA Console [ESC to close]", x, y, width, height);
    if (!g_console->window) {
        serial_debug("[CONSOLE] ERROR: compositor_create_window failed\n");
        heap_free(g_console);
        g_console = NULL;
        return;
    }
    
    serial_debug("[CONSOLE] Window created successfully\n");
    
    // Set content renderer
    g_console->window->on_render_content = console_render_content;
    
    // Add welcome message
    strcpy(g_console->lines[0], "QARMA Console v1.0");
    strcpy(g_console->lines[1], "Type 'help' for available commands");
    strcpy(g_console->lines[2], "Press 'T' to toggle console, ESC to close");
    strcpy(g_console->lines[3], "");
    g_console->line_count = 4;
    
    // Start VISIBLE - console is shown on boot by default
    g_console->window->base.flags |= QARMA_FLAG_VISIBLE;
    
    // Set console callback permanently to capture all gfx_print() output
    extern void gfx_set_console_callback(void (*callback)(const char*));
    gfx_set_console_callback(console_capture_output);
    
    serial_debug("[CONSOLE] Initialization complete, window VISIBLE\n");
}

void console_compositor_show(void) {
    extern void serial_debug(const char* msg);
    
    if (!g_console || !g_console->window) {
        serial_debug("[CONSOLE_SHOW] ERROR: console or window is NULL\n");
        return;
    }
    
    serial_debug("[CONSOLE_SHOW] Setting VISIBLE flag\n");
    
    // Show console and focus it
    g_console->window->base.flags |= QARMA_FLAG_VISIBLE;
    compositor_focus_window(g_console->window);
    
    // Set keyboard focus to console window for proper event targeting
    extern void keyboard_set_focus(void* target);
    keyboard_set_focus(g_console->window);
    
    // Full render to show
    compositor_render_all();
    
    serial_debug("[CONSOLE_SHOW] Console shown and rendered\n");
}

void console_compositor_hide(void) {
    if (!g_console || !g_console->window) return;
    
    // Hide console
    g_console->window->base.flags &= ~QARMA_FLAG_VISIBLE;
    
    // Clear keyboard focus when hiding console
    extern void keyboard_set_focus(void* target);
    keyboard_set_focus(NULL);
    
    // Full render to hide
    compositor_render_all();
}

void console_compositor_toggle(void) {
    if (!g_console || !g_console->window) return;
    
    if (g_console->window->base.flags & QARMA_FLAG_VISIBLE) {
        console_compositor_hide();
    } else {
        console_compositor_show();
    }
}

bool console_compositor_is_visible(void) {
    static int check_count = 0;
    
    if (++check_count <= 10) {
        extern void serial_debug(const char* msg);
        serial_debug("[CONSOLE_VISIBLE_CHECK] g_console=");
        SERIAL_LOG_HEX("", (uint32_t)g_console);
        
        if (g_console) {
            serial_debug(" window=");
            SERIAL_LOG_HEX("", (uint32_t)g_console->window);
            
            if (g_console->window) {
                serial_debug(" flags=");
                SERIAL_LOG_HEX("", g_console->window->base.flags);
                serial_debug(" VISIBLE_FLAG=");
                SERIAL_LOG_HEX("", QARMA_FLAG_VISIBLE);
            }
        }
        serial_debug("\n");
    }
    
    bool visible = g_console && g_console->window && 
                   (g_console->window->base.flags & QARMA_FLAG_VISIBLE);
    
    if (check_count <= 10) {
        extern void serial_debug(const char* msg);
        serial_debug("[CONSOLE_VISIBLE_CHECK] Result: ");
        serial_debug(visible ? "YES\n" : "NO\n");
    }
    
    return visible;
}

void console_compositor_handle_key(uint8_t scancode, char character) {
    extern void serial_debug(const char* msg);
    serial_debug("[CONSOLE_KEY] Handle key called\n");
    
    if (!g_console || !console_compositor_is_visible()) {
        serial_debug("[CONSOLE_KEY] Console not visible or NULL\n");
        return;
    }
    
    serial_debug("[CONSOLE_KEY] Processing key\n");
    
    // ESC - close console
    if (scancode == 0x01) {
        console_compositor_toggle();
        return;
    }
    
    // Enter - execute command
    if (scancode == 0x1C) {
        g_console->input_buffer[g_console->input_pos] = '\0';
        
        // Echo command
        char echo[CONSOLE_LINE_LENGTH];
        snprintf(echo, CONSOLE_LINE_LENGTH, "> %s", g_console->input_buffer);
        
        if (g_console->line_count >= CONSOLE_MAX_LINES) {
            for (int j = 0; j < CONSOLE_MAX_LINES - 1; j++) {
                strcpy(g_console->lines[j], g_console->lines[j + 1]);
            }
            g_console->line_count = CONSOLE_MAX_LINES - 1;
        }
        strcpy(g_console->lines[g_console->line_count++], echo);
        
        // Handle built-in commands
        if (strcmp(g_console->input_buffer, "clear") == 0) {
            g_console->line_count = 0;
        } else if (strcmp(g_console->input_buffer, "exit") == 0) {
            console_compositor_toggle();
        } else {
            // Execute command with output capture (callback already set during init)
            g_console->capturing_output = true;
            
            extern command_result_t execute_command(const char* cmd);
            command_result_t result = execute_command(g_console->input_buffer);
            
            if (result == CMD_ERROR_UNKNOWN_COMMAND) {
                if (g_console->line_count < CONSOLE_MAX_LINES) {
                    strcpy(g_console->lines[g_console->line_count++], 
                           "Unknown command. Type 'help' for available commands.");
                }
            }
            
            g_console->capturing_output = false;
        }
        
        // Add blank line
        if (g_console->line_count < CONSOLE_MAX_LINES) {
            g_console->lines[g_console->line_count++][0] = '\0';
        }
        
        // Clear input buffer
        g_console->input_buffer[0] = '\0';
        g_console->input_pos = 0;
        
        // Re-render console
        compositor_render_all();
        return;
    }
    
    // Backspace
    if (scancode == 0x0E && g_console->input_pos > 0) {
        g_console->input_pos--;
        g_console->input_buffer[g_console->input_pos] = '\0';
        compositor_render_all();
        return;
    }
    
    // Printable characters
    if (character >= 32 && character <= 126 && g_console->input_pos < CONSOLE_INPUT_LENGTH - 1) {
        g_console->input_buffer[g_console->input_pos++] = character;
        g_console->input_buffer[g_console->input_pos] = '\0';
        compositor_render_all();
    }
}

void console_compositor_print(const char* text) {
    if (!g_console) return;
    
    console_capture_output(text);
    
    if (console_compositor_is_visible()) {
        compositor_render_all();
    }
}
