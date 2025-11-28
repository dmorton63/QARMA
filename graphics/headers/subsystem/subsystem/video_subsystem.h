#pragma once

#include "core/stdtools.h"
#include "core/scheduler/subsystem_registry.h"
#include "../graphics.h"
#include "../font_data.h"

typedef struct {
    uint32_t total_frames_rendered;
    uint32_t current_fps;
    uint32_t max_fps;
    uint32_t min_fps;
    uint32_t avg_frame_time_ms;
} video_subsystem_stats_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t color_depth;
    bool fullscreen;
} video_subsystem_config_t;

typedef enum {
    VIDEO_MODE_TEXT,
    VIDEO_MODE_FRAMEBUFFER,
    VIDEO_MODE_GRAPHICS_ACCELERATED
} video_mode_t;

typedef struct {
    uint32_t* pixel_data;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} video_framebuffer_t;

#define VIDEO_SUBSYSTEM_ID 0x02

/* Video Subsystem Initialization */
void video_subsystem_init(subsystem_t* registry);
/* Video Subsystem Statistics Retrieval */
void video_subsystem_get_stats(video_subsystem_stats_t* stats);
/* Video Subsystem Configuration */
void video_subsystem_configure(const video_subsystem_config_t* config);
/* Video Subsystem Shutdown */
void video_subsystem_shutdown(void);
/* Video Subsystem Frame Rendering */
void video_subsystem_render_frame(const video_framebuffer_t* frame);
/* Video Subsystem Driver Initialization */
void video_subsystem_drivers_init(void);
/* Video Subsystem Mode Setting */
bool video_subsystem_set_mode(video_mode_t mode);
/* Video Subsystem Current Mode Retrieval */
video_mode_t video_subsystem_get_mode(void);
/* Video Subsystem Debug Information */
void video_subsystem_debug_info(void);
/* Video Subsystem Test Pattern Display */
void video_subsystem_display_test_pattern(void);
/* Video Subsystem Framebuffer Access */
uint32_t* video_subsystem_get_framebuffer(void);
/* Video Subsystem Resolution Retrieval */
void video_subsystem_get_resolution(uint32_t* width, uint32_t* height);
/* Video Subsystem Color Depth Retrieval */
uint32_t video_subsystem_get_color_depth(void);
/* Video Subsystem Clear Screen */
void video_subsystem_clear_screen(void);
/* Video Subsystem Draw Rectangle */
void video_subsystem_draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color);
/* Video Subsystem Draw Filled Rectangle */
void video_subsystem_draw_filled_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color);
/* Video Subsystem Draw Character */
void video_subsystem_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg, font_t* font);
/* Video Subsystem Draw String */
void video_subsystem_draw_string(uint32_t x, uint32_t y, const char* str, rgb_color_t fg, rgb_color_t bg, font_t* font);
/* Video Subsystem Font Management */
font_t* video_subsystem_get_font(font_type_t type);
/* Video Subsystem Set Font */
void video_subsystem_set_font(font_t* font);
/* Video Subsystem Refresh Screen */
void video_subsystem_refresh_screen(void);
/* Video Subsystem Set Cursor Position */
void video_subsystem_set_cursor_position(uint32_t x, uint32_t y);
/* Video Subsystem Get Cursor Position */
void video_subsystem_get_cursor_position(uint32_t* x, uint32_t* y); 
/* Video Subsystem Set Colors */
void video_subsystem_set_colors(color_t fg, color_t bg);
/* Video Subsystem Scroll Up */
void video_subsystem_scroll_up(void);
/* Video Subsystem Draw Pixel */
void video_subsystem_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color);
/* Video Subsystem Get Pixel Color */
rgb_color_t video_subsystem_get_pixel_color(uint32_t x, uint32_t y);
/* Video Subsystem Test Function */
void video_subsystem_test_function(void);

/* Video Subsystem Splash Screen Functions */
void video_subsystem_splash_clear(rgb_color_t color);
void video_subsystem_splash_box(uint32_t width, uint32_t height, rgb_color_t color);
void video_subsystem_splash_title(const char* text, rgb_color_t fg_color, rgb_color_t bg_color);