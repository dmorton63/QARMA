/**
 * QARMA - Window Compositor
 * 
 * Manages window rendering, z-order, and mouse interaction.
 * Provides draggable windows with title bars and decorations.
 */

#pragma once

#include "qarma_win_handle.h"
#include "core/input/mouse.h"
#include "graphics/graphics.h"

// Window decoration constants
#define WINDOW_TITLE_BAR_HEIGHT    24
#define WINDOW_BORDER_WIDTH         2
#define WINDOW_MIN_WIDTH           120
#define WINDOW_MIN_HEIGHT           80
#define WINDOW_SHADOW_OFFSET        4

// Window states
typedef enum {
    WIN_STATE_NORMAL,
    WIN_STATE_MAXIMIZED,
    WIN_STATE_MINIMIZED,
    WIN_STATE_DRAGGING,
    WIN_STATE_RESIZING
} window_state_t;

// Window decoration style
typedef struct {
    rgb_color_t title_bar_bg;
    rgb_color_t title_bar_fg;
    rgb_color_t border_color;
    rgb_color_t shadow_color;
    rgb_color_t client_bg;
    bool has_shadow;
    bool has_close_button;
    bool has_minimize_button;
    bool has_maximize_button;
} window_style_t;

// Extended window handle with compositor data
typedef struct {
    QARMA_WIN_HANDLE base;          // Base window handle
    window_state_t state;            // Current state
    window_style_t style;            // Visual style
    int z_order;                     // Stacking order (higher = on top)
    bool is_focused;                 // Has keyboard focus
    bool is_dragging;                // Currently being dragged
    int drag_offset_x;               // Mouse offset during drag
    int drag_offset_y;
    void (*on_render_content)(struct QARMA_WIN_HANDLE* win, int x, int y, int w, int h);
} compositor_window_t;

// Compositor state
typedef struct {
    compositor_window_t* windows[QARMA_MAX_WINDOWS];
    uint32_t window_count;
    compositor_window_t* focused_window;
    compositor_window_t* dragging_window;
    int next_z_order;
} window_compositor_t;

// Initialize compositor
void compositor_init(void);

// Window management
compositor_window_t* compositor_create_window(const char* title, int x, int y, int width, int height);
void compositor_destroy_window(compositor_window_t* win);
void compositor_focus_window(compositor_window_t* win);
void compositor_raise_window(compositor_window_t* win);

// Mouse interaction
void compositor_handle_mouse(mouse_state_t* mouse);

// Rendering
void compositor_render_all(void);
void compositor_render_window(compositor_window_t* win);
void compositor_render_title_bar(compositor_window_t* win);
void compositor_render_border(compositor_window_t* win);

// Utility
bool compositor_point_in_window(compositor_window_t* win, int x, int y);
bool compositor_point_in_title_bar(compositor_window_t* win, int x, int y);
compositor_window_t* compositor_find_window_at(int x, int y);

// Default styles
window_style_t compositor_get_default_style(void);

// Get global compositor instance
window_compositor_t* get_compositor(void);
