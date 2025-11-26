/**
 * QARMA - Frame System Implementation
 * 
 * Container/frame management with parent-child relationships and message routing.
 */

#include "gui/frame.h"
#include "gui/qarma_control.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "config.h"
#include "graphics/framebuffer.h"
#include "gui/renderer.h"

// ============================================================================
// Global State
// ============================================================================

static struct {
    qarma_frame_t* root_frame;          // Desktop/root frame
    qarma_frame_t* focused_frame;       // Currently focused frame
    uint32_t next_z_order;              // Next Z-order value
    bool initialized;
} g_frame_system = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * Calculate client area from frame bounds.
 */
static void calculate_client_area(qarma_frame_t* frame) {
    int32_t title_height = (frame->style_flags & FRAME_STYLE_TITLE_BAR) ? 24 : 0;
    int32_t border_width = (frame->style_flags & FRAME_STYLE_BORDER) ? 1 : 0;
    
    frame->client_area.x = frame->bounds.x + border_width;
    frame->client_area.y = frame->bounds.y + title_height + border_width;
    frame->client_area.width = frame->bounds.width - (border_width * 2);
    frame->client_area.height = frame->bounds.height - title_height - (border_width * 2);
}

/**
 * Default frame message handler.
 */
static int32_t default_frame_handler(qarma_handle_t recipient, qarma_message_t* msg) {
    (void)recipient;
    (void)msg;
    // Default handler does nothing - subclasses override
    return 0;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void frame_system_init(void) {
    if (g_frame_system.initialized) {
        SERIAL_LOG("[FRAME_SYS] Already initialized\n");
        return;
    }
    
    g_frame_system.root_frame = NULL;
    g_frame_system.focused_frame = NULL;
    g_frame_system.next_z_order = 0;
    g_frame_system.initialized = true;
    
    SERIAL_LOG("[FRAME_SYS] Frame system initialized\n");
}

void frame_system_shutdown(void) {
    if (!g_frame_system.initialized) {
        return;
    }
    
    // Destroy root frame (cascades to all children)
    if (g_frame_system.root_frame) {
        frame_destroy(g_frame_system.root_frame);
    }
    
    g_frame_system.initialized = false;
    SERIAL_LOG("[FRAME_SYS] Frame system shutdown\n");
}

qarma_frame_t* frame_create(qarma_frame_t* parent, int32_t x, int32_t y,
                           int32_t width, int32_t height,
                           uint32_t style, const char* title) {
    if (!g_frame_system.initialized) {
        SERIAL_LOG("[FRAME_SYS] ERROR: Not initialized\n");
        return NULL;
    }
    
    // Allocate frame
    qarma_frame_t* frame = (qarma_frame_t*)heap_alloc(sizeof(qarma_frame_t));
    if (!frame) {
        SERIAL_LOG("[FRAME_SYS] ERROR: Allocation failed\n");
        return NULL;
    }
    
    memset(frame, 0, sizeof(qarma_frame_t));
    
    // Allocate handle
    frame->handle = handle_allocate(HANDLE_TYPE_FRAME, frame, title);
    if (frame->handle == QARMA_INVALID_HANDLE) {
        heap_free(frame);
        return NULL;
    }
    
    // Initialize basic properties
    frame->parent = parent;
    frame->child_count = 0;
    frame->control_count = 0;
    
    // Set position and size
    frame->bounds.x = x;
    frame->bounds.y = y;
    frame->bounds.width = width;
    frame->bounds.height = height;
    
    // Calculate client area
    frame->style_flags = style;
    calculate_client_area(frame);
    
    // Set visual properties
    frame->background.red = 240;
    frame->background.green = 240;
    frame->background.blue = 240;
    frame->background.alpha = 255;
    
    frame->border_color.red = 100;
    frame->border_color.green = 100;
    frame->border_color.blue = 100;
    frame->border_color.alpha = 255;
    
    frame->transparency = 255;  // Opaque by default
    
    if (title) {
        strncpy(frame->title, title, sizeof(frame->title) - 1);
        frame->title[sizeof(frame->title) - 1] = '\0';
        strncpy(frame->name, title, sizeof(frame->name) - 1);
        frame->name[sizeof(frame->name) - 1] = '\0';
    }
    
    // Set state
    frame->visible = true;
    frame->enabled = true;
    frame->focused = false;
    frame->modal = false;
    frame->dirty = true;
    
    // Set Z-order
    frame->z_order = g_frame_system.next_z_order++;
    
    // Set default message handler
    frame->message_handler = default_frame_handler;
    frame->user_data = NULL;
    
    // Set defaults
    frame->padding = 4;
    frame->margin = 0;
    frame->pixel_buffer = NULL;
    
    // Create message queue for this frame
    message_queue_create(frame->handle, frame->message_handler, 0);
    
    // Add to parent
    if (parent) {
        if (!frame_add_child(parent, frame)) {
            SERIAL_LOG("[FRAME_SYS] ERROR: Failed to add to parent\n");
            handle_release(frame->handle);
            heap_free(frame);
            return NULL;
        }
    } else {
        // This is the root frame
        if (g_frame_system.root_frame) {
            SERIAL_LOG("[FRAME_SYS] WARNING: Root frame already exists\n");
        }
        g_frame_system.root_frame = frame;
    }
    
    return frame;
}

void frame_destroy(qarma_frame_t* frame) {
    if (!frame) {
        return;
    }
    
    // Destroy all child frames first
    for (uint32_t i = 0; i < frame->child_count; i++) {
        frame_destroy(frame->children[i]);
    }
    
    // TODO: Destroy all controls (when control system is implemented)
    
    // Remove from parent
    if (frame->parent) {
        frame_remove_child(frame->parent, frame);
    }
    
    // Destroy message queue
    message_queue_destroy(frame->handle);
    
    // Release handle
    handle_release(frame->handle);
    
    // Free pixel buffer if allocated
    if (frame->pixel_buffer) {
        heap_free(frame->pixel_buffer);
    }
    
    // Free frame
    heap_free(frame);
}

bool frame_add_child(qarma_frame_t* parent, qarma_frame_t* child) {
    if (!parent || !child) {
        return false;
    }
    
    if (parent->child_count >= MAX_CHILD_FRAMES) {
        SERIAL_LOG("[FRAME_SYS] ERROR: Max children reached\n");
        return false;
    }
    
    // Check if already a child
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            return true;  // Already added
        }
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    
    return true;
}

bool frame_remove_child(qarma_frame_t* parent, qarma_frame_t* child) {
    if (!parent || !child) {
        return false;
    }
    
    // Find child
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            // Shift remaining children
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent = NULL;
            return true;
        }
    }
    
    return false;
}

bool frame_add_control(qarma_frame_t* frame, qarma_control_t* control) {
    if (!frame || !control) {
        return false;
    }
    
    if (frame->control_count >= MAX_CHILD_CONTROLS) {
        SERIAL_LOG("[FRAME_SYS] ERROR: Max controls reached\n");
        return false;
    }
    
    frame->controls[frame->control_count++] = control;
    return true;
}

bool frame_remove_control(qarma_frame_t* frame, qarma_control_t* control) {
    if (!frame || !control) {
        return false;
    }
    
    // Find control
    for (uint32_t i = 0; i < frame->control_count; i++) {
        if (frame->controls[i] == control) {
            // Shift remaining controls
            for (uint32_t j = i; j < frame->control_count - 1; j++) {
                frame->controls[j] = frame->controls[j + 1];
            }
            frame->control_count--;
            return true;
        }
    }
    
    return false;
}

void frame_set_position(qarma_frame_t* frame, int32_t x, int32_t y) {
    if (!frame) return;
    
    frame->bounds.x = x;
    frame->bounds.y = y;
    calculate_client_area(frame);
    frame_invalidate(frame);
}

void frame_set_size(qarma_frame_t* frame, int32_t width, int32_t height) {
    if (!frame) return;
    
    frame->bounds.width = width;
    frame->bounds.height = height;
    calculate_client_area(frame);
    frame_invalidate(frame);
}

void frame_set_bounds(qarma_frame_t* frame, int32_t x, int32_t y,
                     int32_t width, int32_t height) {
    if (!frame) return;
    
    frame->bounds.x = x;
    frame->bounds.y = y;
    frame->bounds.width = width;
    frame->bounds.height = height;
    calculate_client_area(frame);
    frame_invalidate(frame);
}

void frame_get_absolute_position(qarma_frame_t* frame, int32_t* x, int32_t* y) {
    if (!frame || !x || !y) return;
    
    *x = frame->bounds.x;
    *y = frame->bounds.y;
    
    // Accumulate parent positions
    qarma_frame_t* parent = frame->parent;
    while (parent) {
        *x += parent->bounds.x;
        *y += parent->bounds.y;
        parent = parent->parent;
    }
}

void frame_set_visible(qarma_frame_t* frame, bool visible) {
    if (!frame) return;
    
    frame->visible = visible;
    frame_invalidate(frame);
}

void frame_set_enabled(qarma_frame_t* frame, bool enabled) {
    if (!frame) return;
    
    frame->enabled = enabled;
}

void frame_set_focus(qarma_frame_t* frame) {
    if (g_frame_system.focused_frame == frame) {
        return;  // Already focused
    }
    
    // Send KILLFOCUS to old focused frame
    if (g_frame_system.focused_frame) {
        qarma_message_t* msg = message_create(MSG_KILLFOCUS, QARMA_INVALID_HANDLE,
                                             g_frame_system.focused_frame->handle, 0, 0);
        if (msg) {
            frame_send_message(g_frame_system.focused_frame, msg);
            message_free(msg);
        }
        g_frame_system.focused_frame->focused = false;
    }
    
    g_frame_system.focused_frame = frame;
    
    // Send SETFOCUS to new focused frame
    if (frame) {
        qarma_message_t* msg = message_create(MSG_SETFOCUS, QARMA_INVALID_HANDLE,
                                             frame->handle, 0, 0);
        if (msg) {
            frame_send_message(frame, msg);
            message_free(msg);
        }
        frame->focused = true;
    }
}

qarma_frame_t* frame_get_focused(void) {
    return g_frame_system.focused_frame;
}

void frame_set_transparency(qarma_frame_t* frame, uint8_t alpha) {
    if (!frame) return;
    
    frame->transparency = alpha;
    frame_invalidate(frame);
}

void frame_set_background(qarma_frame_t* frame, frame_color_t color) {
    if (!frame) return;
    
    frame->background = color;
    frame_invalidate(frame);
}

void frame_set_title(qarma_frame_t* frame, const char* title) {
    if (!frame || !title) return;
    
    strncpy(frame->title, title, sizeof(frame->title) - 1);
    frame->title[sizeof(frame->title) - 1] = '\0';
    frame_invalidate(frame);
}

void frame_invalidate(qarma_frame_t* frame) {
    if (!frame) return;
    
    frame->dirty = true;
}

void frame_invalidate_recursive(qarma_frame_t* frame) {
    if (!frame) return;
    
    frame_invalidate(frame);
    
    for (uint32_t i = 0; i < frame->child_count; i++) {
        frame_invalidate_recursive(frame->children[i]);
    }
}

void frame_render(qarma_frame_t* frame) {
    if (!frame || !frame->visible) {
        return;
    }
    
    // Get framebuffer info
    extern FramebufferInfo* fb_info;
    extern uint32_t* backing_store;  // Use backing store for double buffering
    
    if (!fb_info || !fb_info->virt_addr) {
        frame->dirty = false;
        return;
    }
    
    // Render to backing store if available, otherwise to framebuffer
    uint32_t* framebuffer = backing_store ? backing_store : (uint32_t*)(uintptr_t)fb_info->virt_addr;
    int32_t fb_width = fb_info->width;
    int32_t fb_height = fb_info->height;
    
    // Calculate absolute position (considering parent hierarchy)
    int32_t abs_x = frame->bounds.x;
    int32_t abs_y = frame->bounds.y;
    qarma_frame_t* parent = frame->parent;
    while (parent) {
        abs_x += parent->bounds.x;
        abs_y += parent->bounds.y;
        parent = parent->parent;
    }
    
    // Render frame background
    uint32_t bg_color = (frame->background.alpha << 24) | 
                        (frame->background.red << 16) | 
                        (frame->background.green << 8) | 
                        frame->background.blue;
    draw_filled_rect(framebuffer, fb_width, abs_x, abs_y, frame->bounds.width, frame->bounds.height, bg_color);
    
    // Render border if style includes it
    if (frame->style_flags & FRAME_STYLE_BORDER) {
        uint32_t border_color = (frame->border_color.alpha << 24) | 
                                (frame->border_color.red << 16) | 
                                (frame->border_color.green << 8) | 
                                frame->border_color.blue;
        draw_rect_border(framebuffer, fb_width, abs_x, abs_y, frame->bounds.width, frame->bounds.height, border_color, 2);
    }
    
    // Render title bar if style includes it
    if (frame->style_flags & FRAME_STYLE_TITLE_BAR) {
        // Draw title bar background (darker than main bg)
        uint32_t title_bg = bg_color & 0x7F7F7F7F; // Darken by half
        draw_filled_rect(framebuffer, fb_width, abs_x, abs_y, frame->bounds.width, 24, title_bg);
        // Draw title text
        draw_string_to_buffer(framebuffer, fb_width, abs_x + 8, abs_y + 6, frame->title, 0xFFFFFFFF);
    }
    
    // Render all child controls
    for (uint32_t i = 0; i < frame->control_count; i++) {
        qarma_control_t* control = frame->controls[i];
        if (control && control->render_func && (control->state_flags & CONTROL_STATE_VISIBLE)) {
            // Controls render at absolute positions (already offset by frame position)
            // Add frame's absolute position to control's relative position
            control->render_func(control, framebuffer, fb_width, fb_height);
        }
    }
    
    // Render child frames recursively
    for (uint32_t i = 0; i < frame->child_count; i++) {
        if (frame->children[i]) {
            frame_render(frame->children[i]);
        }
    }
    
    frame->dirty = false;
}

void frame_render_all(void) {
    if (g_frame_system.root_frame) {
        frame_render(g_frame_system.root_frame);
    }
}

void frame_swap_buffers(void) {
    // Copy backing store to actual framebuffer
    extern FramebufferInfo* fb_info;
    extern uint32_t* backing_store;
    
    if (!fb_info || !fb_info->virt_addr || !backing_store) {
        return;
    }
    
    uint32_t* framebuffer = (uint32_t*)(uintptr_t)fb_info->virt_addr;
    uint32_t pixels = fb_info->width * fb_info->height;
    
    // Fast memory copy from back buffer to front buffer
    for (uint32_t i = 0; i < pixels; i++) {
        framebuffer[i] = backing_store[i];
    }
}

bool frame_contains_point(qarma_frame_t* frame, int32_t x, int32_t y) {
    if (!frame) return false;
    
    int32_t abs_x, abs_y;
    frame_get_absolute_position(frame, &abs_x, &abs_y);
    
    return (x >= abs_x && x < abs_x + frame->bounds.width &&
            y >= abs_y && y < abs_y + frame->bounds.height);
}

qarma_frame_t* frame_hit_test(int32_t x, int32_t y) {
    // TODO: Implement proper Z-order hit testing
    // For now, just test root frame
    if (g_frame_system.root_frame && 
        frame_contains_point(g_frame_system.root_frame, x, y)) {
        return g_frame_system.root_frame;
    }
    return NULL;
}

int32_t frame_send_message(qarma_frame_t* frame, qarma_message_t* msg) {
    if (!frame || !msg) {
        return -1;
    }
    
    msg->target = frame->handle;
    return message_send(msg);
}

bool frame_post_message(qarma_frame_t* frame, qarma_message_t* msg) {
    if (!frame || !msg) {
        return false;
    }
    
    msg->target = frame->handle;
    return message_post(msg);
}

uint32_t frame_process_messages(qarma_frame_t* frame) {
    if (!frame) {
        return 0;
    }
    
    return message_dispatch_all(frame->handle);
}

void frame_bring_to_front(qarma_frame_t* frame) {
    if (!frame) return;
    
    frame->z_order = g_frame_system.next_z_order++;
    frame_invalidate(frame);
}

void frame_send_to_back(qarma_frame_t* frame) {
    if (!frame) return;
    
    frame->z_order = 0;
    frame_invalidate(frame);
}

qarma_frame_t* frame_get_root(void) {
    return g_frame_system.root_frame;
}

void frame_dump_hierarchy(qarma_frame_t* frame, int indent) {
    if (!frame) {
        frame = g_frame_system.root_frame;
        if (!frame) {
            SERIAL_LOG("[FRAME_SYS] No root frame\n");
            return;
        }
    }
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        SERIAL_LOG("  ");
    }
    
    // Print frame info
    SERIAL_LOG("Frame: ");
    SERIAL_LOG(frame->name[0] ? frame->name : "(unnamed)");
    SERIAL_LOG(" [");
    SERIAL_LOG_DEC("", frame->bounds.x);
    SERIAL_LOG(",");
    SERIAL_LOG_DEC("", frame->bounds.y);
    SERIAL_LOG(" ");
    SERIAL_LOG_DEC("", frame->bounds.width);
    SERIAL_LOG("x");
    SERIAL_LOG_DEC("", frame->bounds.height);
    SERIAL_LOG("] Controls: ");
    SERIAL_LOG_DEC("", frame->control_count);
    SERIAL_LOG("\n");
    
    // Print children
    for (uint32_t i = 0; i < frame->child_count; i++) {
        frame_dump_hierarchy(frame->children[i], indent + 1);
    }
}
