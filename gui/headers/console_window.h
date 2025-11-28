/*
 * QARMA - Console Window Control (New Architecture)
 * 
 * A terminal/console window using qarma_control_t* arrays
 */

#ifndef CONSOLE_WINDOW_H
#define CONSOLE_WINDOW_H

#include "frame.h"
#include "qarma_control.h"
#include "controls/controls/qarma_textbox.h"
#include "qarma_input_events.h"
#include "kernel_types.h"

#define CONSOLE_MAX_LINES 50
#define CONSOLE_LINE_LENGTH 80
#define CONSOLE_INPUT_LENGTH 80

// Console window structure using new architecture
typedef struct console_window_t {
    qarma_frame_t* main_frame;          // Container frame
    qarma_control_t* output_textbox;    // Multi-line output display
    qarma_control_t* input_textbox;     // Command input line
    
    // Console state
    char lines[CONSOLE_MAX_LINES][CONSOLE_LINE_LENGTH];
    int line_count;
    int scroll_offset;
    bool visible;
    
    // Command history
    char history[20][CONSOLE_INPUT_LENGTH];
    int history_count;
    int history_pos;
} console_window_t;

/**
 * Create a console window with new architecture
 */
console_window_t* console_window_create(int x, int y, int width, int height);

/**
 * Destroy the console window
 */
void console_window_destroy(console_window_t* cw);

/**
 * Show/hide the console window
 */
void console_window_set_visible(console_window_t* cw, bool visible);

/**
 * Add text output to console
 */
void console_window_print(console_window_t* cw, const char* text);

/**
 * Handle input events (message-driven)
 */
void console_window_handle_event(console_window_t* cw, QARMA_INPUT_EVENT* event);

/**
 * Update the console display
 */
void console_window_update(console_window_t* cw);

/**
 * Render the console window
 */
void console_window_render(console_window_t* cw);

/**
 * Execute command from input buffer
 */
void console_window_execute_command(console_window_t* cw);

#endif // CONSOLE_WINDOW_H
