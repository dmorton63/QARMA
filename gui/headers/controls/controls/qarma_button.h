/**
 * QARMA - Hardened Button Control
 * 
 * Message-driven button control integrated with unified control system.
 */

#ifndef QARMA_BUTTON_H
#define QARMA_BUTTON_H

#include "qarma_control.h"

// ============================================================================
// Button Structure
// ============================================================================

typedef struct {
    char text[64];
    bool is_default;                // Is this the default button (activated by Enter)
    void (*on_click)(qarma_control_t* button, void* user_data);
} button_data_t;

// ============================================================================
// Button API
// ============================================================================

/**
 * Create a button control.
 * 
 * @param parent_frame Parent frame
 * @param name Button name
 * @param x X position
 * @param y Y position
 * @param width Button width
 * @param height Button height
 * @param text Button text
 * @return New button control or NULL on failure
 */
qarma_control_t* button_create(qarma_frame_t* parent_frame, const char* name,
                               int32_t x, int32_t y, int32_t width, int32_t height,
                               const char* text);

/**
 * Set button text.
 * 
 * @param button Button control
 * @param text New text
 */
void button_set_text(qarma_control_t* button, const char* text);

/**
 * Get button text.
 * 
 * @param button Button control
 * @return Button text
 */
const char* button_get_text(qarma_control_t* button);

/**
 * Set button click handler.
 * 
 * @param button Button control
 * @param handler Click handler function
 */
void button_set_click_handler(qarma_control_t* button,
                              void (*handler)(qarma_control_t*, void*));

/**
 * Set whether this is the default button.
 * 
 * @param button Button control
 * @param is_default true for default button
 */
void button_set_default(qarma_control_t* button, bool is_default);

/**
 * Programmatically click the button.
 * 
 * @param button Button control
 */
void button_click(qarma_control_t* button);

#endif // QARMA_BUTTON_H
