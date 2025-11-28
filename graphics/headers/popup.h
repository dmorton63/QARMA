#pragma once
#include "core/stdtools.h"
#include "framebuffer.h"
#include "font_data.h"
#include "core/sleep.h"

typedef struct {
    int x, y, width, height;
    uint32_t *pixels;  // Raw pixel buffer
} SavedRegion;


typedef struct popup_params{
    int x, y;                        // Position on screen
    const char *message;            // Message to display
    const char *title;              // Optional title for a title bar
    rgb_color_t bg_color;           // Background color
    rgb_color_t border_color;       // Border color
    rgb_color_t text_color;            // Text color
    rgb_color_t title_color;        // Title text color
    int title_height;               // Height of the title bar in pixels
    int timeout_ms;                 // Auto-dismiss time (0 = wait for key)

    bool dismiss_on_esc;            // Allow Esc to cancel
    bool confirm_on_enter;          // Allow Enter to confirm
    void (*on_confirm)(void);       // Callback for Enter
    void (*on_cancel)(void);        // Callback for Esc
} PopupParams;

void show_popup(const PopupParams *params);

SavedRegion *save_region(int x, int y, int width, int height);

void restore_region(SavedRegion *region);

void confirm_action(void);

void cancel_action(void);
