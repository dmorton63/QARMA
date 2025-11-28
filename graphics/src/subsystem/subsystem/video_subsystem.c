#include "video_subsystem.h"
#include "core/stdtools.h"
#include "../framebuffer.h"
#include "../graphics.h"
#include "config.h"

// Forward declaration for gfx_print
extern void gfx_print(const char* text);

// Global variables
static video_subsystem_stats_t video_subsystem_stats = {0};
static uint32_t* framebuffer = NULL;
static uint32_t screen_pitch = 0;
static video_mode_t current_mode = VIDEO_MODE_TEXT;
static bool initialized = false;
static video_subsystem_config_t video_config = {
    .width = 800,
    .height = 600,
    .color_depth = 32,
    .fullscreen = false
};

void video_subsystem_init(subsystem_t *registry)
{
    if (initialized) {
        return;
    }
    
    SERIAL_LOG("VIDEO: Initializing video subsystem\n");
    gfx_print("Video subsystem: Initializing...\n");
    
    // Register with subsystem registry
    if (registry) {
        subsystem_register(registry, "video", VIDEO_SUBSYSTEM_ID);
    }
    
    // Initialize drivers and get framebuffer info
    video_subsystem_drivers_init();
    
    // Set initial mode to framebuffer
    video_subsystem_set_mode(VIDEO_MODE_FRAMEBUFFER);
    
    // Get existing framebuffer from graphics system
    extern uint32_t* fb_ptr;
    extern uint32_t fb_width, fb_height;
    
    if (fb_ptr) {
        framebuffer = fb_ptr;
        screen_pitch = fb_width * sizeof(uint32_t);
        video_config.width = fb_width;
        video_config.height = fb_height;
        SERIAL_LOG("VIDEO: Framebuffer connected\n");
        gfx_print("Video subsystem: Framebuffer connected\n");
    } else {
        SERIAL_LOG("VIDEO: WARNING - No framebuffer found\n");
        gfx_print("Video subsystem: WARNING - No framebuffer found\n");
    }
    
    initialized = true;
    SERIAL_LOG("VIDEO: Video subsystem initialized successfully\n");
    gfx_print("Video subsystem: Initialization complete\n");
}

void video_subsystem_test_function(void)
{
    video_subsystem_stats_t stats;
    video_subsystem_get_stats(&stats);
    video_subsystem_debug_info();
    video_subsystem_display_test_pattern();
}

void video_subsystem_get_stats(video_subsystem_stats_t* stats)
{
    if (stats) {
        *stats = video_subsystem_stats;
    }
}

void video_subsystem_configure(const video_subsystem_config_t* config)
{
    if (config) {
        video_config = *config;
    }
}

void video_subsystem_shutdown(void)
{
    SERIAL_LOG("VIDEO: Shutting down video subsystem\n");
    initialized = false;
    framebuffer = NULL;
}

void video_subsystem_render_frame(const video_framebuffer_t* frame)
{
    if (!frame || !framebuffer) {
        return;
    }
    
    // Basic frame rendering - copy pixel data to framebuffer
    for (uint32_t y = 0; y < frame->height && y < video_config.height; y++) {
        for (uint32_t x = 0; x < frame->width && x < video_config.width; x++) {
            uint32_t src_offset = y * frame->pitch + x;
            uint32_t dst_offset = y * video_config.width + x;
            framebuffer[dst_offset] = frame->pixel_data[src_offset];
        }
    }
    
    video_subsystem_stats.total_frames_rendered++;
}

void video_subsystem_drivers_init(void)
{
    SERIAL_LOG("VIDEO: Initializing video drivers\n");
    // Initialize graphics drivers if needed - pass NULL for multiboot_info
    graphics_init(NULL);
}

bool video_subsystem_set_mode(video_mode_t mode)
{
    SERIAL_LOG("VIDEO: Setting video mode\n");
    current_mode = mode;
    return true;
}

video_mode_t video_subsystem_get_mode(void)
{
    return current_mode;
}

void video_subsystem_debug_info(void)
{
    SERIAL_LOG("VIDEO: Debug Info\n");
    SERIAL_LOG("VIDEO: Stats - Frames rendered\n");
    if (framebuffer) {
        SERIAL_LOG("VIDEO: Framebuffer active\n");
    } else {
        SERIAL_LOG("VIDEO: ERROR - No framebuffer\n");
    }
}

void video_subsystem_display_test_pattern(void)
{
    if (!framebuffer) {
        SERIAL_LOG("VIDEO: Cannot display test pattern - no framebuffer\n");
        gfx_print("Video subsystem: Cannot display test pattern - no framebuffer\n");
        return;
    }
    
    SERIAL_LOG("VIDEO: Displaying test pattern\n");
    gfx_print("Video subsystem: Displaying test pattern...\n");
    
    // Create a simple test pattern
    for (uint32_t y = 0; y < video_config.height; y++) {
        for (uint32_t x = 0; x < video_config.width; x++) {
            uint32_t r = (x * 255) / video_config.width;
            uint32_t g = (y * 255) / video_config.height;
            uint32_t b = 128;
            uint32_t color = (r << 16) | (g << 8) | b;
            framebuffer[y * video_config.width + x] = color;
        }
    }
    
    SERIAL_LOG("VIDEO: Test pattern completed\n");
    gfx_print("Video subsystem: Test pattern complete\n");
}

uint32_t* video_subsystem_get_framebuffer(void)
{
    return framebuffer;
}

void video_subsystem_get_resolution(uint32_t* width, uint32_t* height)
{
    if (width) *width = video_config.width;
    if (height) *height = video_config.height;
}

uint32_t video_subsystem_get_color_depth(void)
{
    return video_config.color_depth;
}

void video_subsystem_clear_screen(void)
{
    if (!framebuffer) {
        return;
    }
    
    // Clear to black
    for (uint32_t i = 0; i < video_config.width * video_config.height; i++) {
        framebuffer[i] = 0x00000000;
    }
}

void video_subsystem_draw_pixel(uint32_t x, uint32_t y, rgb_color_t color)
{
    if (!framebuffer || x >= video_config.width || y >= video_config.height) {
        return;
    }
    
    uint32_t pixel_color = (color.red << 16) | (color.green << 8) | color.blue;
    framebuffer[y * video_config.width + x] = pixel_color;
}

rgb_color_t video_subsystem_get_pixel_color(uint32_t x, uint32_t y)
{
    rgb_color_t color = {0, 0, 0, 0};
    
    if (!framebuffer || x >= video_config.width || y >= video_config.height) {
        return color;
    }
    
    uint32_t pixel = framebuffer[y * video_config.width + x];
    color.red = (pixel >> 16) & 0xFF;
    color.green = (pixel >> 8) & 0xFF;
    color.blue = pixel & 0xFF;
    color.alpha = 0xFF;
    
    return color;
}

void video_subsystem_draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color)
{
    // Draw rectangle outline
    for (uint32_t i = 0; i < width; i++) {
        video_subsystem_draw_pixel(x + i, y, color);                    // top edge
        video_subsystem_draw_pixel(x + i, y + height - 1, color);      // bottom edge
    }
    
    for (uint32_t i = 0; i < height; i++) {
        video_subsystem_draw_pixel(x, y + i, color);                    // left edge
        video_subsystem_draw_pixel(x + width - 1, y + i, color);       // right edge
    }
}

void video_subsystem_draw_filled_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, rgb_color_t color)
{
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t i = 0; i < width; i++) {
            video_subsystem_draw_pixel(x + i, y + j, color);
        }
    }
}

// Font and text rendering stubs
void video_subsystem_draw_char(uint32_t x, uint32_t y, char c, rgb_color_t fg, rgb_color_t bg, font_t* font)
{
    // Stub implementation
    (void)x; (void)y; (void)c; (void)fg; (void)bg; (void)font;
}

void video_subsystem_draw_string(uint32_t x, uint32_t y, const char* str, rgb_color_t fg, rgb_color_t bg, font_t* font)
{
    // Stub implementation
    (void)x; (void)y; (void)str; (void)fg; (void)bg; (void)font;
}

font_t* video_subsystem_get_font(font_type_t type)
{
    // Stub implementation
    (void)type;
    return NULL;
}

void video_subsystem_set_font(font_t* font)
{
    // Stub implementation
    (void)font;
}

void video_subsystem_refresh_screen(void)
{
    // Stub implementation - would trigger display refresh
}

void video_subsystem_set_cursor_position(uint32_t x, uint32_t y)
{
    // Stub implementation
    (void)x; (void)y;
}

void video_subsystem_get_cursor_position(uint32_t* x, uint32_t* y)
{
    // Stub implementation
    if (x) *x = 0;
    if (y) *y = 0;
}

void video_subsystem_set_colors(color_t fg, color_t bg)
{
    // Stub implementation
    (void)fg; (void)bg;
}

void video_subsystem_scroll_up(void)
{
    // Stub implementation
}

// Splash screen functions using video subsystem
void video_subsystem_splash_clear(rgb_color_t color)
{
    if (!framebuffer) {
        return;
    }
    
    uint32_t pixel_color = (color.red << 16) | (color.green << 8) | color.blue;
    for (uint32_t i = 0; i < video_config.width * video_config.height; i++) {
        framebuffer[i] = pixel_color;
    }
}

void video_subsystem_splash_box(uint32_t width, uint32_t height, rgb_color_t color)
{
    if (!framebuffer) {
        return;
    }
    
    // Center the box on screen
    uint32_t x = (video_config.width - width) / 2;
    uint32_t y = (video_config.height - height) / 2;
    
    video_subsystem_draw_filled_rectangle(x, y, width, height, color);
}

void video_subsystem_splash_title(const char* text, rgb_color_t fg_color, rgb_color_t bg_color)
{
    if (!framebuffer || !text) {
        return;
    }
    
    // Use existing graphics text rendering functions
    // Get center position for the text
    uint32_t text_x = video_config.width / 2;
    uint32_t text_y = (video_config.height / 2) + 10;  // Slightly below center
    
    // Use the existing draw_scaled_text_centered function from graphics system
    extern void draw_scaled_text_centered(uint32_t x, uint32_t y, const char* text, uint32_t scale, rgb_color_t fg, rgb_color_t bg);
    draw_scaled_text_centered(text_x, text_y, text, 2, fg_color, bg_color);
}