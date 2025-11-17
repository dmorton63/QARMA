/**
 * QARMA - Window System Demo
 * 
 * Demonstrates the window compositor with draggable windows.
 */

#include "window_compositor.h"
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
    gfx_print("\n╔═══════════════════════════════════════╗\n");
    gfx_print("║      Window System Demo Test         ║\n");
    gfx_print("╚═══════════════════════════════════════╝\n\n");
    
    // Initialize compositor if not already done
    static bool initialized = false;
    if (!initialized) {
        compositor_init();
        initialized = true;
        gfx_print("Compositor initialized.\n");
    }
    
    // Create three test windows
    gfx_print("Creating test windows...\n");
    
    compositor_window_t* win1 = compositor_create_window("Hello Window", 100, 100, 300, 150);
    if (win1) {
        win1->on_render_content = render_hello_content;
        gfx_print("  Created window 1: Hello Window\n");
    }
    
    compositor_window_t* win2 = compositor_create_window("Info Window", 200, 200, 280, 180);
    if (win2) {
        win2->on_render_content = render_info_content;
        gfx_print("  Created window 2: Info Window\n");
    }
    
    compositor_window_t* win3 = compositor_create_window("Stats", 450, 150, 250, 140);
    if (win3) {
        win3->on_render_content = render_stats_content;
        gfx_print("  Created window 3: Stats Window\n");
    }
    
    gfx_print("\nWindows created successfully!\n");
    gfx_print("Total windows: ");
    window_compositor_t* comp = get_compositor();
    char buf[8];
    buf[0] = '0' + comp->window_count;
    buf[1] = '\0';
    gfx_print(buf);
    gfx_print("\n\n");
    
    // Render all windows
    gfx_print("Rendering windows...\n");
    compositor_render_all();
    
    gfx_print("\n╔═══════════════════════════════════════╗\n");
    gfx_print("║    Windows are now visible!          ║\n");
    gfx_print("║    Use mouse to drag title bars      ║\n");
    gfx_print("╚═══════════════════════════════════════╝\n\n");
    
    gfx_print("Mouse should be active - try dragging windows!\n");
    gfx_print("Windows persist until destroyed.\n");
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
