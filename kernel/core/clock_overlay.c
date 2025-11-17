#include "clock_overlay.h"
#include "graphics/framebuffer.h"
#include "graphics/graphics.h"
#include "config.h"
#include "core/sleep.h"
#include "core/timer.h"
#include "qarma_win_handle/qarma_win_handle.h"
#include "qarma_win_handle/qarma_win_factory.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "core/memory.h"

static uint32_t elapsed_seconds = 0;
static bool clock_initialized = false;
static bool clock_visible = true;

static const uint32_t clock_x = 700;
static const uint32_t clock_y = 10;
static const uint32_t clock_width = 80;
static const uint32_t clock_height = 20;

//static const rgb_color_t clock_bg = {0, 0, 0, 128};
static const uint32_t clock_bg_color = 0xFF008000; 

static const rgb_color_t clock_fg = {192, 255, 192, 255};  // pale green-white
static const rgb_color_t clock_bg = {0, 0, 0, 0};  // pale green-white
// static const uint32_t clock_width = 120;
// static const uint32_t clock_height = 32;
uint32_t ticks = 0;
uint32_t shadow_color = 0x202020;

static QARMA_WIN_VTABLE clock_overlay_vtable = {
    .init = NULL,
    .update = clock_overlay_update,
    .render = clock_overlay_render,
    .destroy = clock_overlay_destroy
};


static void draw_clock_box(void) {
    uint32_t border = 0x404040;

    // Shadow layer (optional depth)
    QARMA_COLOR shadow = {
        .r = (shadow_color >> 0) & 0xFF,
        .g = (shadow_color >> 8) & 0xFF,
        .b = (shadow_color >> 16) & 0xFF,
        .a = 0x80  // semi-transparent shadow
    };
    fb_draw_rect_alpha(clock_x + 4, clock_y + 4, clock_width, clock_height, shadow);

    // Main box fill
    QARMA_COLOR bg = {
        .r = (clock_bg_color >> 0) & 0xFF,
        .g = (clock_bg_color >> 8) & 0xFF,
        .b = (clock_bg_color >> 16) & 0xFF,
        .a = 0xC0  // translucent black
    };
    fb_draw_rect_alpha(clock_x, clock_y, clock_width, clock_height, bg);

    // Optional shadow offset (if you want a second layer)
    QARMA_COLOR offset_shadow = {
        .r = 0x10, .g = 0x10, .b = 0x10, .a = 0x40
    };
    fb_draw_rect_alpha(clock_x + 3, clock_y + 3, clock_width, clock_height, offset_shadow);

    // Border (still solid for clarity)
    fb_draw_rect_outline(clock_x, clock_y, clock_width, clock_height, border);
}

void format_time(char *buffer, uint32_t seconds) {
    int hrs = seconds / 3600;
    int mins = (seconds % 3600) / 60;
    int secs = seconds % 60;

    buffer[0] = '0' + (hrs / 10);
    buffer[1] = '0' + (hrs % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (mins / 10);
    buffer[4] = '0' + (mins % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (secs / 10);
    buffer[7] = '0' + (secs % 10);
    buffer[8] = '\0';
}

void draw_clock(void) {
    if (!clock_visible) return;
    draw_clock_box();
    char time_str[9];
    format_time(time_str, elapsed_seconds);

    fb_draw_text_with_bg(clock_x + 10, clock_y + 6, time_str, clock_fg, clock_bg);
}

void clock_tick(void) {
    static uint32_t last_ticks = 0;
    ticks = get_ticks();
    if ((ticks - last_ticks) >= 100) {  // 100 ticks = 1 second at 100Hz
        last_ticks = ticks;
        elapsed_seconds++;
        draw_clock();
    }

}

void reset_clock(void) {
    elapsed_seconds = 0;
    if (clock_initialized) draw_clock();
   // clock_loop();
}

void toggle_clock_visibility(void)
{
    clock_visible = !clock_visible;
    if (clock_visible) {
        draw_clock();
    } else {
        // Clear the clock area by redrawing the background
        for (uint32_t y = clock_y; y < clock_y + clock_height; y++) {
            for (uint32_t x = clock_x; x < clock_x + clock_width; x++) {
                framebuffer_draw_pixel(x, y, (rgb_color_t){0, 0, 0, 0}); // Assuming black background
            }
        }
    }
}


QARMA_WIN_HANDLE* clock_overlay_init(void) {
    QARMA_WIN_HANDLE* win = qarma_win_create_archetype(QARMA_WIN_TYPE_CLOCK_OVERLAY, "Clock Overlay", QARMA_FLAG_VISIBLE);
    if (!win) return NULL;

    win->id = qarma_generate_window_id();
    win->type = QARMA_WIN_TYPE_CLOCK_OVERLAY;
    win->title = "Clock Overlay";
    win->x = clock_x;
    win->y = clock_y;
    win->size.width = clock_width;
    win->size.height = clock_height;
    win->background = (QARMA_COLOR){0, 0, 0, 128};
    win->vtable = &clock_overlay_vtable;

    CLOCK_OVERLAY_TRAIT* trait = malloc(sizeof(CLOCK_OVERLAY_TRAIT));
    trait->elapsed_seconds = 0;
    trait->visible = true;
    win->traits = trait;
    gfx_print("Creating clock overlay window...\n");
    qarma_window_manager.add_window(&qarma_window_manager, win, "clock_overlay_init");
    return win;
}
void clock_loop() {
    while (1) {
        clock_tick();
        sleep_ms(500);
    }
}



void clock_overlay_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx) {
    (void)ctx;  // Unused parameter
    CLOCK_OVERLAY_TRAIT* trait = (CLOCK_OVERLAY_TRAIT*) self->traits;
    static uint32_t last_ticks = 0;
    uint32_t ticks = get_ticks();

    if ((ticks - last_ticks) >= QARMA_TICK_RATE) {
        last_ticks = ticks;
        trait->elapsed_seconds++;
        self->dirty = true;
    }
}

rgb_color_t qarma_to_rgb(QARMA_COLOR qc) {
    return (rgb_color_t){
        .red = qc.r,
        .green = qc.g,
        .blue = qc.b,
        .alpha = qc.a
    };
}

void clock_overlay_render(QARMA_WIN_HANDLE* self) {
    CLOCK_OVERLAY_TRAIT* trait = (CLOCK_OVERLAY_TRAIT*) self->traits;
    if (!trait->visible) return;

    QARMA_COLOR shadow = {32, 32, 32, 128};
    QARMA_COLOR bg = {0, 128, 0, 192};
    // QARMA_COLOR border = {64, 64, 64, 255};
    // QARMA_COLOR fg = {192, 255, 192, 255};

    fb_draw_rect_alpha(self->x + 4, self->y + 4, self->size.width, self->size.height, shadow);
    fb_draw_rect_alpha(self->x, self->y, self->size.width, self->size.height, bg);
    fb_draw_rect_outline(self->x, self->y, self->size.width, self->size.height, 0x404040);

    char time_str[9];
    format_time(time_str, trait->elapsed_seconds);

    fb_draw_text_with_bg(self->x + 10, self->y + 6, time_str, qarma_to_rgb((QARMA_COLOR){192, 255, 192, 255}), qarma_to_rgb((QARMA_COLOR){0, 0, 0, 0}));
}

void clock_overlay_destroy(QARMA_WIN_HANDLE* self) {
    if (self->traits) free(self->traits);
    free(self);
    fb_mark_dirty();
}

