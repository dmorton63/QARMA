#include "gui/desktop_toolbar.h"
#include "gui/frame.h"
#include "gui/controls/qarma_button.h"
#include "graphics/framebuffer.h"
#include "graphics/graphics.h"
#include "core/memory/heap.h"
#include "config.h"
#include "string.h"

static desktop_toolbar_t* g_toolbar = NULL;

// Button click handlers (forward declarations)
static void toolbar_shutdown_clicked(qarma_control_t* button, void* user_data);
static void toolbar_restart_clicked(qarma_control_t* button, void* user_data);
static void toolbar_cmd_clicked(qarma_control_t* button, void* user_data);

desktop_toolbar_t* desktop_toolbar_create(void) {
    extern FramebufferInfo* fb_info;
    
    if (!fb_info) {
        SERIAL_LOG("[TOOLBAR] Error: No framebuffer info\n");
        return NULL;
    }
    
    g_toolbar = (desktop_toolbar_t*)heap_alloc(sizeof(desktop_toolbar_t));
    if (!g_toolbar) {
        SERIAL_LOG("[TOOLBAR] Error: Failed to allocate toolbar\n");
        return NULL;
    }
    
    memset(g_toolbar, 0, sizeof(desktop_toolbar_t));
    
    // Create toolbar frame at bottom of screen
    uint32_t toolbar_height = 50;
    uint32_t toolbar_y = fb_info->height - toolbar_height;
    
    g_toolbar->frame = frame_create(NULL, 0, toolbar_y, fb_info->width, toolbar_height, 0, "Desktop Toolbar");
    if (!g_toolbar->frame) {
        SERIAL_LOG("[TOOLBAR] Error: Failed to create frame\n");
        heap_free(g_toolbar);
        return NULL;
    }
    
    // Set toolbar background color (light gray)
    g_toolbar->background_color = 0xFFD3D3D3;  // Light gray
    
    // Create three buttons
    uint32_t button_width = 120;
    uint32_t button_height = 30;
    uint32_t button_y = 10;  // 10px from top of toolbar
    uint32_t spacing = 20;
    uint32_t start_x = spacing;
    
    // Shutdown button
    g_toolbar->shutdown_button = button_create(g_toolbar->frame, "ShutdownBtn",
                                               start_x, button_y, button_width, button_height, "Shutdown");
    if (g_toolbar->shutdown_button) {
        button_set_click_handler(g_toolbar->shutdown_button, toolbar_shutdown_clicked);
    }
    
    // Restart button
    g_toolbar->restart_button = button_create(g_toolbar->frame, "RestartBtn",
                                              start_x + button_width + spacing, button_y, button_width, button_height, "Restart");
    if (g_toolbar->restart_button) {
        button_set_click_handler(g_toolbar->restart_button, toolbar_restart_clicked);
    }
    
    // CMD button
    g_toolbar->cmd_button = button_create(g_toolbar->frame, "CMDBtn",
                                          start_x + (button_width + spacing) * 2, button_y, button_width, button_height, "CMD");
    if (g_toolbar->cmd_button) {
        button_set_click_handler(g_toolbar->cmd_button, toolbar_cmd_clicked);
    }
    
    SERIAL_LOG("[TOOLBAR] Desktop toolbar created\n");
    
    return g_toolbar;
}

void desktop_toolbar_render(desktop_toolbar_t* toolbar) {
    if (!toolbar || !toolbar->frame) return;
    
    // Draw toolbar background (raised style)
    // Note: qarma_frame_t doesn't expose bounds directly, use frame accessor functions
    qarma_frame_t* frame = toolbar->frame;
    int32_t rect_x = 0;       // Will get from frame
    int32_t rect_y = 0;
    uint32_t rect_width = 0;
    uint32_t rect_height = 0;
    // TODO: Get frame bounds via accessor function
    // For now, get from framebuffer
    extern FramebufferInfo* fb_info;
    rect_x = 0;
    rect_y = fb_info->height - 50;
    rect_width = fb_info->width;
    rect_height = 50;
    
    // Fill with light gray - use backing_store
    extern void draw_filled_rect(uint32_t* buffer, uint32_t buffer_width, uint32_t buffer_height,
                                  int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
    extern uint32_t* backing_store;
    extern uint32_t fb_width, fb_height;
    
    draw_filled_rect(backing_store, fb_width, fb_height,
                     rect_x, rect_y, rect_width, rect_height, toolbar->background_color);
    
    // Draw raised border (3D effect)
    uint32_t highlight = 0xFFFFFFFF;  // White highlight (top/left)
    uint32_t shadow = 0xFF808080;     // Dark gray shadow (bottom/right)
    
    extern void draw_rect_border(uint32_t* buffer, uint32_t buffer_width, uint32_t buffer_height,
                                  int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
    
    // Top border (highlight)
    draw_rect_border(backing_store, fb_width, fb_height,
                     rect_x, rect_y, rect_width, 2, highlight);
    
    // Left border (highlight)
    draw_rect_border(backing_store, fb_width, fb_height,
                     rect_x, rect_y, 2, rect_height, highlight);
    
    // Bottom border (shadow)
    draw_rect_border(backing_store, fb_width, fb_height,
                     rect_x, rect_y + rect_height - 2, rect_width, 2, shadow);
    
    // Right border (shadow)
    draw_rect_border(backing_store, fb_width, fb_height,
                     rect_x + rect_width - 2, rect_y, 2, rect_height, shadow);
    
    // Render buttons
    frame_render(toolbar->frame);
}

void desktop_toolbar_destroy(desktop_toolbar_t* toolbar) {
    if (!toolbar) return;
    
    if (toolbar->frame) {
        frame_destroy(toolbar->frame);
    }
    
    heap_free(toolbar);
    
    if (toolbar == g_toolbar) {
        g_toolbar = NULL;
    }
}

// Button click handlers

static void toolbar_shutdown_clicked(qarma_control_t* button, void* user_data) {
    (void)button;
    (void)user_data;
    
    SERIAL_LOG("[TOOLBAR] ===== SHUTDOWN BUTTON CLICKED =====\n");
    SERIAL_LOG("[TOOLBAR] System will shutdown in 2 seconds...\n");
    
    // Give user time to see what happened
    extern void task_sleep(uint32_t milliseconds);
    task_sleep(2000);
    
    SERIAL_LOG("[TOOLBAR] Initiating shutdown...\n");
    extern void system_shutdown(void);
    system_shutdown();
}

static void toolbar_restart_clicked(qarma_control_t* button, void* user_data) {
    (void)button;
    (void)user_data;
    
    SERIAL_LOG("[TOOLBAR] ===== RESTART BUTTON CLICKED =====\n");
    SERIAL_LOG("[TOOLBAR] System will restart in 2 seconds...\n");
    
    // Give user time to see what happened
    extern void task_sleep(uint32_t milliseconds);
    task_sleep(2000);
    
    SERIAL_LOG("[TOOLBAR] Initiating restart...\n");
    extern void system_reboot(void);
    system_reboot();
}

static void toolbar_cmd_clicked(qarma_control_t* button, void* user_data) {
    (void)button;
    (void)user_data;
    
    SERIAL_LOG("[TOOLBAR] CMD button clicked - toggling console\n");
    
    // Toggle console visibility
    extern void console_compositor_toggle(void);
    extern void* console_compositor_get_window(void);
    
    // Safety check: only toggle if console exists
    if (console_compositor_get_window()) {
        console_compositor_toggle();
        
        // Trigger full redraw
        extern void compositor_render_all(void);
        compositor_render_all();
    } else {
        SERIAL_LOG("[TOOLBAR] ERROR: Console not initialized\n");
    }
}

desktop_toolbar_t* desktop_toolbar_get_global(void) {
    return g_toolbar;
}
