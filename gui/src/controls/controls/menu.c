/**
 * Menu Control System Implementation
 * 
 * Provides menu bar, dropdown menus, and menu items
 */

#include "controls/controls/menu.h"
#include "memory.h"
#include "string.h"
#include "graphics.h"
#include "config.h"

// Menu bar dimensions
#define MENU_BAR_HEIGHT 24
#define MENU_ITEM_HEIGHT 20
#define MENU_PADDING 8
#define MENU_MIN_WIDTH 150

// Colors
#define MENU_BG_COLOR 0xF0F0F0
#define MENU_TEXT_COLOR 0x000000
#define MENU_HIGHLIGHT_COLOR 0x0078D7
#define MENU_HOVER_COLOR 0xE5F1FB
#define MENU_SEPARATOR_COLOR 0xC0C0C0
#define MENU_DISABLED_COLOR 0x808080

/**
 * Create a menu bar
 */
MenuBar* menu_bar_create(QARMA_WIN_HANDLE* window) {
    MenuBar* bar = (MenuBar*)malloc(sizeof(MenuBar));
    if (!bar) return NULL;
    
    bar->window = window;
    bar->menu_count = 0;
    bar->active_menu = -1;
    bar->height = MENU_BAR_HEIGHT;
    bar->bg_color = MENU_BG_COLOR;
    bar->text_color = MENU_TEXT_COLOR;
    bar->highlight_color = MENU_HIGHLIGHT_COLOR;
    bar->hover_color = MENU_HOVER_COLOR;
    
    SERIAL_LOG("[Menu] Menu bar created\n");
    return bar;
}

/**
 * Destroy menu bar
 */
void menu_bar_destroy(MenuBar* bar) {
    if (!bar) return;
    free(bar);
}

/**
 * Add a menu to the menu bar
 */
Menu* menu_bar_add_menu(MenuBar* bar, const char* title) {
    if (!bar || bar->menu_count >= MAX_MENUS) return NULL;
    
    Menu* menu = &bar->menus[bar->menu_count];
    strncpy(menu->title, title, MAX_MENU_LABEL_LENGTH - 1);
    menu->title[MAX_MENU_LABEL_LENGTH - 1] = '\0';
    menu->item_count = 0;
    menu->selected_index = -1;
    menu->is_open = false;
    
    bar->menu_count++;
    SERIAL_LOG("[Menu] Added menu: ");
    SERIAL_LOG(title);
    SERIAL_LOG("\n");
    
    return menu;
}

/**
 * Calculate menu position on screen
 */
static void menu_calculate_position(Menu* menu, int menu_index, int bar_height) {
    menu->x = 10 + (menu_index * 80);  // Spacing between menus
    menu->y = bar_height;
    menu->width = MENU_MIN_WIDTH;
    menu->height = menu->item_count * MENU_ITEM_HEIGHT + 4;
}

/**
 * Render the menu bar
 */
void menu_bar_render(MenuBar* bar) {
    if (!bar || !bar->window || !bar->window->pixel_buffer) return;
    
    uint32_t* fb = bar->window->pixel_buffer;
    int fb_width = bar->window->size.width;
    
    // Draw menu bar background
    for (int y = 0; y < bar->height; y++) {
        for (int x = 0; x < fb_width; x++) {
            fb[y * fb_width + x] = bar->bg_color;
        }
    }
    
    // Draw menu titles
    int x_offset = 10;
    for (int i = 0; i < bar->menu_count; i++) {
        Menu* menu = &bar->menus[i];
        
        // Highlight if active
        if (i == bar->active_menu) {
            uint32_t highlight = bar->hover_color;
            int title_width = strlen(menu->title) * 8 + 16;
            for (int y = 2; y < bar->height - 2; y++) {
                for (int x = x_offset - 4; x < x_offset + title_width - 4; x++) {
                    if (x >= 0 && x < fb_width) {
                        fb[y * fb_width + x] = highlight;
                    }
                }
            }
        }
        
        // Draw title text (simple character rendering)
        int text_y = (bar->height - 8) / 2;
        for (int ci = 0; menu->title[ci] != '\0' && ci < MAX_MENU_LABEL_LENGTH; ci++) {
            // Very simple character rendering (would need font system)
            int char_x = x_offset + (ci * 8);
            if (char_x + 8 < fb_width) {
                // Draw character placeholder
                for (int cy = 0; cy < 8; cy++) {
                    for (int cx = 0; cx < 8; cx++) {
                        int px = char_x + cx;
                        int py = text_y + cy;
                        if (px < fb_width && py < bar->height) {
                            fb[py * fb_width + px] = bar->text_color;
                        }
                    }
                }
            }
        }
        
        x_offset += strlen(menu->title) * 8 + 24;
        
        // Render dropdown if open
        if (menu->is_open) {
            menu_calculate_position(menu, i, bar->height);
            menu_render(menu, fb, fb_width, bar->window->size.height);
        }
    }
}

/**
 * Render a dropdown menu
 */
void menu_render(Menu* menu, uint32_t* framebuffer, int fb_width, int fb_height) {
    if (!menu || !framebuffer) return;
    
    // Draw menu background
    for (int y = menu->y; y < menu->y + menu->height && y < fb_height; y++) {
        for (int x = menu->x; x < menu->x + menu->width && x < fb_width; x++) {
            framebuffer[y * fb_width + x] = MENU_BG_COLOR;
        }
    }
    
    // Draw border
    for (int x = menu->x; x < menu->x + menu->width && x < fb_width; x++) {
        if (menu->y < fb_height) framebuffer[menu->y * fb_width + x] = 0x808080;
        int bottom = menu->y + menu->height - 1;
        if (bottom < fb_height) framebuffer[bottom * fb_width + x] = 0x808080;
    }
    for (int y = menu->y; y < menu->y + menu->height && y < fb_height; y++) {
        if (menu->x < fb_width) framebuffer[y * fb_width + menu->x] = 0x808080;
        int right = menu->x + menu->width - 1;
        if (right < fb_width) framebuffer[y * fb_width + right] = 0x808080;
    }
    
    // Draw menu items
    int item_y = menu->y + 2;
    for (int i = 0; i < menu->item_count; i++) {
        MenuItem* item = &menu->items[i];
        
        if (item->type == MENU_ITEM_SEPARATOR) {
            // Draw separator line
            int sep_y = item_y + MENU_ITEM_HEIGHT / 2;
            if (sep_y < fb_height) {
                for (int x = menu->x + 4; x < menu->x + menu->width - 4 && x < fb_width; x++) {
                    framebuffer[sep_y * fb_width + x] = MENU_SEPARATOR_COLOR;
                }
            }
        } else {
            // Highlight if selected
            if (item->highlighted || i == menu->selected_index) {
                for (int y = item_y; y < item_y + MENU_ITEM_HEIGHT && y < fb_height; y++) {
                    for (int x = menu->x + 2; x < menu->x + menu->width - 2 && x < fb_width; x++) {
                        framebuffer[y * fb_width + x] = MENU_HOVER_COLOR;
                    }
                }
            }
            
            // Draw item text (placeholder)
            uint32_t text_color = (item->flags & MENU_FLAG_DISABLED) ? MENU_DISABLED_COLOR : MENU_TEXT_COLOR;
            int text_x = menu->x + 8;
            int text_y = item_y + 6;
            
            // Simple text rendering
            for (int ci = 0; item->label[ci] != '\0' && ci < MAX_MENU_LABEL_LENGTH; ci++) {
                int char_x = text_x + (ci * 8);
                if (char_x + 8 < fb_width && text_y + 8 < fb_height) {
                    for (int cy = 0; cy < 8; cy++) {
                        for (int cx = 0; cx < 8; cx++) {
                            int px = char_x + cx;
                            int py = text_y + cy;
                            if (px < fb_width && py < fb_height) {
                                framebuffer[py * fb_width + px] = text_color;
                            }
                        }
                    }
                }
            }
            
            // Draw checkbox/checkmark
            if (item->type == MENU_ITEM_CHECKBOX && (item->flags & MENU_FLAG_CHECKED)) {
                int check_x = menu->x + menu->width - 20;
                int check_y = item_y + 6;
                for (int i = 0; i < 8; i++) {
                    if (check_x + i < fb_width && check_y + i < fb_height) {
                        framebuffer[(check_y + i) * fb_width + (check_x + i)] = text_color;
                    }
                }
            }
        }
        
        item_y += MENU_ITEM_HEIGHT;
    }
}

/**
 * Handle mouse input on menu bar
 */
void menu_bar_handle_mouse(MenuBar* bar, int x, int y, bool clicked) {
    if (!bar) return;
    
    // Check if clicking on menu bar
    if (y < bar->height) {
        int x_offset = 10;
        for (int i = 0; i < bar->menu_count; i++) {
            Menu* menu = &bar->menus[i];
            int title_width = strlen(menu->title) * 8 + 24;
            
            if (x >= x_offset - 4 && x < x_offset + title_width - 4) {
                if (clicked) {
                    // Toggle menu
                    if (bar->active_menu == i) {
                        menu_bar_close_all(bar);
                    } else {
                        menu_bar_close_all(bar);
                        bar->active_menu = i;
                        menu->is_open = true;
                        SERIAL_LOG("[Menu] Opened menu: ");
                        SERIAL_LOG(menu->title);
                        SERIAL_LOG("\n");
                    }
                }
                return;
            }
            x_offset += title_width;
        }
        
        // Clicked outside menus - close all
        if (clicked) {
            menu_bar_close_all(bar);
        }
        return;
    }
    
    // Check if clicking in open menu
    if (bar->active_menu >= 0) {
        Menu* menu = &bar->menus[bar->active_menu];
        if (menu->is_open) {
            int result = menu_handle_mouse(menu, x, y, clicked);
            if (result >= 0) {
                // Item was clicked
                menu_bar_close_all(bar);
            }
        }
    }
}

/**
 * Handle mouse input on dropdown menu
 */
int menu_handle_mouse(Menu* menu, int mouse_x, int mouse_y, bool clicked) {
    if (!menu || !menu->is_open) return -1;
    
    // Check if mouse is in menu bounds
    if (mouse_x < menu->x || mouse_x >= menu->x + menu->width ||
        mouse_y < menu->y || mouse_y >= menu->y + menu->height) {
        // Mouse outside menu
        for (int i = 0; i < menu->item_count; i++) {
            menu->items[i].highlighted = false;
        }
        return -1;
    }
    
    // Find which item is under mouse
    int relative_y = mouse_y - menu->y - 2;
    int item_index = relative_y / MENU_ITEM_HEIGHT;
    
    if (item_index >= 0 && item_index < menu->item_count) {
        MenuItem* item = &menu->items[item_index];
        
        // Update highlighting
        for (int i = 0; i < menu->item_count; i++) {
            menu->items[i].highlighted = (i == item_index);
        }
        
        // Handle click
        if (clicked && item->type != MENU_ITEM_SEPARATOR && !(item->flags & MENU_FLAG_DISABLED)) {
            SERIAL_LOG("[Menu] Item clicked: ");
            SERIAL_LOG(item->label);
            SERIAL_LOG("\n");
            
            // Toggle checkbox
            if (item->type == MENU_ITEM_CHECKBOX) {
                item->flags ^= MENU_FLAG_CHECKED;
            }
            
            // Call callback
            if (item->callback) {
                item->callback(item->user_data);
            }
            
            return item_index;
        }
    }
    
    return -1;
}

/**
 * Close all open menus
 */
void menu_bar_close_all(MenuBar* bar) {
    if (!bar) return;
    
    for (int i = 0; i < bar->menu_count; i++) {
        bar->menus[i].is_open = false;
        bar->menus[i].selected_index = -1;
    }
    bar->active_menu = -1;
    SERIAL_LOG("[Menu] Closed all menus\n");
}

/**
 * Handle keyboard shortcuts
 */
void menu_bar_handle_key(MenuBar* bar, uint8_t scancode, uint8_t modifiers) {
    if (!bar) return;
    
    // Check all menu items for matching shortcuts
    for (int mi = 0; mi < bar->menu_count; mi++) {
        Menu* menu = &bar->menus[mi];
        for (int ii = 0; ii < menu->item_count; ii++) {
            MenuItem* item = &menu->items[ii];
            
            if (item->shortcut.key == scancode && item->shortcut.modifiers == modifiers) {
                if (!(item->flags & MENU_FLAG_DISABLED) && item->callback) {
                    SERIAL_LOG("[Menu] Shortcut activated: ");
                    SERIAL_LOG(item->label);
                    SERIAL_LOG("\n");
                    item->callback(item->user_data);
                    return;
                }
            }
        }
    }
}

/**
 * Add a regular menu item
 */
MenuItem* menu_add_item(Menu* menu, const char* label, MenuItemCallback callback, void* user_data) {
    if (!menu || menu->item_count >= MAX_MENU_ITEMS) return NULL;
    
    MenuItem* item = &menu->items[menu->item_count];
    item->type = MENU_ITEM_NORMAL;
    strncpy(item->label, label, MAX_MENU_LABEL_LENGTH - 1);
    item->label[MAX_MENU_LABEL_LENGTH - 1] = '\0';
    item->flags = MENU_FLAG_NONE;
    item->shortcut.modifiers = 0;
    item->shortcut.key = 0;
    item->callback = callback;
    item->user_data = user_data;
    item->submenu = NULL;
    item->highlighted = false;
    item->id = menu->item_count;
    
    menu->item_count++;
    return item;
}

/**
 * Add a separator
 */
MenuItem* menu_add_separator(Menu* menu) {
    if (!menu || menu->item_count >= MAX_MENU_ITEMS) return NULL;
    
    MenuItem* item = &menu->items[menu->item_count];
    item->type = MENU_ITEM_SEPARATOR;
    item->label[0] = '\0';
    item->flags = MENU_FLAG_NONE;
    item->callback = NULL;
    item->highlighted = false;
    
    menu->item_count++;
    return item;
}

/**
 * Add a checkbox item
 */
MenuItem* menu_add_checkbox(Menu* menu, const char* label, bool checked, MenuItemCallback callback, void* user_data) {
    MenuItem* item = menu_add_item(menu, label, callback, user_data);
    if (!item) return NULL;
    
    item->type = MENU_ITEM_CHECKBOX;
    if (checked) {
        item->flags |= MENU_FLAG_CHECKED;
    }
    
    return item;
}

/**
 * Add a submenu
 */
MenuItem* menu_add_submenu(Menu* menu, const char* label, Menu* submenu) {
    MenuItem* item = menu_add_item(menu, label, NULL, NULL);
    if (!item) return NULL;
    
    item->type = MENU_ITEM_SUBMENU;
    item->submenu = submenu;
    
    return item;
}

/**
 * Set keyboard shortcut for menu item
 */
void menu_set_shortcut(MenuItem* item, uint8_t modifiers, uint8_t key) {
    if (!item) return;
    item->shortcut.modifiers = modifiers;
    item->shortcut.key = key;
}

/**
 * Enable/disable menu items
 */
void menu_item_enable(MenuItem* item) {
    if (item) item->flags &= ~MENU_FLAG_DISABLED;
}

void menu_item_disable(MenuItem* item) {
    if (item) item->flags |= MENU_FLAG_DISABLED;
}

/**
 * Check/uncheck menu item
 */
void menu_item_check(MenuItem* item, bool checked) {
    if (!item) return;
    if (checked) {
        item->flags |= MENU_FLAG_CHECKED;
    } else {
        item->flags &= ~MENU_FLAG_CHECKED;
    }
}
