/**
 * QARMA - Window Compositor Implementation
 * 
 * Manages window rendering, z-order, and mouse interaction.
 */

#include "window_compositor.h"
#include "graphics/graphics.h"
#include "graphics/framebuffer.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "core/input/mouse.h"

// External framebuffer info
extern uint32_t fb_width;
extern uint32_t fb_height;
extern mouse_state_t mouse_state;

// Global compositor instance
static window_compositor_t g_compositor = {0};
static bool g_z_order_dirty = true;
static bool g_compositor_enabled = true;  // Disabled during boot messages

// ────────────────────────────────────────────────────────────────────────────
// Initialization
// ────────────────────────────────────────────────────────────────────────────

void compositor_init(void) {
    memset(&g_compositor, 0, sizeof(window_compositor_t));
    g_compositor.next_z_order = 1;
    g_compositor_enabled = true;  // Enable by default
}

void compositor_set_enabled(bool enabled) {
    extern void serial_debug(const char* msg);
    g_compositor_enabled = enabled;
    serial_debug("[COMPOSITOR] Rendering ");
    serial_debug(enabled ? "ENABLED" : "DISABLED");
    serial_debug("\n");
}

bool compositor_is_enabled(void) {
    return g_compositor_enabled;
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
    g_z_order_dirty = true;  // Mark for re-sort on next render
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

// Helper: Check if point is in close button
static bool compositor_point_in_close_button(compositor_window_t* win, int px, int py) {
    if (!win->style.has_close_button) return false;
    
    int btn_x = win->base.x + win->base.size.width - 20;
    int btn_y = win->base.y + 4;
    int btn_w = 16;
    int btn_h = 16;
    
    return (px >= btn_x && px < btn_x + btn_w &&
            py >= btn_y && py < btn_y + btn_h);
}

void compositor_handle_mouse(mouse_state_t* mouse) {
    if (!mouse) return;
    
    // Handle dragging
    if (g_compositor.dragging_window) {
        if (mouse->left_pressed) {
            // Continue dragging
            compositor_window_t* win = g_compositor.dragging_window;
            int new_x = mouse->x - win->drag_offset_x;
            int new_y = mouse->y - win->drag_offset_y;
            
            // Clamp window position to screen bounds
            // Prevent negative coordinates which cause rendering issues
            int min_x = 0;  // Don't allow window to go off left edge
            int max_x = (int)fb_width - 100;  // Keep at least 100px visible on right
            int min_y = 0;  // Don't allow dragging above screen
            int max_y = (int)fb_height - WINDOW_TITLE_BAR_HEIGHT;
            
            if (new_x < min_x) new_x = min_x;
            if (new_x > max_x) new_x = max_x;
            if (new_y < min_y) new_y = min_y;
            if (new_y > max_y) new_y = max_y;
            
            // Only mark dirty if position actually changed
            if (win->base.x != new_x || win->base.y != new_y) {
                win->base.x = new_x;
                win->base.y = new_y;
                win->base.dirty = true;
            }
        } else {
            // Release drag
            g_compositor.dragging_window->is_dragging = false;
            g_compositor.dragging_window->state = WIN_STATE_NORMAL;
            g_compositor.dragging_window = NULL;
        }
        return;
    }
    
    // Check for mouse click
    static bool was_pressed = false;
    bool clicked = !mouse->left_pressed && was_pressed;  // Button released
    was_pressed = mouse->left_pressed;
    
    if (clicked) {
        // Check for close button click
        compositor_window_t* win = compositor_find_window_at(mouse->x, mouse->y);
        if (win && compositor_point_in_close_button(win, mouse->x, mouse->y)) {
            gfx_print("Closing window: ");
            gfx_print(win->base.title ? win->base.title : "(untitled)");
            gfx_print("\n");
            compositor_destroy_window(win);
            return;
        }
    }
    
    // Check for new drag start
    if (mouse->left_pressed) {
        compositor_window_t* win = compositor_find_window_at(mouse->x, mouse->y);
        if (win && compositor_point_in_title_bar(win, mouse->x, mouse->y)) {
            // Don't drag if clicking close button
            if (!compositor_point_in_close_button(win, mouse->x, mouse->y)) {
                // Start dragging
                win->is_dragging = true;
                win->state = WIN_STATE_DRAGGING;
                win->drag_offset_x = mouse->x - win->base.x;
                win->drag_offset_y = mouse->y - win->base.y;
                g_compositor.dragging_window = win;
                compositor_focus_window(win);
            }
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
        rgb_color_t btn_bg = {180, 40, 40, 255};  // Dark red background
        rgb_color_t btn_fg = {255, 255, 255, 255};  // White X
        
        // Draw button background (filled rectangle)
        gfx_draw_filled_rectangle(btn_x, btn_y, 16, 16, btn_bg);
        
        // Draw X with thicker lines (2px wide each stroke)
        // Top-left to bottom-right diagonal
        gfx_draw_line(btn_x + 4, btn_y + 4, btn_x + 12, btn_y + 12, btn_fg);
        gfx_draw_line(btn_x + 5, btn_y + 4, btn_x + 13, btn_y + 12, btn_fg);
        // Top-right to bottom-left diagonal  
        gfx_draw_line(btn_x + 12, btn_y + 4, btn_x + 4, btn_y + 12, btn_fg);
        gfx_draw_line(btn_x + 11, btn_y + 4, btn_x + 3, btn_y + 12, btn_fg);
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

// Cursor rendering control
static bool cursor_enabled = true;
static int last_cursor_x = -1;
static int last_cursor_y = -1;

// Background buffer for cursor (15x20 pixels)
#define CURSOR_BG_WIDTH 15
#define CURSOR_BG_HEIGHT 20
static rgb_color_t cursor_bg_buffer[CURSOR_BG_HEIGHT][CURSOR_BG_WIDTH];
static bool cursor_bg_saved = false;

// Forward declaration
static void compositor_render_cursor(int x, int y);
static void compositor_save_cursor_background(int x, int y);
static void compositor_restore_cursor_background(int x, int y);

// Helper to get pixel as rgb_color_t
static rgb_color_t framebuffer_get_pixel(int x, int y) {
    extern uint32_t fb_get_pixel(int x, int y);
    uint32_t pixel = fb_get_pixel(x, y);
    rgb_color_t color;
    color.blue = (pixel >> 0) & 0xFF;
    color.green = (pixel >> 8) & 0xFF;
    color.red = (pixel >> 16) & 0xFF;
    color.alpha = 255;
    return color;
}

void compositor_set_cursor_enabled(bool enabled) {
    cursor_enabled = enabled;
}

// Save background under cursor
static void compositor_save_cursor_background(int x, int y) {
    if (x < 0 || y < 0) return;
    
    for (int dy = 0; dy < CURSOR_BG_HEIGHT; dy++) {
        for (int dx = 0; dx < CURSOR_BG_WIDTH; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < (int)fb_width && py >= 0 && py < (int)fb_height) {
                cursor_bg_buffer[dy][dx] = framebuffer_get_pixel(px, py);
            } else {
                cursor_bg_buffer[dy][dx] = (rgb_color_t){0, 0, 0, 0};
            }
        }
    }
    cursor_bg_saved = true;
}

// Restore background under cursor
static void compositor_restore_cursor_background(int x, int y) {
    if (x < 0 || y < 0 || !cursor_bg_saved) return;
    
    for (int dy = 0; dy < CURSOR_BG_HEIGHT; dy++) {
        for (int dx = 0; dx < CURSOR_BG_WIDTH; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < (int)fb_width && py >= 0 && py < (int)fb_height) {
                framebuffer_draw_pixel(px, py, cursor_bg_buffer[dy][dx]);
            }
        }
    }
}

// Lightweight cursor-only rendering (doesn't redraw windows)
void compositor_render_cursor_only(int x, int y) {
    if (cursor_enabled) {
        // Restore old cursor position background to backing store
        if (last_cursor_x >= 0 && last_cursor_y >= 0) {
            compositor_restore_cursor_background(last_cursor_x, last_cursor_y);
        }
        
        // Save background at new position from backing store
        compositor_save_cursor_background(x, y);
        
        // Draw new cursor to backing store
        compositor_render_cursor(x, y);
        
        // Remember position
        last_cursor_x = x;
        last_cursor_y = y;
        
        // DOUBLE BUFFERING: Swap to visible framebuffer
        extern void framebuffer_swap(void);
        framebuffer_swap();
    }
}

// Render mouse cursor
static void compositor_render_cursor(int x, int y) {
    if (!cursor_enabled) return;
    
    // Bounds check
    if (x < 0 || y < 0 || x >= (int)fb_width - 15 || y >= (int)fb_height - 20) {
        return; // Don't render cursor near edges to avoid overflow
    }
    
    // Simple arrow cursor (11x16 pixels)
    const int cursor_size = 11;
    rgb_color_t white = {255, 255, 255, 255};
    rgb_color_t black = {0, 0, 0, 255};
    
    // Draw cursor outline (black border)
    for (int dy = 0; dy < cursor_size + 4; dy++) {
        int width = (cursor_size + 4 - dy) / 2;
        if (width < 1) width = 1;
        for (int dx = 0; dx < width; dx++) {
            if (x + dx >= 0 && x + dx < (int)fb_width && 
                y + dy >= 0 && y + dy < (int)fb_height) {
                framebuffer_draw_pixel(x + dx, y + dy, black);
            }
        }
    }
    
    // Draw cursor fill (white)
    for (int dy = 1; dy < cursor_size + 2; dy++) {
        int width = (cursor_size + 2 - dy) / 2;
        if (width < 1) break;
        for (int dx = 1; dx < width - 1; dx++) {
            if (x + dx >= 0 && x + dx < (int)fb_width && 
                y + dy >= 0 && y + dy < (int)fb_height) {
                framebuffer_draw_pixel(x + dx, y + dy, white);
            }
        }
    }
}

void compositor_render_all(void) {
    extern void serial_debug(const char* msg);
    serial_debug("[COMPOSITOR_RENDER] Called\n");
    
    // Check if compositor is enabled (disabled during boot messages)
    if (!g_compositor_enabled) {
        serial_debug("[COMPOSITOR_RENDER] Skipped (disabled)\n");
        return;
    }
    
    // Safety check: Prevent potential issues with uninitialized framebuffer
    extern uint32_t* backing_store;
    if (!backing_store) {
        serial_debug("[COMPOSITOR_RENDER] ERROR: backing_store is NULL\n");
        return;
    }
    
    if (fb_width == 0 || fb_height == 0) {
        serial_debug("[COMPOSITOR_RENDER] ERROR: Invalid framebuffer dimensions\n");
        return;
    }
    
    serial_debug("[COMPOSITOR_RENDER] Clearing background\n");
    
    // CRITICAL: Clear backing store to desktop background first
    // This prevents window trails during dragging
    extern void fb_draw_rect(int x, int y, int width, int height, uint32_t color);
    rgb_color_t desktop_bg = {176, 196, 222, 255};  // Powder blue
    uint32_t bg_color = (desktop_bg.alpha << 24) | (desktop_bg.red << 16) | 
                        (desktop_bg.green << 8) | desktop_bg.blue;
    fb_draw_rect(0, 0, fb_width, fb_height, bg_color);
    
    serial_debug("[COMPOSITOR_RENDER] Background cleared\n");
    
    serial_debug("[COMPOSITOR_RENDER] Sorting windows\n");
    
    // Sort windows by z-order (optimized - only sort if needed)
    if (g_z_order_dirty && g_compositor.window_count > 1) {
        // Bubble sort for simplicity
        for (uint32_t i = 0; i < g_compositor.window_count - 1; i++) {
            bool swapped = false;
            for (uint32_t j = 0; j < g_compositor.window_count - 1 - i; j++) {
                if (g_compositor.windows[j]->z_order > g_compositor.windows[j + 1]->z_order) {
                    compositor_window_t* temp = g_compositor.windows[j];
                    g_compositor.windows[j] = g_compositor.windows[j + 1];
                    g_compositor.windows[j + 1] = temp;
                    swapped = true;
                }
            }
            if (!swapped) break;  // Early exit if already sorted
        }
        g_z_order_dirty = false;
    }
    
    serial_debug("[COMPOSITOR_RENDER] Rendering windows\n");
    
    // Render from back to front to backing store
    for (uint32_t i = 0; i < g_compositor.window_count; i++) {
        compositor_render_window(g_compositor.windows[i]);
    }
    
    serial_debug("[COMPOSITOR_RENDER] Windows rendered\n");
    
    // Invalidate cursor background since we just redrew everything
    cursor_bg_saved = false;
    
    serial_debug("[COMPOSITOR_RENDER] Rendering cursor\n");
    
    // Render cursor on top of everything to backing store
    if (cursor_enabled) {
        extern mouse_state_t mouse_state;
        // Save background and draw cursor
        compositor_save_cursor_background(mouse_state.x, mouse_state.y);
        compositor_render_cursor(mouse_state.x, mouse_state.y);
        last_cursor_x = mouse_state.x;
        last_cursor_y = mouse_state.y;
    }
    
    serial_debug("[COMPOSITOR_RENDER] Swapping buffers\n");
    
    // DOUBLE BUFFERING: Swap back buffer to visible framebuffer in one operation
    extern void framebuffer_swap(void);
    framebuffer_swap();
    
    serial_debug("[COMPOSITOR_RENDER] Complete\n");
}
