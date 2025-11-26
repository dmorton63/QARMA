/*
 * QARMA - Boot Messages Window (New Architecture)
 * Displays system boot/initialization messages in a scrollable window
 */

#ifndef QARMA_BOOT_MESSAGES_H
#define QARMA_BOOT_MESSAGES_H

#include "kernel_types.h"
#include "gui/frame.h"
#include "gui/qarma_control.h"
#include "qarma_win_handle/qarma_input_events.h"

#define MAX_BOOT_MESSAGES 100
#define MAX_MESSAGE_LENGTH 120

/*
 * Boot Messages Window Structure
 * Uses qarma_frame_t container with qarma_control_t* label array for message lines
 */
typedef struct {
    qarma_frame_t* main_frame;           // Main window frame
    qarma_control_t* title_label;        // Window title
    qarma_control_t* close_button;       // Close button
    qarma_control_t* message_labels[MAX_BOOT_MESSAGES];  // Message line labels
    
    // Message data
    char messages[MAX_BOOT_MESSAGES][MAX_MESSAGE_LENGTH];
    int32_t message_count;
    int32_t scroll_offset;               // For scrolling
    int32_t visible_lines;               // How many lines can be displayed
    
    // State
    bool visible;
    bool active;
    
    // Callbacks
    void (*on_close)(void* user_data);
    void* close_user_data;
    
} boot_messages_window_t;

/*
 * Create boot messages window
 * Returns: boot_messages_window_t* or NULL on failure
 */
boot_messages_window_t* boot_messages_create(int32_t x, int32_t y, int32_t width, int32_t height);

/*
 * Destroy boot messages window
 */
void boot_messages_destroy(boot_messages_window_t* bmw);

/*
 * Add a message to the window
 * Automatically scrolls to show latest message
 */
void boot_messages_add(boot_messages_window_t* bmw, const char* message);

/*
 * Clear all messages
 */
void boot_messages_clear(boot_messages_window_t* bmw);

/*
 * Scroll messages
 * delta: positive = scroll down, negative = scroll up
 */
void boot_messages_scroll(boot_messages_window_t* bmw, int32_t delta);

/*
 * Render the window
 */
void boot_messages_render(boot_messages_window_t* bmw);

/*
 * Update window state (animations, etc.)
 */
void boot_messages_update(boot_messages_window_t* bmw);

/*
 * Set close callback
 */
void boot_messages_set_close_callback(boot_messages_window_t* bmw, 
                                      void (*callback)(void* user_data), 
                                      void* user_data);

/*
 * Handle input events
 */
void boot_messages_handle_event(boot_messages_window_t* bmw, QARMA_INPUT_EVENT* event);

/*
 * Set window visibility
 */
void boot_messages_set_visible(boot_messages_window_t* bmw, bool visible);

/*
 * Get window visibility
 */
bool boot_messages_is_visible(boot_messages_window_t* bmw);

#endif // QARMA_BOOT_MESSAGES_H
