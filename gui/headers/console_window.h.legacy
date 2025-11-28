/*
 * QARMA - Console Window Control
 * 
 * A terminal/console window for command input and output
 */

#ifndef CONSOLE_WINDOW_H
#define CONSOLE_WINDOW_H

#include "qarma_win_handle/qarma_win_handle.h"
#include "kernel_types.h"

#define CONSOLE_MAX_LINES 50
#define CONSOLE_LINE_LENGTH 80
#define CONSOLE_INPUT_LENGTH 80

typedef struct ConsoleWindow {
    QARMA_WIN_HANDLE* window;
    char lines[CONSOLE_MAX_LINES][CONSOLE_LINE_LENGTH];
    int line_count;
    int scroll_offset;
    char input_buffer[CONSOLE_INPUT_LENGTH];
    int input_pos;
    int cursor_pos;
    bool visible;
} ConsoleWindow;

/**
 * Create a console window
 */
ConsoleWindow* console_window_create(int x, int y, int width, int height);

/**
 * Destroy the console window
 */
void console_window_destroy(ConsoleWindow* cw);

/**
 * Show/hide the console window
 */
void console_window_set_visible(ConsoleWindow* cw, bool visible);

/**
 * Handle keyboard input
 */
void console_window_handle_event(ConsoleWindow* cw, QARMA_INPUT_EVENT* event);

/**
 * Update the console window
 */
void console_window_update(ConsoleWindow* cw);

/**
 * Render the console window
 */
void console_window_render(ConsoleWindow* cw);

/**
 * Add a line of output to the console
 */
void console_window_print(ConsoleWindow* cw, const char* text);

/**
 * Execute a command
 */
void console_window_execute_command(ConsoleWindow* cw, const char* command);

#endif // CONSOLE_WINDOW_H
