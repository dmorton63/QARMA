#include "label.h"
#include "../renderer.h"
#include "core/string.h"

// ============================================================================
// Label Implementation
// ============================================================================

void label_init(Label* lbl, int x, int y, const char* text, uint32_t color) {
    if (!lbl) return;
    
    lbl->base.x = x;
    lbl->base.y = y;
    lbl->base.width = text ? strlen(text) * 8 + 10 : 100;
    lbl->base.height = 20;
    lbl->base.visible = true;
    lbl->base.enabled = true;
    lbl->base.id = control_generate_id();
    
    if (text) {
        strncpy(lbl->text, text, 255);
        lbl->text[255] = '\0';
    } else {
        lbl->text[0] = '\0';
    }
    
    lbl->text_color = color;
    lbl->centered = false;
}

void label_render(Label* lbl, uint32_t* buffer, int buf_width, int buf_height) {
    if (!lbl || !buffer || !lbl->base.visible) return;
    
    int x = lbl->base.x;
    int y = lbl->base.y;
    int w = lbl->base.width;
    int h = lbl->base.height;
    
    // Draw text
    int text_x = lbl->centered ? (x + (w - strlen(lbl->text) * 8) / 2) : (x + 5);
    int text_y = y + (h - 8) / 2;
    draw_string_to_buffer(buffer, buf_width, text_x, text_y, lbl->text, lbl->text_color);
}

void label_set_text(Label* lbl, const char* text) {
    if (lbl && text) {
        strncpy(lbl->text, text, 255);
        lbl->text[255] = '\0';
    }
}

void label_set_color(Label* lbl, uint32_t color) {
    if (lbl) {
        lbl->text_color = color;
    }
}
