/**
 * QARMA - Window Compositor Implementation
 * 
 * Manages window rendering, z-order, and mouse interaction.
 */

#include "window_compositor.h"
#include "graphics/graphics.h"
#include "core/memory/heap.h"
#include "core/string.h"

// Global compositor instance
static window_compositor_t g_compositor = {0};

// ────────────────────────────────────────────────────────────────────────────
// Initialization
// ────────────────────────────────────────────────────────────────────────────

void compositor_init(void) {
    memset(&g_compositor, 0, sizeof(window_compositor_t));
    g_compositor.next_z_order = 1;
}

window_compositor_t* get_compositor(void) {
    return &g_compositor;
}

// ────────────────────────────────────────────────────────────────────────────
// Default Styles
// ────────────────────────────────────────────────────────────────────────────

window_style_t compositor_get_default_style(void) {
    window_style_t style = {
        .title_bar_bg = {40, 40, 45, 255},      // Dark gray
        .title_bar_fg = {220, 220, 220, 255},   // Light gray text
        .border_color = {60, 60, 65, 255},      // Medium gray
        .shadow_color = {0, 0, 0, 100},         // Semi-transparent black
        .client_bg = {30, 30, 35, 255},         // Darker gray
        .has_shadow = true,
        .has_close_button = true,
        .has_minimize_button = false,
        .has_maximize_button = false
    };
    return style;
}

// ────────────────────────────────────────────────────────────────────────────
// Window Management
// ────────────────────────────────────────────────────────────────────────────

compositor_window_t* compositor_create_window(const char* title, int x, int y, int width, int height) {
    if (g_compositor.window_count >= QARMA_MAX_WINDOWS) {
        return NULL;
    }
    
    // Enforce minimum size
    if (width < WINDOW_MIN_WIDTH) width = WINDOW_MIN_WIDTH;
    if (height < WINDOW_MIN_HEIGHT) height = WINDOW_MIN_HEIGHT;
    
    compositor_window_t* win = (compositor_window_t*)heap_alloc(sizeof(compositor_window_t));
    if (!win) return NULL;
    
    memset(win, 0, sizeof(compositor_window_t));
    
    // Initialize base window handle
    win->base.id = qarma_generate_window_id();
    win->base.type = QARMA_WIN_TYPE_GENERIC;
    win->base.flags = QARMA_FLAG_VISIBLE | QARMA_FLAG_INTERACTIVE;
    win->base.x = x;
    win->base.y = y;
    win->base.size.width = width;
    win->base.size.height = height;
    win->base.title = title;
    win->base.alpha = 1.0f;
    
    // Initialize compositor-specific data
    win->state = WIN_STATE_NORMAL;
    win->style = compositor_get_default_style();
    win->z_order = g_compositor.next_z_order++;
    win->is_focused = false;
    win->is_dragging = false;
    win->on_render_content = NULL;
    
    // Add to compositor
    g_compositor.windows[g_compositor.window_count++] = win;
    
    // Focus new window
    compositor_focus_window(win);
    
    return win;
}

void compositor_destroy_window(compositor_window_t* win) {
    if (!win) return;
    
    // Remove from compositor array
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        if (g_compositor.windows[i] == win) {
            // Shift remaining windows
            for (uint32_t j = i; j < g_compositor.window_count - 1; j++) {
                g_compositor.windows[j] = g_compositor.windows[j + 1];
            }
            g_compositor.window_count--;
            break;
        }
    }
    
    // Clear focus if this window was focused
    if (g_compositor.focused_window == win) {
        g_compositor.focused_window = NULL;
    }
    
    heap_free(win);
}

void compositor_focus_window(compositor_window_t* win) {
    if (!win) return;
    
    // Unfocus all windows
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        g_compositor.windows[i]->is_focused = false;
    }
    
    // Focus this window
    win->is_focused = true;
    g_compositor.focused_window = win;
    
    // Raise to top
    compositor_raise_window(win);
}

void compositor_raise_window(compositor_window_t* win) {
    if (!win) return;
    
    // Find highest z-order
    int max_z = 0;
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        if (g_compositor.windows[i]->z_order > max_z) {
            max_z = g_compositor.windows[i]->z_order;
        }
    }
    
    // Set this window's z-order to top
    win->z_order = max_z + 1;
}

// ────────────────────────────────────────────────────────────────────────────
// Geometry and Hit Testing
// ────────────────────────────────────────────────────────────────────────────

bool compositor_point_in_window(compositor_window_t* win, int x, int y) {
    return (x >= win->base.x && 
            x < win->base.x + win->base.size.width &&
            y >= win->base.y && 
            y < win->base.y + win->base.size.height);
}

bool compositor_point_in_title_bar(compositor_window_t* win, int x, int y) {
    return (x >= win->base.x && 
            x < win->base.x + win->base.size.width &&
            y >= win->base.y && 
            y < win->base.y + WINDOW_TITLE_BAR_HEIGHT);
}

compositor_window_t* compositor_find_window_at(int x, int y) {
    // Search from top to bottom (highest z-order first)
    compositor_window_t* top_window = NULL;
    int max_z = -1;
    
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        compositor_window_t* win = g_compositor.windows[i];
        if (!(win->base.flags & QARMA_FLAG_VISIBLE)) continue;
        
        if (compositor_point_in_window(win, x, y)) {
            if (win->z_order > max_z) {
                max_z = win->z_order;
                top_window = win;
            }
        }
    }
    
    return top_window;
}

// ────────────────────────────────────────────────────────────────────────────
// Mouse Interaction
// ────────────────────────────────────────────────────────────────────────────

void compositor_handle_mouse(mouse_state_t* mouse) {
    if (!mouse) return;
    
    // Handle dragging
    if (g_compositor.dragging_window) {
        if (mouse->left_pressed) {
            // Continue dragging
            g_compositor.dragging_window->base.x = mouse->x - g_compositor.dragging_window->drag_offset_x;
            g_compositor.dragging_window->base.y = mouse->y - g_compositor.dragging_window->drag_offset_y;
            g_compositor.dragging_window->base.dirty = true;
        } else {
            // Release drag
            g_compositor.dragging_window->is_dragging = false;
            g_compositor.dragging_window->state = WIN_STATE_NORMAL;
            g_compositor.dragging_window = NULL;
        }
        return;
    }
    
    // Check for new drag start
    if (mouse->left_pressed) {
        compositor_window_t* win = compositor_find_window_at(mouse->x, mouse->y);
        if (win && compositor_point_in_title_bar(win, mouse->x, mouse->y)) {
            // Start dragging
            win->is_dragging = true;
            win->state = WIN_STATE_DRAGGING;
            win->drag_offset_x = mouse->x - win->base.x;
            win->drag_offset_y = mouse->y - win->base.y;
            g_compositor.dragging_window = win;
            compositor_focus_window(win);
        } else if (win) {
            // Just focus the window
            compositor_focus_window(win);
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Rendering
// ────────────────────────────────────────────────────────────────────────────

void compositor_render_title_bar(compositor_window_t* win) {
    int x = win->base.x;
    int y = win->base.y;
    int width = win->base.size.width;
    
    // Draw title bar background
    rgb_color_t bg = win->style.title_bar_bg;
    if (win->is_focused) {
        // Slightly brighter for focused window
        bg.red = (bg.red + 30 > 255) ? 255 : bg.red + 30;
        bg.green = (bg.green + 30 > 255) ? 255 : bg.green + 30;
        bg.blue = (bg.blue + 30 > 255) ? 255 : bg.blue + 30;
    }
    
    gfx_draw_filled_rectangle(x, y, width, WINDOW_TITLE_BAR_HEIGHT, bg);
    
    // Draw title text
    if (win->base.title) {
        gfx_draw_string(x + 8, y + 6, win->base.title, 
                       win->style.title_bar_fg, bg, NULL);
    }
    
    // Draw close button (X in top right)
    if (win->style.has_close_button) {
        int btn_x = x + width - 20;
        int btn_y = y + 4;
        rgb_color_t btn_color = {200, 50, 50, 255};  // Red
        gfx_draw_rectangle(btn_x, btn_y, 16, 16, btn_color);
        // Draw X
        gfx_draw_line(btn_x + 4, btn_y + 4, btn_x + 12, btn_y + 12, btn_color);
        gfx_draw_line(btn_x + 12, btn_y + 4, btn_x + 4, btn_y + 12, btn_color);
    }
}

void compositor_render_border(compositor_window_t* win) {
    int x = win->base.x;
    int y = win->base.y;
    int width = win->base.size.width;
    int height = win->base.size.height;
    
    // Draw border
    rgb_color_t border = win->style.border_color;
    if (win->is_focused) {
        // Highlight border for focused window
        border.red = (border.red + 40 > 255) ? 255 : border.red + 40;
        border.green = (border.green + 40 > 255) ? 255 : border.green + 40;
        border.blue = (border.blue + 80 > 255) ? 255 : border.blue + 80;
    }
    
    gfx_draw_rectangle(x, y, width, height, border);
}

void compositor_render_window(compositor_window_t* win) {
    if (!(win->base.flags & QARMA_FLAG_VISIBLE)) return;
    
    int x = win->base.x;
    int y = win->base.y;
    int width = win->base.size.width;
    int height = win->base.size.height;
    
    // Draw shadow (if enabled)
    if (win->style.has_shadow) {
        gfx_draw_filled_rectangle(
            x + WINDOW_SHADOW_OFFSET, 
            y + WINDOW_SHADOW_OFFSET,
            width, height, 
            win->style.shadow_color
        );
    }
    
    // Draw window background
    gfx_draw_filled_rectangle(x, y, width, height, win->style.client_bg);
    
    // Draw border
    compositor_render_border(win);
    
    // Draw title bar
    compositor_render_title_bar(win);
    
    // Draw client area content
    if (win->on_render_content) {
        int client_x = x + WINDOW_BORDER_WIDTH;
        int client_y = y + WINDOW_TITLE_BAR_HEIGHT;
        int client_w = width - 2 * WINDOW_BORDER_WIDTH;
        int client_h = height - WINDOW_TITLE_BAR_HEIGHT - WINDOW_BORDER_WIDTH;
        win->on_render_content(&win->base, client_x, client_y, client_w, client_h);
    }
}

void compositor_render_all(void) {
    // Sort windows by z-order (bubble sort for simplicity)
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        for (uint32_t j = i + 1; j < g_compositor.window_count; j++) {
            if (g_compositor.windows[i]->z_order > g_compositor.windows[j]->z_order) {
                compositor_window_t* temp = g_compositor.windows[i];
                g_compositor.windows[i] = g_compositor.windows[j];
                g_compositor.windows[j] = temp;
            }
        }
    }
    
    // Render from back to front
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        compositor_render_window(g_compositor.windows[i]);
    }
}
