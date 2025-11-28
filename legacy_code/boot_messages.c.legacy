#include "boot_messages.h"
#include "renderer.h"
#include "core/string.h"
#include "core/memory.h"
#include "config.h"
#include "keyboard/keyboard_types.h"

// Colors
#define WINDOW_BG_COLOR     0xFF1E1E1E
#define TITLE_BG_COLOR      0xFF2D2D30
#define TEXT_COLOR          0xFFCCCCCC
#define TITLE_TEXT_COLOR    0xFFFFFFFF
#define BORDER_COLOR        0xFF3E3E42

// Layout
#define TITLE_BAR_HEIGHT 30
#define TEXT_PADDING 10
#define LINE_HEIGHT 14

// Forward declarations for event handlers
static void on_mouse_click(QARMA_INPUT_EVENT* event, void* user_data);
static void on_key_down(QARMA_INPUT_EVENT* event, void* user_data);
static void on_mouse_move(QARMA_INPUT_EVENT* event, void* user_data);
static void on_close_button_click(void* user_data);

BootMessagesWindow* boot_messages_create(int x, int y, int width, int height) {
    BootMessagesWindow* bmw = (BootMessagesWindow*)malloc(sizeof(BootMessagesWindow));
    if (!bmw) {
        SERIAL_LOG("[BOOT_MESSAGES] Failed to allocate BootMessagesWindow\n");
        return NULL;
    }
    
    memset(bmw, 0, sizeof(BootMessagesWindow));
    
    // Create main window
    bmw->main_window = qarma_win_create(QARMA_WIN_TYPE_MODAL, "Boot Messages", QARMA_FLAG_VISIBLE);
    if (!bmw->main_window) {
        SERIAL_LOG("[BOOT_MESSAGES] Failed to create window\n");
        free(bmw);
        return NULL;
    }
    
    // Set window position and size
    bmw->main_window->x = x;
    bmw->main_window->y = y;
    bmw->main_window->size.width = width;
    bmw->main_window->size.height = height;
    
    // Reallocate pixel buffer if size changed from default
    extern void* heap_alloc(size_t size);
    size_t buffer_size = width * height * sizeof(uint32_t);
    bmw->main_window->pixel_buffer = heap_alloc(buffer_size);
    if (!bmw->main_window->pixel_buffer) {
        SERIAL_LOG("[BOOT_MESSAGES] Failed to allocate pixel buffer\n");
        free(bmw);
        return NULL;
    }
    
    // Initialize close button (top-right corner of title bar)
    int close_btn_x = width - 35;
    int close_btn_y = 5;
    close_button_init(&bmw->close_button_ctrl, close_btn_x, close_btn_y, 20);
    bmw->close_button_ctrl.on_click = on_close_button_click;
    bmw->close_button_ctrl.userdata = bmw;
    
    // Initialize message buffer
    bmw->message_count = 0;
    bmw->scroll_offset = 0;
    
    // Register event listeners
    bmw->mouse_click_listener = qarma_input_event_listen(QARMA_INPUT_EVENT_MOUSE_DOWN, on_mouse_click, bmw, 100);
    bmw->key_down_listener = qarma_input_event_listen(QARMA_INPUT_EVENT_KEY_DOWN, on_key_down, bmw, 100);
    bmw->mouse_move_listener = qarma_input_event_listen(QARMA_INPUT_EVENT_MOUSE_MOVE, on_mouse_move, bmw, 100);
    
    SERIAL_LOG("[BOOT_MESSAGES] Window created\n");
    
    return bmw;
}

void boot_messages_destroy(BootMessagesWindow* bmw) {
    if (!bmw) return;
    
    // Unregister event listeners
    if (bmw->mouse_click_listener) {
        qarma_input_event_unlisten(bmw->mouse_click_listener);
    }
    if (bmw->key_down_listener) {
        qarma_input_event_unlisten(bmw->key_down_listener);
    }
    if (bmw->mouse_move_listener) {
        qarma_input_event_unlisten(bmw->mouse_move_listener);
    }
    
    // Destroy window
    if (bmw->main_window) {
        // Window destruction handled by qarma system
        bmw->main_window = NULL;
    }
    
    free(bmw);
}

void boot_messages_add(BootMessagesWindow* bmw, const char* message) {
    if (!bmw || !message) return;
    
    if (bmw->message_count >= MAX_BOOT_MESSAGES) {
        // Shift messages up (remove oldest)
        for (int i = 0; i < MAX_BOOT_MESSAGES - 1; i++) {
            strncpy(bmw->messages[i], bmw->messages[i + 1], MAX_MESSAGE_LENGTH - 1);
            bmw->messages[i][MAX_MESSAGE_LENGTH - 1] = '\0';
        }
        bmw->message_count = MAX_BOOT_MESSAGES - 1;
    }
    
    // Add new message
    strncpy(bmw->messages[bmw->message_count], message, MAX_MESSAGE_LENGTH - 1);
    bmw->messages[bmw->message_count][MAX_MESSAGE_LENGTH - 1] = '\0';
    bmw->message_count++;
    
    // Auto-scroll to bottom
    int visible_lines = (bmw->main_window->size.height - TITLE_BAR_HEIGHT - TEXT_PADDING * 2) / LINE_HEIGHT;
    if (bmw->message_count > visible_lines) {
        bmw->scroll_offset = bmw->message_count - visible_lines;
    }
}

void boot_messages_clear(BootMessagesWindow* bmw) {
    if (!bmw) return;
    bmw->message_count = 0;
    bmw->scroll_offset = 0;
}

void boot_messages_render(BootMessagesWindow* bmw) {
    if (!bmw || !bmw->main_window || !bmw->main_window->pixel_buffer) return;
    
    uint32_t* buffer = bmw->main_window->pixel_buffer;
    int width = bmw->main_window->size.width;
    int height = bmw->main_window->size.height;
    
    // Clear background
    draw_filled_rect(buffer, width, 0, 0, width, height, WINDOW_BG_COLOR);
    
    // Draw title bar
    draw_filled_rect(buffer, width, 0, 0, width, TITLE_BAR_HEIGHT, TITLE_BG_COLOR);
    draw_string_to_buffer(buffer, width, 10, 8, "Boot Messages", TITLE_TEXT_COLOR);
    
    // Draw close button
    close_button_render(&bmw->close_button_ctrl, buffer, width, height);
    
    // Draw border around content area
    int content_y = TITLE_BAR_HEIGHT;
    int content_height = height - TITLE_BAR_HEIGHT;
    draw_rect_border(buffer, width, 0, content_y, width, content_height, BORDER_COLOR, 1);
    
    // Draw messages
    int y = content_y + TEXT_PADDING;
    int visible_lines = (content_height - TEXT_PADDING * 2) / LINE_HEIGHT;
    int start_idx = bmw->scroll_offset;
    int end_idx = start_idx + visible_lines;
    
    if (end_idx > bmw->message_count) {
        end_idx = bmw->message_count;
    }
    
    for (int i = start_idx; i < end_idx; i++) {
        draw_string_to_buffer(buffer, width, TEXT_PADDING, y, bmw->messages[i], TEXT_COLOR);
        y += LINE_HEIGHT;
    }
    
    // Mark as dirty to trigger blit
    bmw->main_window->dirty = true;
}

void boot_messages_update(BootMessagesWindow* bmw) {
    if (!bmw) return;
    // Currently no animations, but can add cursor blink or auto-scroll later
}

void boot_messages_set_close_callback(BootMessagesWindow* bmw, void (*callback)(void* user_data), void* user_data) {
    if (!bmw) return;
    bmw->on_close = callback;
    bmw->close_user_data = user_data;
}

void boot_messages_handle_event(BootMessagesWindow* bmw, QARMA_INPUT_EVENT* event) {
    if (!bmw || !event) return;
    
    // Handle keyboard scrolling
    if (event->type == QARMA_INPUT_EVENT_KEY_DOWN) {
        int visible_lines = (bmw->main_window->size.height - TITLE_BAR_HEIGHT - TEXT_PADDING * 2) / LINE_HEIGHT;
        
        if (event->data.key.keycode == KEY_UP || event->data.key.keycode == KEY_LEFT) {
            if (bmw->scroll_offset > 0) {
                bmw->scroll_offset--;
                boot_messages_render(bmw);
            }
        } else if (event->data.key.keycode == KEY_DOWN || event->data.key.keycode == KEY_RIGHT) {
            int max_scroll = bmw->message_count - visible_lines;
            if (max_scroll < 0) max_scroll = 0;
            if (bmw->scroll_offset < max_scroll) {
                bmw->scroll_offset++;
                boot_messages_render(bmw);
            }
        }
    }
}

// ============================================================================
// Event Handlers
// ============================================================================

static void on_mouse_click(QARMA_INPUT_EVENT* event, void* user_data) {
    BootMessagesWindow* bmw = (BootMessagesWindow*)user_data;
    if (!bmw || !bmw->main_window) return;
    
    int rel_x = event->data.mouse.x - bmw->main_window->x;
    int rel_y = event->data.mouse.y - bmw->main_window->y;
    
    // Check close button click
    close_button_handle_click(&bmw->close_button_ctrl, rel_x, rel_y);
}

static void on_key_down(QARMA_INPUT_EVENT* event, void* user_data) {
    BootMessagesWindow* bmw = (BootMessagesWindow*)user_data;
    if (!bmw) return;
    
    // Handle scrolling
    boot_messages_handle_event(bmw, event);
    
    // Handle close button keyboard activation (Enter or Space on focused close button)
    if (bmw->close_button_ctrl.focused) {
        if (event->data.key.keycode == KEY_ENTER || event->data.key.keycode == KEY_SPACE) {
            SERIAL_LOG("[BOOT_MESSAGES] Close button activated via keyboard\n");
            close_button_activate(&bmw->close_button_ctrl);
        }
    }
    
    // Tab to focus close button
    if (event->data.key.keycode == KEY_TAB) {
        close_button_set_focus(&bmw->close_button_ctrl, !bmw->close_button_ctrl.focused);
        boot_messages_render(bmw);
    }
}

static void on_mouse_move(QARMA_INPUT_EVENT* event, void* user_data) {
    BootMessagesWindow* bmw = (BootMessagesWindow*)user_data;
    if (!bmw || !bmw->main_window) return;
    
    int rel_x = event->data.mouse.x - bmw->main_window->x;
    int rel_y = event->data.mouse.y - bmw->main_window->y;
    
    // Update close button hover state
    bool was_hovered = bmw->close_button_ctrl.hovered;
    close_button_update(&bmw->close_button_ctrl, rel_x, rel_y, false);
    
    // Re-render if hover state changed
    if (was_hovered != bmw->close_button_ctrl.hovered) {
        boot_messages_render(bmw);
    }
}

static void on_close_button_click(void* user_data) {
    BootMessagesWindow* bmw = (BootMessagesWindow*)user_data;
    if (!bmw) return;
    
    SERIAL_LOG("[BOOT_MESSAGES] Close button clicked\n");
    if (bmw->on_close) {
        bmw->on_close(bmw->close_user_data);
    }
}
