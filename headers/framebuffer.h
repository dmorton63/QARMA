#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

void fb_clear(void);
void fb_write_char(char c);
void fb_write_string(const char* str);
void fb_set_color(uint8_t fg, uint8_t bg);

#endif