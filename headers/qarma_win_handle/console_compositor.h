/**
 * QARMA - Console Window for Compositor
 * 
 * A windowed console that integrates with the window compositor system.
 */

#ifndef CONSOLE_COMPOSITOR_H
#define CONSOLE_COMPOSITOR_H

#include "core/stdtools.h"

/**
 * Initialize the console compositor window
 */
void console_compositor_init(void);

/**
 * Show console window
 */
void console_compositor_show(void);

/**
 * Hide console window
 */
void console_compositor_hide(void);

/**
 * Toggle console visibility (show/hide)
 */
void console_compositor_toggle(void);

/**
 * Check if console is visible
 */
bool console_compositor_is_visible(void);

/**
 * Get console window handle for event targeting
 */
struct compositor_window_t* console_compositor_get_window(void);

/**
 * Handle keyboard input for console
 */
void console_compositor_handle_key(uint8_t scancode, char character);

/**
 * Print text to console
 */
void console_compositor_print(const char* text);

/**
 * Get scroll offset (0 = at bottom, positive = scrolled up)
 */
int console_compositor_get_scroll_offset(void);

/**
 * Set scroll offset (for scrollbar integration)
 */
void console_compositor_set_scroll_offset(int offset);

/**
 * Get total number of lines in buffer
 */
int console_compositor_get_line_count(void);

/**
 * Get maximum number of visible lines
 */
int console_compositor_get_visible_lines(void);

#endif // CONSOLE_COMPOSITOR_H
