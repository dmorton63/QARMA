/**
 * QARMA - Hardened Label Control
 * 
 * Message-driven label control for displaying text.
 */

#ifndef QARMA_LABEL_H
#define QARMA_LABEL_H

#include "qarma_control.h"

// ============================================================================
// Label Alignment
// ============================================================================

typedef enum {
    LABEL_ALIGN_LEFT,
    LABEL_ALIGN_CENTER,
    LABEL_ALIGN_RIGHT
} label_alignment_t;

// ============================================================================
// Label Structure
// ============================================================================

typedef struct {
    char text[256];
    label_alignment_t alignment;
    bool word_wrap;
} label_data_t;

// ============================================================================
// Label API
// ============================================================================

/**
 * Create a label control.
 * 
 * @param parent_frame Parent frame
 * @param name Label name
 * @param x X position
 * @param y Y position
 * @param width Label width
 * @param height Label height
 * @param text Label text
 * @return New label control or NULL on failure
 */
qarma_control_t* label_create(qarma_frame_t* parent_frame, const char* name,
                              int32_t x, int32_t y, int32_t width, int32_t height,
                              const char* text);

/**
 * Set label text.
 * 
 * @param label Label control
 * @param text New text
 */
void label_set_text(qarma_control_t* label, const char* text);

/**
 * Get label text.
 * 
 * @param label Label control
 * @return Label text
 */
const char* label_get_text(qarma_control_t* label);

/**
 * Set label text alignment.
 * 
 * @param label Label control
 * @param alignment Text alignment
 */
void label_set_alignment(qarma_control_t* label, label_alignment_t alignment);

/**
 * Set word wrap.
 * 
 * @param label Label control
 * @param wrap true to enable word wrap
 */
void label_set_word_wrap(qarma_control_t* label, bool wrap);

#endif // QARMA_LABEL_H
