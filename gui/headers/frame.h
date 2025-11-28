/**
 * QARMA - Frame System
 * 
 * Base frame/container system. All UI elements must be contained within a frame.
 * Frames provide containment, clipping, layout management, and event routing.
 */

#ifndef FRAME_H
#define FRAME_H

#include "handle_manager.h"
#include "message_system.h"

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct qarma_frame qarma_frame_t;
typedef struct qarma_control qarma_control_t;

// ============================================================================
// Frame Style and Properties
// ============================================================================

typedef enum {
    FRAME_STYLE_NONE        = 0x00,
    FRAME_STYLE_BORDER      = 0x01,     // Has border
    FRAME_STYLE_TITLE_BAR   = 0x02,     // Has title bar
    FRAME_STYLE_RESIZABLE   = 0x04,     // Can be resized
    FRAME_STYLE_MOVABLE     = 0x08,     // Can be moved
    FRAME_STYLE_CLOSABLE    = 0x10,     // Has close button
    FRAME_STYLE_MINIMIZABLE = 0x20,     // Has minimize button
    FRAME_STYLE_MAXIMIZABLE = 0x40,     // Has maximize button
    FRAME_STYLE_TRANSPARENT = 0x80,     // Supports transparency
} frame_style_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} frame_color_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} frame_rect_t;

// ============================================================================
// Frame Structure
// ============================================================================

#define MAX_CHILD_CONTROLS 128  // Increased for windows with many controls (e.g., boot messages)
#define MAX_CHILD_FRAMES 16

struct qarma_frame {
    // Handle and identification
    qarma_handle_t handle;              // Unique frame handle
    char name[32];                      // Frame name for debugging
    
    // Hierarchy
    qarma_frame_t* parent;              // Parent frame (NULL for root)
    qarma_frame_t* children[MAX_CHILD_FRAMES];  // Child frames
    uint32_t child_count;               // Number of child frames
    
    // Controls within this frame
    qarma_control_t* controls[MAX_CHILD_CONTROLS];
    uint32_t control_count;             // Number of controls
    
    // Position and size
    frame_rect_t bounds;                // Frame bounds (relative to parent)
    frame_rect_t client_area;           // Client area (excludes title bar, border)
    
    // Visual properties
    uint32_t style_flags;               // Frame style flags
    frame_color_t background;           // Background color
    frame_color_t border_color;         // Border color
    uint8_t transparency;               // 0=transparent, 255=opaque
    char title[64];                     // Title bar text
    
    // State
    bool visible;                       // Is frame visible?
    bool enabled;                       // Is frame enabled?
    bool focused;                       // Does frame have focus?
    bool modal;                         // Is frame modal (blocks parent)?
    
    // Z-order and layering
    uint32_t z_order;                   // Z-order for drawing
    
    // Message handling
    message_handler_fn message_handler; // Frame message handler
    void* user_data;                    // User-defined data
    
    // Layout
    uint32_t padding;                   // Internal padding
    uint32_t margin;                    // External margin
    
    // Rendering
    uint32_t* pixel_buffer;             // Frame's pixel buffer (if double-buffered)
    bool dirty;                         // Needs redraw?
};

// ============================================================================
// Frame Management API
// ============================================================================

/**
 * Initialize the frame system.
 */
void frame_system_init(void);

/**
 * Shutdown the frame system.
 */
void frame_system_shutdown(void);

/**
 * Create a new frame.
 * @param parent Parent frame (NULL for root frame)
 * @param x X position relative to parent
 * @param y Y position relative to parent
 * @param width Frame width
 * @param height Frame height
 * @param style Style flags
 * @param title Title text (can be NULL)
 * @return New frame, or NULL on failure
 */
qarma_frame_t* frame_create(qarma_frame_t* parent, int32_t x, int32_t y,
                           int32_t width, int32_t height,
                           uint32_t style, const char* title);

/**
 * Destroy a frame and all its children.
 * @param frame Frame to destroy
 */
void frame_destroy(qarma_frame_t* frame);

/**
 * Add a child frame.
 * @param parent Parent frame
 * @param child Child frame to add
 * @return true if successful
 */
bool frame_add_child(qarma_frame_t* parent, qarma_frame_t* child);

/**
 * Remove a child frame.
 * @param parent Parent frame
 * @param child Child frame to remove
 * @return true if successful
 */
bool frame_remove_child(qarma_frame_t* parent, qarma_frame_t* child);

/**
 * Add a control to a frame.
 * @param frame Frame to add control to
 * @param control Control to add
 * @return true if successful
 */
bool frame_add_control(qarma_frame_t* frame, qarma_control_t* control);

/**
 * Remove a control from a frame.
 * @param frame Frame to remove control from
 * @param control Control to remove
 * @return true if successful
 */
bool frame_remove_control(qarma_frame_t* frame, qarma_control_t* control);

/**
 * Set frame position.
 * @param frame Frame to move
 * @param x New X position
 * @param y New Y position
 */
void frame_set_position(qarma_frame_t* frame, int32_t x, int32_t y);

/**
 * Set frame size.
 * @param frame Frame to resize
 * @param width New width
 * @param height New height
 */
void frame_set_size(qarma_frame_t* frame, int32_t width, int32_t height);

/**
 * Set frame bounds (position and size).
 * @param frame Frame to modify
 * @param x X position
 * @param y Y position
 * @param width Width
 * @param height Height
 */
void frame_set_bounds(qarma_frame_t* frame, int32_t x, int32_t y,
                     int32_t width, int32_t height);

/**
 * Get absolute position of frame (in screen coordinates).
 * @param frame Frame to query
 * @param x Output X position
 * @param y Output Y position
 */
void frame_get_absolute_position(qarma_frame_t* frame, int32_t* x, int32_t* y);

/**
 * Set frame visibility.
 * @param frame Frame to modify
 * @param visible Visible flag
 */
void frame_set_visible(qarma_frame_t* frame, bool visible);

/**
 * Set frame enabled state.
 * @param frame Frame to modify
 * @param enabled Enabled flag
 */
void frame_set_enabled(qarma_frame_t* frame, bool enabled);

/**
 * Set frame focus.
 * @param frame Frame to focus (or NULL to clear focus)
 */
void frame_set_focus(qarma_frame_t* frame);

/**
 * Get currently focused frame.
 * @return Focused frame, or NULL if none
 */
qarma_frame_t* frame_get_focused(void);

/**
 * Set frame transparency.
 * @param frame Frame to modify
 * @param alpha Alpha value (0=transparent, 255=opaque)
 */
void frame_set_transparency(qarma_frame_t* frame, uint8_t alpha);

/**
 * Set frame background color.
 * @param frame Frame to modify
 * @param color Background color
 */
void frame_set_background(qarma_frame_t* frame, frame_color_t color);

/**
 * Set frame title.
 * @param frame Frame to modify
 * @param title New title text
 */
void frame_set_title(qarma_frame_t* frame, const char* title);

/**
 * Mark frame as needing redraw.
 * @param frame Frame to invalidate
 */
void frame_invalidate(qarma_frame_t* frame);

/**
 * Invalidate frame and all children.
 * @param frame Frame to invalidate recursively
 */
void frame_invalidate_recursive(qarma_frame_t* frame);

/**
 * Render a frame and its contents.
 * @param frame Frame to render
 */
void frame_render(qarma_frame_t* frame);

/**
 * Render all frames in the system.
 */
void frame_render_all(void);

/**
 * Swap back buffer to front buffer (double buffering).
 */
void frame_swap_buffers(void);

/**
 * Check if point is inside frame.
 * @param frame Frame to test
 * @param x X coordinate (absolute)
 * @param y Y coordinate (absolute)
 * @return true if point is inside frame
 */
bool frame_contains_point(qarma_frame_t* frame, int32_t x, int32_t y);

/**
 * Find topmost frame at given point.
 * @param x X coordinate (absolute)
 * @param y Y coordinate (absolute)
 * @return Frame at point, or NULL if none
 */
qarma_frame_t* frame_hit_test(int32_t x, int32_t y);

/**
 * Send message to frame.
 * @param frame Target frame
 * @param msg Message to send
 * @return Result code from handler
 */
int32_t frame_send_message(qarma_frame_t* frame, qarma_message_t* msg);

/**
 * Post message to frame (async).
 * @param frame Target frame
 * @param msg Message to post
 * @return true if queued successfully
 */
bool frame_post_message(qarma_frame_t* frame, qarma_message_t* msg);

/**
 * Process pending messages for a frame.
 * @param frame Frame to process messages for
 * @return Number of messages processed
 */
uint32_t frame_process_messages(qarma_frame_t* frame);

/**
 * Bring frame to front (highest Z-order).
 * @param frame Frame to raise
 */
void frame_bring_to_front(qarma_frame_t* frame);

/**
 * Send frame to back (lowest Z-order).
 * @param frame Frame to lower
 */
void frame_send_to_back(qarma_frame_t* frame);

/**
 * Get root frame (desktop).
 * @return Root frame
 */
qarma_frame_t* frame_get_root(void);

/**
 * Dump frame hierarchy (debugging).
 * @param frame Starting frame (NULL for root)
 * @param indent Indentation level
 */
void frame_dump_hierarchy(qarma_frame_t* frame, int indent);

#endif // FRAME_H
