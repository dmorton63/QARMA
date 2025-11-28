/**
 * Menu Control System
 * 
 * Provides menu bar, dropdown menus, and menu items
 */

#ifndef GUI_MENU_H
#define GUI_MENU_H

#include "kernel_types.h"
#include "qarma_win_handle/qarma_win_handle.h"

#define MAX_MENU_ITEMS 32
#define MAX_MENUS 16
#define MAX_MENU_LABEL_LENGTH 32

// Menu item types
typedef enum {
    MENU_ITEM_NORMAL,
    MENU_ITEM_SEPARATOR,
    MENU_ITEM_CHECKBOX,
    MENU_ITEM_RADIO,
    MENU_ITEM_SUBMENU
} MenuItemType;

// Menu item flags
typedef enum {
    MENU_FLAG_NONE = 0,
    MENU_FLAG_DISABLED = (1 << 0),
    MENU_FLAG_CHECKED = (1 << 1),
    MENU_FLAG_HIDDEN = (1 << 2)
} MenuItemFlags;

// Keyboard shortcuts
typedef struct {
    uint8_t modifiers;  // Bit flags: Ctrl=1, Alt=2, Shift=4
    uint8_t key;        // Scancode
} MenuShortcut;

// Forward declarations
typedef struct MenuItem MenuItem;
typedef struct Menu Menu;
typedef struct MenuBar MenuBar;

// Menu item callback
typedef void (*MenuItemCallback)(void* user_data);

// Menu item
struct MenuItem {
    MenuItemType type;
    char label[MAX_MENU_LABEL_LENGTH];
    uint32_t flags;
    MenuShortcut shortcut;
    MenuItemCallback callback;
    void* user_data;
    Menu* submenu;  // For MENU_ITEM_SUBMENU
    bool highlighted;
    int id;
};

// Dropdown menu
struct Menu {
    char title[MAX_MENU_LABEL_LENGTH];
    MenuItem items[MAX_MENU_ITEMS];
    int item_count;
    int selected_index;
    bool is_open;
    int x, y;  // Position when open
    int width, height;
};

// Menu bar
struct MenuBar {
    QARMA_WIN_HANDLE* window;
    Menu menus[MAX_MENUS];
    int menu_count;
    int active_menu;  // Index of currently open menu (-1 if none)
    int height;
    uint32_t bg_color;
    uint32_t text_color;
    uint32_t highlight_color;
    uint32_t hover_color;
};

// MenuBar functions
MenuBar* menu_bar_create(QARMA_WIN_HANDLE* window);
void menu_bar_destroy(MenuBar* bar);
Menu* menu_bar_add_menu(MenuBar* bar, const char* title);
void menu_bar_render(MenuBar* bar);
void menu_bar_handle_mouse(MenuBar* bar, int x, int y, bool clicked);
void menu_bar_handle_key(MenuBar* bar, uint8_t scancode, uint8_t modifiers);
void menu_bar_close_all(MenuBar* bar);

// Menu functions
MenuItem* menu_add_item(Menu* menu, const char* label, MenuItemCallback callback, void* user_data);
MenuItem* menu_add_separator(Menu* menu);
MenuItem* menu_add_checkbox(Menu* menu, const char* label, bool checked, MenuItemCallback callback, void* user_data);
MenuItem* menu_add_submenu(Menu* menu, const char* label, Menu* submenu);
void menu_set_shortcut(MenuItem* item, uint8_t modifiers, uint8_t key);
void menu_render(Menu* menu, uint32_t* framebuffer, int fb_width, int fb_height);
int menu_handle_mouse(Menu* menu, int mouse_x, int mouse_y, bool clicked);
void menu_item_enable(MenuItem* item);
void menu_item_disable(MenuItem* item);
void menu_item_check(MenuItem* item, bool checked);

// Shortcut key constants
#define MENU_MOD_CTRL  (1 << 0)
#define MENU_MOD_ALT   (1 << 1)
#define MENU_MOD_SHIFT (1 << 2)

// Common scancodes for shortcuts
#define KEY_N 0x31  // N
#define KEY_O 0x18  // O
#define KEY_S 0x1F  // S
#define KEY_Q 0x10  // Q
#define KEY_Z 0x2C  // Z
#define KEY_Y 0x15  // Y
#define KEY_X 0x2D  // X
#define KEY_C 0x2E  // C
#define KEY_V 0x2F  // V
#define KEY_A 0x1E  // A
#define KEY_F 0x21  // F
#define KEY_R 0x13  // R
#define KEY_B 0x30  // B

#endif // GUI_MENU_H
