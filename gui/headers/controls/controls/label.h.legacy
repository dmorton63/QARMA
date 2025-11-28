#pragma once

#include "../control_base.h"

// ============================================================================
// Label - Static text display control
// ============================================================================

typedef struct {
    ControlBase base;
    char text[256];
    uint32_t text_color;
    bool centered;
} Label;

// ============================================================================
// Label API
// ============================================================================

void label_init(Label* lbl, int x, int y, const char* text, uint32_t color);
void label_render(Label* lbl, uint32_t* buffer, int buf_width, int buf_height);
void label_set_text(Label* lbl, const char* text);
void label_set_color(Label* lbl, uint32_t color);
