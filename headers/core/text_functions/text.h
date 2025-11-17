// text.h

#ifndef TEXT_H
#define TEXT_H

#include "core/stdtools.h"

// Measure pixel width of text between two pointers [start, end)
int measure_text_pixel_width(const char* start, const char* end);

#endif // TEXT_H