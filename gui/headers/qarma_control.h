/**
 * QARMA - Unified Control System
 * 
 * Base control structure integrated with handle manager and message system.
 * All GUI controls inherit from qarma_control_t.
 */

#ifndef QARMA_CONTROL_H
#define QARMA_CONTROL_H

#include "handle_manager.h"
#include "message_system.h"
#include "frame.h"

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct qarma_control qarma_control_t;
typedef struct QARMA_INPUT_EVENT QARMA_INPUT_EVENT;

// ============================================================================
// Control Types
// ============================================================================

typedef enum {
    CONTROL_TYPE_UNKNOWN = 0,
    CONTROL_TYPE_BUTTON,
    CONTROL_TYPE_LABEL,
    CONTROL_TYPE_TEXTBOX,
    CONTROL_TYPE_CHECKBOX,
    CONTROL_TYPE_RADIO,
    CONTROL_TYPE_LISTBOX,
    CONTROL_TYPE_COMBOBOX,
    CONTROL_TYPE_SLIDER,
    CONTROL_TYPE_PROGRESSBAR,
    CONTROL_TYPE_SCROLLBAR,
    CONTROL_TYPE_MENU,
    CONTROL_TYPE_MENUITEM,
    CONTROL_TYPE_STATUSBAR,
    CONTROL_TYPE_TOOLBAR,
    CONTROL_TYPE_CUSTOM = 0x1000
} control_type_t;

// ============================================================================
// Control State Flags
// ============================================================================

typedef enum {
    CONTROL_STATE_VISIBLE       = 0x0001,
    CONTROL_STATE_ENABLED       = 0x0002,
    CONTROL_STATE_FOCUSED       = 0x0004,
    CONTROL_STATE_HOVERED       = 0x0008,
    CONTROL_STATE_PRESSED       = 0x0010,
    CONTROL_STATE_CHECKED       = 0x0020,
    CONTROL_STATE_SELECTED      = 0x0040,
    CONTROL_STATE_DISABLED      = 0x0080,
    CONTROL_STATE_HIDDEN        = 0x0100,
    CONTROL_STATE_READONLY      = 0x0200,
    CONTROL_STATE_DIRTY         = 0x0400,  // Needs repaint
} control_state_t;

// ============================================================================
// Control Style Flags
// ============================================================================

typedef enum {
    CONTROL_STYLE_BORDER        = 0x0001,
    CONTROL_STYLE_FLAT          = 0x0002,
    CONTROL_STYLE_3D            = 0x0004,
    CONTROL_STYLE_TRANSPARENT   = 0x0008,
    CONTROL_STYLE_MULTILINE     = 0x0010,
    CONTROL_STYLE_WORDWRAP      = 0x0020,
    CONTROL_STYLE_SCROLLBAR     = 0x0040,
    CONTROL_STYLE_AUTOHIDE      = 0x0080,
    CONTROL_STYLE_OWNERDRAW     = 0x0100,
} control_style_t;

// ============================================================================
// Control Message Handler
// ============================================================================

/**
 * Control message handler function.
 * 
 * @param control The control receiving the message
 * @param msg The message to process
 * @return Message result (0 = processed, non-zero = error or specific result)
 */
typedef int32_t (*control_message_handler_fn)(qarma_control_t* control, qarma_message_t* msg);

/**
 * Control render function.
 * 
 * @param control The control to render
 * @param buffer Pixel buffer to render into
 * @param buf_width Buffer width in pixels
 * @param buf_height Buffer height in pixels
 */
typedef void (*control_render_fn)(qarma_control_t* control, uint32_t* buffer, 
                                   int32_t buf_width, int32_t buf_height);

/**
 * Control input handler function.
 * 
 * @param control The control receiving input
 * @param event The input event
 * @return true if event was handled, false to propagate
 */
typedef bool (*control_input_handler_fn)(qarma_control_t* control, QARMA_INPUT_EVENT* event);

/**
 * Control cleanup function.
 * 
 * @param control The control being destroyed
 */
typedef void (*control_cleanup_fn)(qarma_control_t* control);

// ============================================================================
// Control Structure
// ============================================================================

typedef struct qarma_control {
    // Identification
    qarma_handle_t handle;              // Unique handle
    char name[32];                      // Control name
    control_type_t type;                // Control type
    uint32_t id;                        // Legacy ID for compatibility
    
    // Hierarchy
    qarma_frame_t* parent_frame;        // Parent frame (required)
    qarma_control_t* parent_control;    // Parent control (for composite controls)
    qarma_control_t** children;         // Child controls
    uint32_t child_count;
    uint32_t child_capacity;
    
    // Position and Size (relative to parent)
    int32_t x, y;
    int32_t width, height;
    int32_t min_width, min_height;
    int32_t max_width, max_height;
    
    // State
    uint32_t state_flags;               // CONTROL_STATE_* flags
    uint32_t style_flags;               // CONTROL_STYLE_* flags
    
    // Visual Properties
    frame_color_t background;
    frame_color_t foreground;
    frame_color_t border_color;
    uint8_t transparency;               // 0=transparent, 255=opaque
    int32_t z_order;
    
    // Padding and Margins
    int32_t padding_left, padding_top;
    int32_t padding_right, padding_bottom;
    int32_t margin_left, margin_top;
    int32_t margin_right, margin_bottom;
    
    // Message Handling
    control_message_handler_fn message_handler;
    control_render_fn render_func;
    control_input_handler_fn input_handler;
    control_cleanup_fn cleanup_func;
    
    // User Data
    void* user_data;                    // Application-specific data
    void* implementation_data;          // Control-specific implementation data
    
    // Tab Order
    int32_t tab_index;
    qarma_control_t* tab_next;
    qarma_control_t* tab_prev;
    
} qarma_control_t;

// ============================================================================
// Control System API
// ============================================================================

/**
 * Initialize the control system.
 */
void control_system_init(void);

/**
 * Shutdown the control system.
 */
void control_system_shutdown(void);

/**
 * Create a new control.
 * 
 * @param parent_frame Parent frame (required)
 * @param type Control type
 * @param name Control name
 * @param x X position relative to parent
 * @param y Y position relative to parent
 * @param width Control width
 * @param height Control height
 * @return New control or NULL on failure
 */
qarma_control_t* control_create(qarma_frame_t* parent_frame, control_type_t type,
                                const char* name, int32_t x, int32_t y,
                                int32_t width, int32_t height);

/**
 * Destroy a control and all its children.
 * 
 * @param control Control to destroy
 */
void control_destroy(qarma_control_t* control);

/**
 * Add a child control.
 * 
 * @param parent Parent control
 * @param child Child control
 * @return true on success
 */
bool control_add_child(qarma_control_t* parent, qarma_control_t* child);

/**
 * Remove a child control.
 * 
 * @param parent Parent control
 * @param child Child control
 * @return true on success
 */
bool control_remove_child(qarma_control_t* parent, qarma_control_t* child);

// ============================================================================
// Control State Management
// ============================================================================

void control_set_visible(qarma_control_t* control, bool visible);
bool control_is_visible(qarma_control_t* control);

void control_set_enabled(qarma_control_t* control, bool enabled);
bool control_is_enabled(qarma_control_t* control);

void control_set_focus(qarma_control_t* control);
bool control_has_focus(qarma_control_t* control);

void control_invalidate(qarma_control_t* control);

// ============================================================================
// Control Properties
// ============================================================================

void control_set_position(qarma_control_t* control, int32_t x, int32_t y);
void control_get_position(qarma_control_t* control, int32_t* x, int32_t* y);

void control_set_size(qarma_control_t* control, int32_t width, int32_t height);
void control_get_size(qarma_control_t* control, int32_t* width, int32_t* height);

void control_set_bounds(qarma_control_t* control, int32_t x, int32_t y,
                       int32_t width, int32_t height);

void control_get_absolute_position(qarma_control_t* control, int32_t* x, int32_t* y);

void control_set_background(qarma_control_t* control, frame_color_t color);
void control_set_foreground(qarma_control_t* control, frame_color_t color);
void control_set_transparency(qarma_control_t* control, uint8_t alpha);

// ============================================================================
// Control Rendering
// ============================================================================

void control_render(qarma_control_t* control, uint32_t* buffer,
                   int32_t buf_width, int32_t buf_height);

void control_render_recursive(qarma_control_t* control, uint32_t* buffer,
                             int32_t buf_width, int32_t buf_height);

// ============================================================================
// Control Messages
// ============================================================================

int32_t control_send_message(qarma_control_t* control, qarma_message_t* msg);
bool control_post_message(qarma_control_t* control, qarma_message_t* msg);
uint32_t control_process_messages(qarma_control_t* control);

// ============================================================================
// Control Hit Testing
// ============================================================================

bool control_contains_point(qarma_control_t* control, int32_t x, int32_t y);
qarma_control_t* control_hit_test(qarma_control_t* root, int32_t x, int32_t y);

// ============================================================================
// Control Utilities
// ============================================================================

const char* control_type_to_string(control_type_t type);
void control_dump_hierarchy(qarma_control_t* control, int indent);

#endif // QARMA_CONTROL_H
