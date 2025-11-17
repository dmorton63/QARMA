/**
 * QARMA - Font Data Definitions
 * 8x8 bitmap font data for text rendering in graphics modes
 */

#ifndef QUANTUM_FONT_DATA_H
#define QUANTUM_FONT_DATA_H

#include "core/kernel.h"

// Font constants
#define FONT_WIDTH 8
#define FONT_HEIGHT 8
#define FONT_CHARS_COUNT 128

// Font type enumeration
typedef enum {
    FONT_VGA_DEFAULT = 0,    // Standard VGA ASCII font
    FONT_BOX_DRAWING = 1,    // Box drawing characters
    FONT_CONTROL = 2         // Control characters
} font_type_t;

// Font descriptor structure
typedef struct {
    const uint8_t (*data)[FONT_HEIGHT];  // Pointer to font bitmap data
    uint8_t width;                       // Character width in pixels
    uint8_t height;                      // Character height in pixels  
    uint8_t char_spacing;                // Horizontal spacing between characters
    uint8_t line_spacing;                // Vertical spacing between lines
    uint32_t char_count;                 // Number of characters in font
} font_descriptor_t;

// Font data declarations - 8x8 bitmap fonts
extern const uint8_t vga_font[FONT_CHARS_COUNT][FONT_HEIGHT];
extern const uint8_t font8x8_box[FONT_CHARS_COUNT][FONT_HEIGHT];
extern const uint8_t font8x8_control[FONT_CHARS_COUNT][FONT_HEIGHT];

// Font descriptor instances
extern const font_descriptor_t font_vga_8x8;
extern const font_descriptor_t font_box_8x8;

// Font management functions
font_descriptor_t* get_font(font_type_t type);
uint8_t get_char_bitmap_row(char c, uint8_t row, font_type_t font_type);

// Function that takes color parameters - implementation will include graphics.h for full types

#endif // QUANTUM_FONT_DATA_H