#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "core/stdtools.h"
#include "graphics.h"
#include "qarma_win_handle/qarma_win_handle.h"

#define IN_BOUNDS(x, max) ((x) >= 0 && (x) < (int)(max))


typedef struct {
    uint32_t* buffer;
    int width;
    int height;
} FRAMEBUFFER_LAYER;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint8_t* address;
} FramebufferInfo;

extern FramebufferInfo* fb_info;
extern FramebufferInfo* fbinfo;

// Framebuffer functions
void framebuffer_init(void);
void framebuffer_putchar(char c);
void framebuffer_clear(void);
void framebuffer_blit_window(QARMA_WIN_HANDLE* win);
void framebuffer_compose_all(QARMA_WIN_HANDLE** windows, int count);
void framebuffer_set_cursor(uint32_t x, uint32_t y);
void framebuffer_scroll(void);
void framebuffer_blend_pixel(int x, int y, uint32_t src);
void splash_clear(rgb_color_t bg);
void splash_box(uint32_t w, uint32_t h, rgb_color_t color);
void splash_title(const char *text, rgb_color_t fg, rgb_color_t bg);
void framebuffer_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color);
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg);
void framebuffer_test(void);
rgb_color_t blend_color(rgb_color_t fg, rgb_color_t bg, float alpha) ;

void fb_draw_text(uint32_t x, uint32_t y, const char *text, rgb_color_t color);

void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text, rgb_color_t fg, rgb_color_t bg);

// VESA/GOP mode detection (placeholders for future implementation)
bool framebuffer_detect_vesa(void);
bool framebuffer_detect_gop(void);
void framebuffer_set_mode(uint32_t width, uint32_t height, uint32_t bpp);
void fb_draw_rect_to_buffer(uint32_t* buffer, QARMA_DIMENSION size, int x, int y, QARMA_DIMENSION buffSize, QARMA_COLOR color);
void fb_draw_rect(int x, int y, int width, int height, uint32_t color) ;
void fb_draw_rect_alpha(int x, int y, int width, int height, QARMA_COLOR color);
void fb_draw_rect_outline(int x, int y, int width, int height, uint32_t color);
uint32_t fb_get_pixel(int x, int y);
void draw_ellipse(int cx, int cy, int rx, int ry, float angle, rgb_color_t color);
void draw_circle(int cx, int cy, int radius, rgb_color_t color, rgb_color_t bg);
void draw_atom(int cx, int cy);
void draw_line(int x0, int y0, int x1, int y1, rgb_color_t color);
void draw_scaled_char(int x, int y, char c, int scale, rgb_color_t fg, rgb_color_t bg);
void draw_scaled_text(int x, int y, const char *text, int scale, rgb_color_t fg, rgb_color_t bg);
// Raw framebuffer pointer (used by some helpers)
extern uint32_t* fb_ptr;

// Compositor/backing store
void fb_compose(void);
void fb_mark_dirty(void);

extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint32_t fb_bpp;

#endif // FRAMEBUFFER_H
