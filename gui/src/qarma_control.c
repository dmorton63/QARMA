/**
 * QARMA - Unified Control System Implementation
 * 
 * Message-driven control system with handle-based addressing.
 */

#include "qarma_control.h"
#include "memory/heap.h"
#include "string.h"
#include "config.h"

// ============================================================================
// Global State
// ============================================================================

static struct {
    qarma_control_t* focused_control;
    bool initialized;
} g_control_system = {0};

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * Default control message handler.
 */
static int32_t default_control_handler(qarma_control_t* control, qarma_message_t* msg) {
    (void)control;
    (void)msg;
    // Default handler does nothing - subclasses override
    return 0;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void control_system_init(void) {
    if (g_control_system.initialized) {
        SERIAL_LOG("[CONTROL_SYS] Already initialized\n");
        return;
    }
    
    g_control_system.focused_control = NULL;
    g_control_system.initialized = true;
    
    SERIAL_LOG("[CONTROL_SYS] Control system initialized\n");
}

void control_system_shutdown(void) {
    if (!g_control_system.initialized) {
        return;
    }
    
    g_control_system.focused_control = NULL;
    g_control_system.initialized = false;
    
    SERIAL_LOG("[CONTROL_SYS] Control system shutdown\n");
}

qarma_control_t* control_create(qarma_frame_t* parent_frame, control_type_t type,
                                const char* name, int32_t x, int32_t y,
                                int32_t width, int32_t height) {
    if (!g_control_system.initialized) {
        SERIAL_LOG("[CONTROL_SYS] ERROR: Not initialized\n");
        return NULL;
    }
    
    if (!parent_frame) {
        SERIAL_LOG("[CONTROL_SYS] ERROR: Parent frame required\n");
        return NULL;
    }
    
    // Allocate control
    qarma_control_t* control = (qarma_control_t*)heap_alloc(sizeof(qarma_control_t));
    if (!control) {
        SERIAL_LOG("[CONTROL_SYS] ERROR: Allocation failed\n");
        return NULL;
    }
    
    memset(control, 0, sizeof(qarma_control_t));
    
    // Allocate handle
    control->handle = handle_allocate(HANDLE_TYPE_CONTROL, control, name);
    if (control->handle == QARMA_INVALID_HANDLE) {
        heap_free(control);
        return NULL;
    }
    
    // Initialize properties
    control->type = type;
    control->id = (uint32_t)(control->handle & 0xFFFFFFFF);  // Legacy compatibility
    control->parent_frame = parent_frame;
    control->parent_control = NULL;
    
    if (name) {
        strncpy(control->name, name, sizeof(control->name) - 1);
        control->name[sizeof(control->name) - 1] = '\0';
    }
    
    // Position and size
    control->x = x;
    control->y = y;
    control->width = width;
    control->height = height;
    control->min_width = 0;
    control->min_height = 0;
    control->max_width = 0;
    control->max_height = 0;
    
    // State
    control->state_flags = CONTROL_STATE_VISIBLE | CONTROL_STATE_ENABLED;
    control->style_flags = 0;
    
    // Colors
    control->background.red = 240;
    control->background.green = 240;
    control->background.blue = 240;
    control->background.alpha = 255;
    
    control->foreground.red = 0;
    control->foreground.green = 0;
    control->foreground.blue = 0;
    control->foreground.alpha = 255;
    
    control->border_color.red = 100;
    control->border_color.green = 100;
    control->border_color.blue = 100;
    control->border_color.alpha = 255;
    
    control->transparency = 255;  // Opaque
    
    // Children
    control->children = NULL;
    control->child_count = 0;
    control->child_capacity = 0;
    
    // Handlers
    control->message_handler = default_control_handler;
    control->render_func = NULL;
    control->input_handler = NULL;
    control->cleanup_func = NULL;
    
    // User data
    control->user_data = NULL;
    control->implementation_data = NULL;
    
    // Tab order
    control->tab_index = -1;
    control->tab_next = NULL;
    control->tab_prev = NULL;
    
    // Z-order
    control->z_order = 0;
    
    // Create message queue for this control
    message_queue_create(control->handle, NULL, 0);
    
    // Add to parent frame
    if (!frame_add_control(parent_frame, control)) {
        SERIAL_LOG("[CONTROL_SYS] ERROR: Failed to add to parent frame\n");
        message_queue_destroy(control->handle);
        handle_release(control->handle);
        heap_free(control);
        return NULL;
    }
    
    // Send CREATE message to control
    qarma_message_t* create_msg = message_create(MSG_CREATE, QARMA_INVALID_HANDLE,
                                                 control->handle, 0, 0);
    if (create_msg) {
        control_send_message(control, create_msg);
        message_free(create_msg);
    }
    
    return control;
}

void control_destroy(qarma_control_t* control) {
    if (!control) {
        return;
    }
    
    // Send DESTROY message
    qarma_message_t* destroy_msg = message_create(MSG_DESTROY, QARMA_INVALID_HANDLE,
                                                  control->handle, 0, 0);
    if (destroy_msg) {
        control_send_message(control, destroy_msg);
        message_free(destroy_msg);
    }
    
    // Destroy all children first
    for (uint32_t i = 0; i < control->child_count; i++) {
        control_destroy(control->children[i]);
    }
    
    // Free children array
    if (control->children) {
        heap_free(control->children);
    }
    
    // Call cleanup function if provided
    if (control->cleanup_func) {
        control->cleanup_func(control);
    }
    
    // Free implementation data if allocated
    if (control->implementation_data) {
        heap_free(control->implementation_data);
    }
    
    // Remove from parent frame
    if (control->parent_frame) {
        frame_remove_control(control->parent_frame, control);
    }
    
    // Remove from parent control
    if (control->parent_control) {
        control_remove_child(control->parent_control, control);
    }
    
    // Clear focus if this control has it
    if (g_control_system.focused_control == control) {
        g_control_system.focused_control = NULL;
    }
    
    // Destroy message queue
    message_queue_destroy(control->handle);
    
    // Release handle
    handle_release(control->handle);
    
    // Free control
    heap_free(control);
}

bool control_add_child(qarma_control_t* parent, qarma_control_t* child) {
    if (!parent || !child) {
        return false;
    }
    
    // Resize children array if needed
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        qarma_control_t** new_children = (qarma_control_t**)heap_alloc(
            sizeof(qarma_control_t*) * new_capacity);
        
        if (!new_children) {
            return false;
        }
        
        if (parent->children) {
            for (uint32_t i = 0; i < parent->child_count; i++) {
                new_children[i] = parent->children[i];
            }
            heap_free(parent->children);
        }
        
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent_control = parent;
    
    return true;
}

bool control_remove_child(qarma_control_t* parent, qarma_control_t* child) {
    if (!parent || !child || !parent->children) {
        return false;
    }
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Shift remaining children
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent_control = NULL;
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// State Management
// ============================================================================

void control_set_visible(qarma_control_t* control, bool visible) {
    if (!control) return;
    
    if (visible) {
        control->state_flags |= CONTROL_STATE_VISIBLE;
    } else {
        control->state_flags &= ~CONTROL_STATE_VISIBLE;
    }
    
    control_invalidate(control);
}

bool control_is_visible(qarma_control_t* control) {
    return control && (control->state_flags & CONTROL_STATE_VISIBLE);
}

void control_set_enabled(qarma_control_t* control, bool enabled) {
    if (!control) return;
    
    if (enabled) {
        control->state_flags |= CONTROL_STATE_ENABLED;
        control->state_flags &= ~CONTROL_STATE_DISABLED;
    } else {
        control->state_flags &= ~CONTROL_STATE_ENABLED;
        control->state_flags |= CONTROL_STATE_DISABLED;
    }
}

bool control_is_enabled(qarma_control_t* control) {
    return control && (control->state_flags & CONTROL_STATE_ENABLED);
}

void control_set_focus(qarma_control_t* control) {
    if (g_control_system.focused_control == control) {
        return;
    }
    
    // Send KILLFOCUS to old focused control
    if (g_control_system.focused_control) {
        qarma_message_t* msg = message_create(MSG_KILLFOCUS, QARMA_INVALID_HANDLE,
                                             g_control_system.focused_control->handle, 0, 0);
        if (msg) {
            control_send_message(g_control_system.focused_control, msg);
            message_free(msg);
        }
        g_control_system.focused_control->state_flags &= ~CONTROL_STATE_FOCUSED;
    }
    
    g_control_system.focused_control = control;
    
    // Send SETFOCUS to new focused control
    if (control) {
        qarma_message_t* msg = message_create(MSG_SETFOCUS, QARMA_INVALID_HANDLE,
                                             control->handle, 0, 0);
        if (msg) {
            control_send_message(control, msg);
            message_free(msg);
        }
        control->state_flags |= CONTROL_STATE_FOCUSED;
    }
}

bool control_has_focus(qarma_control_t* control) {
    return control && (control->state_flags & CONTROL_STATE_FOCUSED);
}

void control_invalidate(qarma_control_t* control) {
    if (!control) return;
    
    control->state_flags |= CONTROL_STATE_DIRTY;
    
    // Invalidate parent frame
    if (control->parent_frame) {
        frame_invalidate(control->parent_frame);
    }
}

// ============================================================================
// Properties
// ============================================================================

void control_set_position(qarma_control_t* control, int32_t x, int32_t y) {
    if (!control) return;
    
    control->x = x;
    control->y = y;
    control_invalidate(control);
}

void control_get_position(qarma_control_t* control, int32_t* x, int32_t* y) {
    if (!control || !x || !y) return;
    
    *x = control->x;
    *y = control->y;
}

void control_set_size(qarma_control_t* control, int32_t width, int32_t height) {
    if (!control) return;
    
    control->width = width;
    control->height = height;
    control_invalidate(control);
}

void control_get_size(qarma_control_t* control, int32_t* width, int32_t* height) {
    if (!control || !width || !height) return;
    
    *width = control->width;
    *height = control->height;
}

void control_set_bounds(qarma_control_t* control, int32_t x, int32_t y,
                       int32_t width, int32_t height) {
    if (!control) return;
    
    control->x = x;
    control->y = y;
    control->width = width;
    control->height = height;
    control_invalidate(control);
}

static void control_get_absolute_position_internal(qarma_control_t* control, int32_t* x, int32_t* y, int depth) {
    if (!control || !x || !y || depth > 10) return;  // Prevent infinite recursion
    
    *x = control->x;
    *y = control->y;
    
    // Add parent control offset if any
    if (control->parent_control) {
        int32_t parent_x, parent_y;
        control_get_absolute_position_internal(control->parent_control, &parent_x, &parent_y, depth + 1);
        *x += parent_x;
        *y += parent_y;
    }
    
    // Add parent frame offset
    if (control->parent_frame) {
        int32_t frame_x, frame_y;
        frame_get_absolute_position(control->parent_frame, &frame_x, &frame_y);
        *x += frame_x;
        *y += frame_y;
    }
}

void control_get_absolute_position(qarma_control_t* control, int32_t* x, int32_t* y) {
    control_get_absolute_position_internal(control, x, y, 0);
}

void control_set_background(qarma_control_t* control, frame_color_t color) {
    if (!control) return;
    
    control->background = color;
    control_invalidate(control);
}

void control_set_foreground(qarma_control_t* control, frame_color_t color) {
    if (!control) return;
    
    control->foreground = color;
    control_invalidate(control);
}

void control_set_transparency(qarma_control_t* control, uint8_t alpha) {
    if (!control) return;
    
    control->transparency = alpha;
    control_invalidate(control);
}

// ============================================================================
// Rendering
// ============================================================================

void control_render(qarma_control_t* control, uint32_t* buffer,
                   int32_t buf_width, int32_t buf_height) {
    if (!control || !control_is_visible(control) || !buffer) {
        return;
    }
    
    if (control->render_func) {
        control->render_func(control, buffer, buf_width, buf_height);
    }
    
    control->state_flags &= ~CONTROL_STATE_DIRTY;
}

void control_render_recursive(qarma_control_t* control, uint32_t* buffer,
                             int32_t buf_width, int32_t buf_height) {
    if (!control || !control_is_visible(control)) {
        return;
    }
    
    // Render self
    control_render(control, buffer, buf_width, buf_height);
    
    // Render children
    for (uint32_t i = 0; i < control->child_count; i++) {
        control_render_recursive(control->children[i], buffer, buf_width, buf_height);
    }
}

// ============================================================================
// Messages
// ============================================================================

int32_t control_send_message(qarma_control_t* control, qarma_message_t* msg) {
    if (!control || !msg) {
        return -1;
    }
    
    msg->target = control->handle;
    
    // Call message handler if set
    if (control->message_handler) {
        return control->message_handler(control, msg);
    }
    
    return 0;
}

bool control_post_message(qarma_control_t* control, qarma_message_t* msg) {
    if (!control || !msg) {
        return false;
    }
    
    msg->target = control->handle;
    return message_post(msg);
}

uint32_t control_process_messages(qarma_control_t* control) {
    if (!control) {
        return 0;
    }
    
    return message_dispatch_all(control->handle);
}

// ============================================================================
// Hit Testing
// ============================================================================

bool control_contains_point(qarma_control_t* control, int32_t x, int32_t y) {
    if (!control) return false;
    
    int32_t abs_x, abs_y;
    control_get_absolute_position(control, &abs_x, &abs_y);
    
    return (x >= abs_x && x < abs_x + control->width &&
            y >= abs_y && y < abs_y + control->height);
}

qarma_control_t* control_hit_test(qarma_control_t* root, int32_t x, int32_t y) {
    if (!root || !control_is_visible(root) || !control_is_enabled(root)) {
        return NULL;
    }
    
    // Test children first (front to back)
    for (int32_t i = (int32_t)root->child_count - 1; i >= 0; i--) {
        qarma_control_t* hit = control_hit_test(root->children[i], x, y);
        if (hit) {
            return hit;
        }
    }
    
    // Test self
    if (control_contains_point(root, x, y)) {
        return root;
    }
    
    return NULL;
}

// ============================================================================
// Utilities
// ============================================================================

const char* control_type_to_string(control_type_t type) {
    switch (type) {
        case CONTROL_TYPE_BUTTON: return "Button";
        case CONTROL_TYPE_LABEL: return "Label";
        case CONTROL_TYPE_TEXTBOX: return "TextBox";
        case CONTROL_TYPE_CHECKBOX: return "CheckBox";
        case CONTROL_TYPE_RADIO: return "RadioButton";
        case CONTROL_TYPE_LISTBOX: return "ListBox";
        case CONTROL_TYPE_COMBOBOX: return "ComboBox";
        case CONTROL_TYPE_SLIDER: return "Slider";
        case CONTROL_TYPE_PROGRESSBAR: return "ProgressBar";
        case CONTROL_TYPE_SCROLLBAR: return "ScrollBar";
        case CONTROL_TYPE_MENU: return "Menu";
        case CONTROL_TYPE_MENUITEM: return "MenuItem";
        case CONTROL_TYPE_STATUSBAR: return "StatusBar";
        case CONTROL_TYPE_TOOLBAR: return "ToolBar";
        case CONTROL_TYPE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

void control_dump_hierarchy(qarma_control_t* control, int indent) {
    if (!control) {
        return;
    }
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        SERIAL_LOG("  ");
    }
    
    // Print control info
    SERIAL_LOG("Control: ");
    SERIAL_LOG(control->name[0] ? control->name : "(unnamed)");
    SERIAL_LOG(" Type: ");
    SERIAL_LOG(control_type_to_string(control->type));
    SERIAL_LOG(" [");
    SERIAL_LOG_DEC("", control->x);
    SERIAL_LOG(",");
    SERIAL_LOG_DEC("", control->y);
    SERIAL_LOG(" ");
    SERIAL_LOG_DEC("", control->width);
    SERIAL_LOG("x");
    SERIAL_LOG_DEC("", control->height);
    SERIAL_LOG("]\n");
    
    // Print children
    for (uint32_t i = 0; i < control->child_count; i++) {
        control_dump_hierarchy(control->children[i], indent + 1);
    }
}
