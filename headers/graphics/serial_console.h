#pragma once

#include "graphics/graphics.h"

static bool serial_is_transmit_ready(void);
void serial_console_init(void);
// Send a single character to serial port
static void serial_write_char(char c);
void serial_putchar(char c);

void serial_clear(void);
char serial_read_char(void);
bool serial_has_data(void);
void serial_set_colors(color_t fg, color_t bg);
void serial_print_color(const char* str, color_t fg, color_t bg) ;
void serial_scroll(void);
void serial_set_cursor(uint32_t x, uint32_t y);
