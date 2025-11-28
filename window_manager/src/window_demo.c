/**
 * QARMA - Window System Demo
 * 
 * Demonstrates the window compositor with draggable windows.
 */

#include "window_compositor.h"
#include "console_compositor.h"
#include "graphics/graphics.h"
#include "core/input/mouse.h"

// Sample window content renderers
void render_hello_content(QARMA_WIN_HANDLE* win, int x, int y, int w, int h) {
    (void)win; (void)w; (void)h;
    
    rgb_color_t white = {255, 255, 255, 255};
    rgb_color_t bg = {30, 30, 35, 255};
    
    gfx_draw_string(x + 10, y + 10, "Hello, QARMA!", white, bg, NULL);
    gfx_draw_string(x + 10, y + 30, "This is a draggable window.", white, bg, NULL);
    gfx_draw_string(x + 10, y + 50, "Click the title bar to drag!", white, bg, NULL);
}

void render_info_content(QARMA_WIN_HANDLE* win, int x, int y, int w, int h) {
    (void)win; (void)w; (void)h;
    
    rgb_color_t cyan = {100, 200, 255, 255};
    rgb_color_t bg = {30, 30, 35, 255};
    
    gfx_draw_string(x + 10, y + 10, "Window System Info:", cyan, bg, NULL);
    gfx_draw_string(x + 10, y + 30, "- Draggable windows", cyan, bg, NULL);
    gfx_draw_string(x + 10, y + 50, "- Z-order management", cyan, bg, NULL);
    gfx_draw_string(x + 10, y + 70, "- Mouse interaction", cyan, bg, NULL);
}

void render_stats_content(QARMA_WIN_HANDLE* win, int x, int y, int w, int h) {
    (void)win; (void)w; (void)h;
    
    rgb_color_t green = {100, 255, 100, 255};
    rgb_color_t bg = {30, 30, 35, 255};
    
    window_compositor_t* comp = get_compositor();
    
    gfx_draw_string(x + 10, y + 10, "Compositor Stats:", green, bg, NULL);
    
    char buf[64];
    gfx_draw_string(x + 10, y + 30, "Windows: ", green, bg, NULL);
    // Simple number to string
    buf[0] = '0' + (comp->window_count / 10);
    buf[1] = '0' + (comp->window_count % 10);
    buf[2] = '\0';
    gfx_draw_string(x + 90, y + 30, buf, green, bg, NULL);
    
    gfx_draw_string(x + 10, y + 50, "Focused: ", green, bg, NULL);
    gfx_draw_string(x + 90, y + 50, comp->focused_window ? "Yes" : "No", green, bg, NULL);
}

void window_test_demo(void) {
    extern void serial_debug(const char* msg);
    serial_debug("[WINDOW_DEMO] Starting window_test_demo\n");
    
    // Compositor and console already initialized on boot
    // Just ensure console is visible
    serial_debug("[WINDOW_DEMO] Ensuring console is visible\n");
    extern void console_compositor_show(void);
    console_compositor_show();
    
    // Create main desktop window at top of screen
    serial_debug("[WINDOW_DEMO] Creating windows\n");
    extern uint32_t fb_width;
    int screen_w = (fb_width > 0) ? fb_width : 1024;
    
    compositor_window_t* main_win = compositor_create_window("QARMA Desktop", 0, 0, 300, 400);
    if (main_win) {
        main_win->on_render_content = render_hello_content;
    }
    
    // Create additional test windows
    compositor_window_t* win1 = compositor_create_window("Info Window", 200, 200, 280, 180);
    if (win1) {
        win1->on_render_content = render_info_content;
    }
    
    compositor_window_t* win2 = compositor_create_window("Stats", 450, 150, 250, 140);
    if (win2) {
        win2->on_render_content = render_stats_content;
    }
    
    // Render all windows (double buffered - will swap to display)
    serial_debug("[WINDOW_DEMO] Rendering all windows\n");
    compositor_render_all();
    serial_debug("[WINDOW_DEMO] Initial render complete\n");
}

// Mouse cursor rendering
void render_mouse_cursor(int x, int y) {
    rgb_color_t white = {255, 255, 255, 255};
    rgb_color_t black = {0, 0, 0, 255};
    
    // Simple arrow cursor (7x11 pixels)
    // Draw black outline first
    for (int dy = 0; dy < 12; dy++) {
        gfx_draw_pixel(x, y + dy, black);
    }
    for (int dx = 0; dx < 8; dx++) {
        gfx_draw_pixel(x + dx, y, black);
    }
    
    // Draw white fill
    for (int dy = 1; dy < 11; dy++) {
        int width = (dy < 6) ? dy : (11 - dy);
        for (int dx = 1; dx <= width && dx < 7; dx++) {
            gfx_draw_pixel(x + dx, y + dy, white);
        }
    }
}

// Update compositor with current mouse state
void window_update_mouse(void) {
    mouse_state_t* mouse = get_mouse_state();
    if (mouse) {
        // Update compositor with mouse interaction
        compositor_handle_mouse(mouse);
        
        // Re-render all windows
        compositor_render_all();
        
        // Render mouse cursor on top
        render_mouse_cursor(mouse->x, mouse->y);
    }
}
