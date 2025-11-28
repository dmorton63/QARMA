#pragma once

#include "control_base.h"
#include "qarma_input_events.h"

// ============================================================================
// Scrollbar - Horizontal and Vertical scroll controls with draggable thumb
// ============================================================================

typedef enum {
    SCROLLBAR_HORIZONTAL,
    SCROLLBAR_VERTICAL
} ScrollbarOrientation;

typedef struct {
    ControlBase base;
    ScrollbarOrientation orientation;
    
    // Scroll state
    int min_value;
    int max_value;
    int current_value;
    int page_size;         // Size of visible area (affects thumb size)
    
    // Visual state
    bool thumb_hovered;
    bool thumb_pressed;
    bool thumb_focused;
    int thumb_drag_offset;  // Offset from thumb start when dragging
    
    // Thumb bounds (calculated)
    int thumb_pos;          // Position along track
    int thumb_size;         // Size of thumb
    int track_size;         // Size of scrollable area
    
    // Callbacks
    void (*on_scroll)(void* userdata, int new_value);
    void* userdata;
} Scrollbar;

// ============================================================================
// Scrollbar API
// ============================================================================

void scrollbar_init(Scrollbar* sb, int x, int y, int length, ScrollbarOrientation orientation);
void scrollbar_render(Scrollbar* sb, uint32_t* buffer, int buf_width, int buf_height);
bool scrollbar_handle_event(Scrollbar* sb, QARMA_INPUT_EVENT* event);

void scrollbar_set_range(Scrollbar* sb, int min_val, int max_val, int page_size);
void scrollbar_set_value(Scrollbar* sb, int value);
int scrollbar_get_value(Scrollbar* sb);
void scrollbar_scroll_by(Scrollbar* sb, int delta);  // Scroll by delta amount

