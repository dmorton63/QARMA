#pragma once

#include "core/stdtools.h"
#include "controls/button.h"
#include "controls/label.h"

#define STATUS_BAR_HEIGHT 32
#define STATUS_BAR_MAX_ITEMS 16
#define STATUS_BAR_MAX_ICONS 8

// Status bar item types
typedef enum {
    STATUS_ITEM_BUTTON,
    STATUS_ITEM_LABEL,
    STATUS_ITEM_ICON,
    STATUS_ITEM_SPACER
} StatusItemType;

// Status bar alignment
typedef enum {
    STATUS_ALIGN_LEFT,
    STATUS_ALIGN_CENTER,
    STATUS_ALIGN_RIGHT
} StatusAlignment;

// Status bar item
typedef struct {
    StatusItemType type;
    StatusAlignment alignment;
    bool visible;
    int width;
    int x_offset;  // Calculated during layout
    
    union {
        Button button;
        Label label;
        struct {
            uint32_t* icon_data;  // 16x16 icon
            int icon_size;
            void (*on_click)(void* user_data);
            void* user_data;
        } icon;
    } data;
} StatusBarItem;

// Status bar structure
typedef struct {
    int x, y;
    int width, height;
    uint32_t* pixel_buffer;
    
    StatusBarItem items[STATUS_BAR_MAX_ITEMS];
    int item_count;
    
    int focused_item;  // -1 if none focused
    
    uint32_t bg_color;
    uint32_t border_color;
} StatusBar;

// API Functions

// Create status bar
StatusBar* status_bar_create(int x, int y, int width, int height);

// Destroy status bar
void status_bar_destroy(StatusBar* bar);

// Add items to status bar
int status_bar_add_button(StatusBar* bar, const char* text, StatusAlignment align, 
                          void (*on_click)(void* user_data), void* user_data);
int status_bar_add_label(StatusBar* bar, const char* text, StatusAlignment align);
int status_bar_add_icon(StatusBar* bar, uint32_t* icon_data, int icon_size, StatusAlignment align,
                        void (*on_click)(void* user_data), void* user_data);
int status_bar_add_spacer(StatusBar* bar, int width, StatusAlignment align);

// Remove item
void status_bar_remove_item(StatusBar* bar, int item_index);

// Update item
void status_bar_update_label_text(StatusBar* bar, int item_index, const char* new_text);
void status_bar_update_button_text(StatusBar* bar, int item_index, const char* new_text);

// Render status bar
void status_bar_render(StatusBar* bar);

// Handle input
void status_bar_handle_click(StatusBar* bar, int mouse_x, int mouse_y);
void status_bar_handle_key(StatusBar* bar, uint32_t keycode);

// Get item at position
int status_bar_get_item_at(StatusBar* bar, int mouse_x, int mouse_y);
