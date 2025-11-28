// text.c

#include "text.h"
#include "graphics/font_data.h" // glyph metrics if available

#define FONT_WIDTH 8 // Fallback width if no metrics provided

// Measure pixel width of text in the half-open range [start, end)
int measure_text_pixel_width(const char* start, const char* end) {
    int width = 0;
    const char* p = start;
    while (p < end && *p != '\0') {
        // TODO: consult font_data for per-glyph widths if available
        width += FONT_WIDTH;
        p++;
    }
    return width;
}

