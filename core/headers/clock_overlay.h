#pragma once
#include "core/stdtools.h"
#include "graphics/framebuffer.h"
#include "qarma_win_handle/qarma_win_handle.h"

#define COLOR_CLOCK_TEXT 0x00FF00  // Bright green
#define COLOR_CLOCK_BG 0x000000    // Black background

typedef struct {
    uint32_t elapsed_seconds;
    bool visible;
} CLOCK_OVERLAY_TRAIT;


void clock_tick(void);
void draw_clock(void);
void reset_clock(void);
void toggle_clock_visibility(void);
QARMA_WIN_HANDLE*  clock_overlay_init(void);
void clock_loop();
void clock_overlay_update(QARMA_WIN_HANDLE *self, QARMA_TICK_CONTEXT *ctx);
rgb_color_t qarma_to_rgb(QARMA_COLOR qc);
void clock_overlay_render(QARMA_WIN_HANDLE *self);
void clock_overlay_destroy(QARMA_WIN_HANDLE *self);
// Show/hide clock overlay