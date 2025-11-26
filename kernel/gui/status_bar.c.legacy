#include "status_bar.h"
#include "renderer.h"
#include "core/memory.h"
#include "core/string.h"
#include "config.h"
#include "keyboard/keyboard_types.h"

#define STATUS_BAR_BG_COLOR    0xFF2D2D30
#define STATUS_BAR_BORDER_COLOR 0xFF3E3E42
#define STATUS_BAR_ITEM_SPACING 4

// Forward declarations
static void layout_items(StatusBar* bar);
static void render_button_item(StatusBar* bar, StatusBarItem* item, int index);
static void render_label_item(StatusBar* bar, StatusBarItem* item);
static void render_icon_item(StatusBar* bar, StatusBarItem* item);

StatusBar* status_bar_create(int x, int y, int width, int height) {
    StatusBar* bar = (StatusBar*)malloc(sizeof(StatusBar));
    if (!bar) {
        SERIAL_LOG("[STATUS_BAR] Failed to allocate StatusBar\n");
        return NULL;
    }
    
    memset(bar, 0, sizeof(StatusBar));
    
    bar->x = x;
    bar->y = y;
    bar->width = width;
    bar->height = height;
    bar->item_count = 0;
    bar->focused_item = -1;
    bar->bg_color = STATUS_BAR_BG_COLOR;
    bar->border_color = STATUS_BAR_BORDER_COLOR;
    
    // Allocate pixel buffer
    extern void* heap_alloc(size_t size);
    size_t buffer_size = width * height * sizeof(uint32_t);
    bar->pixel_buffer = heap_alloc(buffer_size);
    if (!bar->pixel_buffer) {
        SERIAL_LOG("[STATUS_BAR] Failed to allocate pixel buffer\n");
        free(bar);
        return NULL;
    }
    
    SERIAL_LOG("[STATUS_BAR] Created status bar\n");
    return bar;
}

void status_bar_destroy(StatusBar* bar) {
    if (!bar) return;
    // Note: heap_alloc doesn't have a free function yet, so we just free the struct
    free(bar);
}

int status_bar_add_button(StatusBar* bar, const char* text, StatusAlignment align,
                          void (*on_click)(void* user_data), void* user_data) {
    if (!bar || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    StatusBarItem* item = &bar->items[index];
    
    item->type = STATUS_ITEM_BUTTON;
    item->alignment = align;
    item->visible = true;
    item->width = strlen(text) * 8 + 20;  // Estimate width
    
    // Initialize button (position will be set during layout)
    button_init(&item->data.button, 0, 0, item->width, bar->height - 4, text);
    item->data.button.on_click = on_click;
    item->data.button.user_data = user_data;
    
    layout_items(bar);
    return index;
}

int status_bar_add_label(StatusBar* bar, const char* text, StatusAlignment align) {
    if (!bar || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    StatusBarItem* item = &bar->items[index];
    
    item->type = STATUS_ITEM_LABEL;
    item->alignment = align;
    item->visible = true;
    item->width = strlen(text) * 8 + 10;
    
    label_init(&item->data.label, 0, 0, text, TEXT_COLOR);
    
    layout_items(bar);
    return index;
}

int status_bar_add_icon(StatusBar* bar, uint32_t* icon_data, int icon_size, StatusAlignment align,
                        void (*on_click)(void* user_data), void* user_data) {
    if (!bar || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    StatusBarItem* item = &bar->items[index];
    
    item->type = STATUS_ITEM_ICON;
    item->alignment = align;
    item->visible = true;
    item->width = icon_size + 8;
    
    item->data.icon.icon_data = icon_data;
    item->data.icon.icon_size = icon_size;
    item->data.icon.on_click = on_click;
    item->data.icon.user_data = user_data;
    
    layout_items(bar);
    return index;
}

int status_bar_add_spacer(StatusBar* bar, int width, StatusAlignment align) {
    if (!bar || bar->item_count >= STATUS_BAR_MAX_ITEMS) return -1;
    
    int index = bar->item_count++;
    StatusBarItem* item = &bar->items[index];
    
    item->type = STATUS_ITEM_SPACER;
    item->alignment = align;
    item->visible = true;
    item->width = width;
    
    layout_items(bar);
    return index;
}

void status_bar_remove_item(StatusBar* bar, int item_index) {
    if (!bar || item_index < 0 || item_index >= bar->item_count) return;
    
    // Shift items down
    for (int i = item_index; i < bar->item_count - 1; i++) {
        bar->items[i] = bar->items[i + 1];
    }
    bar->item_count--;
    
    layout_items(bar);
}

void status_bar_update_label_text(StatusBar* bar, int item_index, const char* new_text) {
    if (!bar || item_index < 0 || item_index >= bar->item_count) return;
    if (bar->items[item_index].type != STATUS_ITEM_LABEL) return;
    
    label_set_text(&bar->items[item_index].data.label, new_text);
    bar->items[item_index].width = strlen(new_text) * 8 + 10;
    layout_items(bar);
}

void status_bar_update_button_text(StatusBar* bar, int item_index, const char* new_text) {
    if (!bar || item_index < 0 || item_index >= bar->item_count) return;
    if (bar->items[item_index].type != STATUS_ITEM_BUTTON) return;
    
    button_set_label(&bar->items[item_index].data.button, new_text);
    bar->items[item_index].width = strlen(new_text) * 8 + 20;
    layout_items(bar);
}

static void layout_items(StatusBar* bar) {
    if (!bar) return;
    
    int left_x = STATUS_BAR_ITEM_SPACING;
    int center_x = bar->width / 2;
    int right_x = bar->width - STATUS_BAR_ITEM_SPACING;
    
    // Calculate center items width for proper centering
    int center_width = 0;
    for (int i = 0; i < bar->item_count; i++) {
        if (bar->items[i].visible && bar->items[i].alignment == STATUS_ALIGN_CENTER) {
            center_width += bar->items[i].width + STATUS_BAR_ITEM_SPACING;
        }
    }
    center_x -= center_width / 2;
    
    // Layout items
    for (int i = 0; i < bar->item_count; i++) {
        StatusBarItem* item = &bar->items[i];
        if (!item->visible) continue;
        
        int y_offset = (bar->height - 24) / 2;  // Center vertically
        
        switch (item->alignment) {
            case STATUS_ALIGN_LEFT:
                item->x_offset = left_x;
                if (item->type == STATUS_ITEM_BUTTON) {
                    item->data.button.base.x = left_x;
                    item->data.button.base.y = y_offset;
                } else if (item->type == STATUS_ITEM_LABEL) {
                    item->data.label.base.x = left_x;
                    item->data.label.base.y = y_offset + 8;
                }
                left_x += item->width + STATUS_BAR_ITEM_SPACING;
                break;
                
            case STATUS_ALIGN_CENTER:
                item->x_offset = center_x;
                if (item->type == STATUS_ITEM_BUTTON) {
                    item->data.button.base.x = center_x;
                    item->data.button.base.y = y_offset;
                } else if (item->type == STATUS_ITEM_LABEL) {
                    item->data.label.base.x = center_x;
                    item->data.label.base.y = y_offset + 8;
                }
                center_x += item->width + STATUS_BAR_ITEM_SPACING;
                break;
                
            case STATUS_ALIGN_RIGHT:
                right_x -= item->width;
                item->x_offset = right_x;
                if (item->type == STATUS_ITEM_BUTTON) {
                    item->data.button.base.x = right_x;
                    item->data.button.base.y = y_offset;
                } else if (item->type == STATUS_ITEM_LABEL) {
                    item->data.label.base.x = right_x;
                    item->data.label.base.y = y_offset + 8;
                }
                right_x -= STATUS_BAR_ITEM_SPACING;
                break;
        }
    }
}

void status_bar_render(StatusBar* bar) {
    if (!bar || !bar->pixel_buffer) return;
    
    // Clear background
    draw_filled_rect(bar->pixel_buffer, bar->width, 0, 0, bar->width, bar->height, bar->bg_color);
    
    // Draw top border
    draw_filled_rect(bar->pixel_buffer, bar->width, 0, 0, bar->width, 1, bar->border_color);
    
    // Render items
    for (int i = 0; i < bar->item_count; i++) {
        StatusBarItem* item = &bar->items[i];
        if (!item->visible) continue;
        
        switch (item->type) {
            case STATUS_ITEM_BUTTON:
                render_button_item(bar, item, i);
                break;
            case STATUS_ITEM_LABEL:
                render_label_item(bar, item);
                break;
            case STATUS_ITEM_ICON:
                render_icon_item(bar, item);
                break;
            case STATUS_ITEM_SPACER:
                // Spacers don't render
                break;
        }
    }
}

static void render_button_item(StatusBar* bar, StatusBarItem* item, int index) {
    // Update focus state
    item->data.button.has_focus = (bar->focused_item == index);
    button_render(&item->data.button, bar->pixel_buffer, bar->width, bar->height);
}

static void render_label_item(StatusBar* bar, StatusBarItem* item) {
    label_render(&item->data.label, bar->pixel_buffer, bar->width, bar->height);
}

static void render_icon_item(StatusBar* bar, StatusBarItem* item) {
    if (!item->data.icon.icon_data) return;
    
    int x = item->x_offset;
    int y = (bar->height - item->data.icon.icon_size) / 2;
    int size = item->data.icon.icon_size;
    
    // Draw icon
    for (int py = 0; py < size; py++) {
        for (int px = 0; px < size; px++) {
            int buf_x = x + px;
            int buf_y = y + py;
            if (buf_x >= 0 && buf_x < bar->width && buf_y >= 0 && buf_y < bar->height) {
                uint32_t pixel = item->data.icon.icon_data[py * size + px];
                if ((pixel >> 24) > 0) {  // Check alpha
                    bar->pixel_buffer[buf_y * bar->width + buf_x] = pixel;
                }
            }
        }
    }
}

void status_bar_handle_click(StatusBar* bar, int mouse_x, int mouse_y) {
    if (!bar) return;
    
    for (int i = 0; i < bar->item_count; i++) {
        StatusBarItem* item = &bar->items[i];
        if (!item->visible) continue;
        
        if (item->type == STATUS_ITEM_BUTTON) {
            button_handle_click(&item->data.button, mouse_x, mouse_y);
        } else if (item->type == STATUS_ITEM_ICON) {
            int x = item->x_offset;
            int y = (bar->height - item->data.icon.icon_size) / 2;
            int size = item->data.icon.icon_size;
            
            if (mouse_x >= x && mouse_x < x + size && 
                mouse_y >= y && mouse_y < y + size) {
                if (item->data.icon.on_click) {
                    item->data.icon.on_click(item->data.icon.user_data);
                }
            }
        }
    }
}

void status_bar_handle_key(StatusBar* bar, uint32_t keycode) {
    if (!bar) return;
    
    // Tab navigation
    if (keycode == KEY_TAB) {
        // Find next focusable item
        int start = bar->focused_item;
        do {
            bar->focused_item++;
            if (bar->focused_item >= bar->item_count) {
                bar->focused_item = -1;  // Un-focus
                return;
            }
            
            StatusBarItem* item = &bar->items[bar->focused_item];
            if (item->visible && item->type == STATUS_ITEM_BUTTON) {
                return;  // Found focusable item
            }
        } while (bar->focused_item != start);
    }
    
    // Left/Right navigation
    if (keycode == KEY_LEFT || keycode == KEY_RIGHT) {
        if (bar->focused_item < 0) return;
        
        int direction = (keycode == KEY_RIGHT) ? 1 : -1;
        int next = bar->focused_item;
        
        do {
            next += direction;
            if (next < 0) next = bar->item_count - 1;
            if (next >= bar->item_count) next = 0;
            
            StatusBarItem* item = &bar->items[next];
            if (item->visible && item->type == STATUS_ITEM_BUTTON) {
                bar->focused_item = next;
                return;
            }
        } while (next != bar->focused_item);
    }
    
    // Enter to activate focused item
    if (keycode == KEY_ENTER && bar->focused_item >= 0) {
        StatusBarItem* item = &bar->items[bar->focused_item];
        if (item->type == STATUS_ITEM_BUTTON) {
            button_activate(&item->data.button);
        }
    }
}

int status_bar_get_item_at(StatusBar* bar, int mouse_x, int mouse_y) {
    if (!bar) return -1;
    
    for (int i = 0; i < bar->item_count; i++) {
        StatusBarItem* item = &bar->items[i];
        if (!item->visible) continue;
        
        int x = item->x_offset;
        int width = item->width;
        
        if (mouse_x >= x && mouse_x < x + width &&
            mouse_y >= 0 && mouse_y < bar->height) {
            return i;
        }
    }
    
    return -1;
}
