/**
 * QARMA - Hardened Label Control Implementation
 */

#include "controls/controls/qarma_label.h"
#include "memory/heap.h"
#include "string.h"
#include "config.h"

// ============================================================================
// Message Handler
// ============================================================================

static int32_t label_message_handler(qarma_control_t* control, qarma_message_t* msg) {
    if (!control || !msg) {
        return -1;
    }
    
    switch (msg->type) {
        case MSG_CREATE:
            // Label created
            break;
            
        case MSG_DESTROY:
            // Label being destroyed
            break;
            
        case MSG_PAINT:
            // Paint request
            control_invalidate(control);
            break;
            
        default:
            break;
    }
    
    return 0;
}

// ============================================================================
// Render Function
// ============================================================================

static void label_render(qarma_control_t* control, uint32_t* buffer,
                        int32_t buf_width, int32_t buf_height) {
    if (!control || !buffer) {
        return;
    }
    
    label_data_t* data = (label_data_t*)control->implementation_data;
    if (!data) {
        return;
    }
    
    // Get absolute position
    int32_t abs_x, abs_y;
    control_get_absolute_position(control, &abs_x, &abs_y);
    
    // Background color (if not transparent)
    if (!(control->style_flags & CONTROL_STYLE_TRANSPARENT)) {
        uint32_t bg_color = (control->background.alpha << 24) |
                           (control->background.red << 16) |
                           (control->background.green << 8) |
                           control->background.blue;
        
        for (int32_t y = 0; y < control->height; y++) {
            for (int32_t x = 0; x < control->width; x++) {
                int32_t screen_x = abs_x + x;
                int32_t screen_y = abs_y + y;
                
                if (screen_x >= 0 && screen_x < buf_width &&
                    screen_y >= 0 && screen_y < buf_height) {
                    buffer[screen_y * buf_width + screen_x] = bg_color;
                }
            }
        }
    }
    
    // TODO: Render text when font system is available
    // For now, text rendering is placeholder
}

// ============================================================================
// Cleanup Function
// ============================================================================

static void label_cleanup(qarma_control_t* control) {
    if (!control) {
        return;
    }
    
    // Label data will be freed by control_destroy
}

// ============================================================================
// Public API
// ============================================================================

qarma_control_t* label_create(qarma_frame_t* parent_frame, const char* name,
                              int32_t x, int32_t y, int32_t width, int32_t height,
                              const char* text) {
    // Create control
    qarma_control_t* control = control_create(parent_frame, CONTROL_TYPE_LABEL,
                                              name, x, y, width, height);
    if (!control) {
        return NULL;
    }
    
    // Allocate label data
    label_data_t* data = (label_data_t*)heap_alloc(sizeof(label_data_t));
    if (!data) {
        control_destroy(control);
        return NULL;
    }
    
    memset(data, 0, sizeof(label_data_t));
    
    if (text) {
        strncpy(data->text, text, sizeof(data->text) - 1);
        data->text[sizeof(data->text) - 1] = '\0';
    }
    
    data->alignment = LABEL_ALIGN_LEFT;
    data->word_wrap = false;
    
    // Set implementation data
    control->implementation_data = data;
    
    // Set handlers
    control->message_handler = label_message_handler;
    control->render_func = label_render;
    control->cleanup_func = label_cleanup;
    
    // Labels are typically not interactive
    control->state_flags &= ~CONTROL_STATE_ENABLED;
    
    // Set transparent by default
    control->style_flags |= CONTROL_STYLE_TRANSPARENT;
    
    return control;
}

void label_set_text(qarma_control_t* label, const char* text) {
    if (!label || label->type != CONTROL_TYPE_LABEL) {
        return;
    }
    
    label_data_t* data = (label_data_t*)label->implementation_data;
    if (!data || !text) {
        return;
    }
    
    strncpy(data->text, text, sizeof(data->text) - 1);
    data->text[sizeof(data->text) - 1] = '\0';
    
    control_invalidate(label);
}

const char* label_get_text(qarma_control_t* label) {
    if (!label || label->type != CONTROL_TYPE_LABEL) {
        return NULL;
    }
    
    label_data_t* data = (label_data_t*)label->implementation_data;
    if (!data) {
        return NULL;
    }
    
    return data->text;
}

void label_set_alignment(qarma_control_t* label, label_alignment_t alignment) {
    if (!label || label->type != CONTROL_TYPE_LABEL) {
        return;
    }
    
    label_data_t* data = (label_data_t*)label->implementation_data;
    if (!data) {
        return;
    }
    
    data->alignment = alignment;
    control_invalidate(label);
}

void label_set_word_wrap(qarma_control_t* label, bool wrap) {
    if (!label || label->type != CONTROL_TYPE_LABEL) {
        return;
    }
    
    label_data_t* data = (label_data_t*)label->implementation_data;
    if (!data) {
        return;
    }
    
    data->word_wrap = wrap;
    
    if (wrap) {
        label->style_flags |= CONTROL_STYLE_WORDWRAP;
    } else {
        label->style_flags &= ~CONTROL_STYLE_WORDWRAP;
    }
    
    control_invalidate(label);
}
