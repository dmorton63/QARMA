/*
 * QARMA - Status Bar Control (New Architecture)
 * 
 * A status bar using qarma_control_t* arrays
 */

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include "frame.h"
#include "qarma_control.h"
#include "controls/qarma_button.h"
#include "controls/qarma_label.h"
#include "kernel_types.h"

#define STATUS_BAR_HEIGHT 32
#define STATUS_BAR_MAX_ITEMS 16

// Status bar item types
typedef enum {
    STATUS_ITEM_BUTTON,
    STATUS_ITEM_LABEL,
    STATUS_ITEM_SPACER
} status_item_type_t;

// Status bar alignment
typedef enum {
    STATUS_ALIGN_LEFT,
    STATUS_ALIGN_CENTER,
    STATUS_ALIGN_RIGHT
} status_alignment_t;

// Status bar item descriptor
typedef struct {
    status_item_type_t type;
    status_alignment_t alignment;
    bool visible;
    int width;
    int x_offset;  // Calculated during layout
    qarma_control_t* control;  // Points to button or label control
    void (*on_click)(void* user_data);
    void* user_data;
} status_bar_item_t;

// Status bar structure
typedef struct status_bar_t {
    qarma_frame_t* main_frame;  // Container frame
    
    status_bar_item_t items[STATUS_BAR_MAX_ITEMS];
    int item_count;
    
    int focused_item;  // -1 if none focused
    
    uint32_t bg_color;
    uint32_t border_color;
} status_bar_t;

/**
 * Create a status bar
 */
status_bar_t* status_bar_create(int x, int y, int width, int height);

/**
 * Destroy the status bar
 */
void status_bar_destroy(status_bar_t* bar);

/**
 * Add a button to the status bar
 */
int status_bar_add_button(status_bar_t* bar, const char* text, status_alignment_t align,
                          void (*on_click)(void* user_data), void* user_data);

/**
 * Add a label to the status bar
 */
int status_bar_add_label(status_bar_t* bar, const char* text, status_alignment_t align);

/**
 * Add a spacer (empty space)
 */
int status_bar_add_spacer(status_bar_t* bar, int width, status_alignment_t align);

/**
 * Remove an item
 */
void status_bar_remove_item(status_bar_t* bar, int item_index);

/**
 * Update label text
 */
void status_bar_update_label_text(status_bar_t* bar, int item_index, const char* new_text);

/**
 * Update button text
 */
void status_bar_update_button_text(status_bar_t* bar, int item_index, const char* new_text);

/**
 * Show/hide the status bar
 */
void status_bar_set_visible(status_bar_t* bar, bool visible);

/**
 * Handle input events
 */
void status_bar_handle_event(status_bar_t* bar, qarma_message_t* msg);

/**
 * Render the status bar
 */
void status_bar_render(status_bar_t* bar);

/**
 * Recalculate item positions
 */
void status_bar_layout(status_bar_t* bar);

#endif // STATUS_BAR_H
