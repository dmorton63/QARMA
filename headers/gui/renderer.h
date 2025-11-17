#pragma once

#include "core/stdtools.h"

// ============================================================================
// GUI Rendering Utilities
// ============================================================================

// Visual constants
#define TEXTBOX_BG_COLOR        0xECF0F1
#define TEXTBOX_BORDER_COLOR    0x34495E
#define TEXTBOX_FOCUSED_BORDER  0x3498DB
#define BUTTON_BG_COLOR         0x3498DB
#define BUTTON_HOVER_COLOR      0x2980B9
#define BUTTON_PRESSED_COLOR    0x1F5F8B
#define CURSOR_COLOR            0x2C3E50
#define TEXT_COLOR              0x2C3E50

// Aliases for button controls
#define COLOR_BUTTON_BG         BUTTON_BG_COLOR
#define COLOR_BUTTON_HOVER      BUTTON_HOVER_COLOR
#define COLOR_BUTTON_PRESSED    BUTTON_PRESSED_COLOR
#define COLOR_BORDER            TEXTBOX_BORDER_COLOR
#define COLOR_TEXT              TEXT_COLOR
#define COLOR_FOCUS             TEXTBOX_FOCUSED_BORDER

// Drawing primitives
void draw_filled_rect(uint32_t* buffer, int buf_width, int x, int y, int w, int h, uint32_t color);
void draw_rect_border(uint32_t* buffer, int buf_width, int x, int y, int w, int h, uint32_t color, int thickness);
void draw_char_to_buffer(uint32_t* buffer, int buf_width, int x, int y, char c, uint32_t color);
void draw_string_to_buffer(uint32_t* buffer, int buf_width, int x, int y, const char* str, uint32_t color);

// Gradient rendering
void draw_vertical_gradient(uint32_t* buffer, int buf_width, int buf_height, uint32_t color_top, uint32_t color_bottom);
