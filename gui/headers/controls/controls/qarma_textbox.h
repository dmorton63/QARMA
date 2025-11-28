/**
 * QARMA - Hardened TextBox Control
 * 
 * Message-driven text input control.
 */

#ifndef QARMA_TEXTBOX_H
#define QARMA_TEXTBOX_H

#include "qarma_control.h"

// ============================================================================
// TextBox Structure
// ============================================================================

typedef struct {
    char text[256];
    uint32_t cursor_pos;
    uint32_t selection_start;
    uint32_t selection_end;
    bool password_mode;
    char password_char;
    uint32_t max_length;
    void (*on_changed)(qarma_control_t* textbox, void* user_data);
} textbox_data_t;

// ============================================================================
// TextBox API
// ============================================================================

/**
 * Create a textbox control.
 * 
 * @param parent_frame Parent frame
 * @param name TextBox name
 * @param x X position
 * @param y Y position
 * @param width TextBox width
 * @param height TextBox height
 * @return New textbox control or NULL on failure
 */
qarma_control_t* textbox_create(qarma_frame_t* parent_frame, const char* name,
                                int32_t x, int32_t y, int32_t width, int32_t height);

/**
 * Set textbox text.
 * 
 * @param textbox TextBox control
 * @param text New text
 */
void textbox_set_text(qarma_control_t* textbox, const char* text);

/**
 * Get textbox text.
 * 
 * @param textbox TextBox control
 * @return TextBox text
 */
const char* textbox_get_text(qarma_control_t* textbox);

/**
 * Set password mode.
 * 
 * @param textbox TextBox control
 * @param password true for password mode
 * @param password_char Character to display (e.g., '*')
 */
void textbox_set_password_mode(qarma_control_t* textbox, bool password, char password_char);

/**
 * Set maximum text length.
 * 
 * @param textbox TextBox control
 * @param max_length Maximum number of characters
 */
void textbox_set_max_length(qarma_control_t* textbox, uint32_t max_length);

/**
 * Set text changed handler.
 * 
 * @param textbox TextBox control
 * @param handler Change handler function
 */
void textbox_set_changed_handler(qarma_control_t* textbox,
                                 void (*handler)(qarma_control_t*, void*));

/**
 * Clear textbox content.
 * 
 * @param textbox TextBox control
 */
void textbox_clear(qarma_control_t* textbox);

#endif // QARMA_TEXTBOX_H
