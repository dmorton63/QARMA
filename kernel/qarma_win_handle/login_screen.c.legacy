#include "login_screen.h"
#include "qarma_window_manager.h"
#include "gui/gui.h"
#include "qarma_win_factory.h"
#include "qarma_input_events.h"
#include "graphics/graphics.h"
#include "graphics/framebuffer.h"
#include "core/kernel.h"
#include "config.h"
#include "core/string.h"

// UI Layout Constants
#define LOGIN_WINDOW_WIDTH  400
#define LOGIN_WINDOW_HEIGHT 300
#define FIELD_WIDTH         280
#define FIELD_HEIGHT        30
#define BUTTON_WIDTH        120
#define BUTTON_HEIGHT       35
#define LABEL_HEIGHT        20
#define SPACING             15

// Colors
#define COLOR_BACKGROUND    0x2C3E50
#define COLOR_TEXT_LABEL    0xECF0F1
#define COLOR_ERROR         0xE74C3C

// Forward declarations
static void on_username_change(void* user_data, const char* text);
static void on_password_change(void* user_data, const char* text);
static void on_username_enter(void* user_data);
static void on_password_enter(void* user_data);
static void on_login_click(void* user_data);
static void on_cancel_click(void* user_data);
static void login_screen_event_handler(QARMA_INPUT_EVENT* event, void* user_data);

// ============================================================================
// Public API
// ============================================================================

LoginScreen* login_screen_create(void) {
    SERIAL_LOG("[LOGIN] login_screen_create() called\n");
    
    // Allocate login screen state (using static for now)
    static LoginScreen login;
    memset(&login, 0, sizeof(LoginScreen));
    
    SERIAL_LOG("[LOGIN] Getting framebuffer dimensions\n");
    // Get screen dimensions from framebuffer
    extern FramebufferInfo* fb_info;
    int screen_width = fb_info ? fb_info->width : 800;
    int screen_height = fb_info ? fb_info->height : 600;
    
    // Calculate centered position
    int x = (screen_width - LOGIN_WINDOW_WIDTH) / 2;
    int y = (screen_height - LOGIN_WINDOW_HEIGHT) / 2;
    
    SERIAL_LOG("[LOGIN] Creating main window\n");
    // Create main window
    login.main_window = qarma_win_create(QARMA_WIN_TYPE_MODAL, "QARMA Login", QARMA_FLAG_VISIBLE);
    
    SERIAL_LOG("[LOGIN] Main window create returned\n");
    if (!login.main_window) {
        SERIAL_LOG("[LOGIN] Failed to create window\n");
        return NULL;
    }
    
    // Set window position and size
    login.main_window->x = x;
    login.main_window->y = y;
    login.main_window->size.width = LOGIN_WINDOW_WIDTH;
    login.main_window->size.height = LOGIN_WINDOW_HEIGHT;
    
    // Reallocate pixel buffer with correct size using heap_alloc (20MB heap, not 1MB malloc heap)
    extern void* heap_alloc(size_t size);
    extern void heap_free(void* ptr);
    
    if (login.main_window->pixel_buffer) {
        heap_free(login.main_window->pixel_buffer);
    }
    size_t buffer_size = LOGIN_WINDOW_WIDTH * LOGIN_WINDOW_HEIGHT * sizeof(uint32_t);
    login.main_window->pixel_buffer = heap_alloc(buffer_size);
    if (!login.main_window->pixel_buffer) {
        SERIAL_LOG("[LOGIN] Failed to allocate pixel buffer\n");
        return NULL;
    }
    SERIAL_LOG("[LOGIN] Pixel buffer allocated\n");
    SERIAL_LOG("[LOGIN] Window position: x=");
    SERIAL_LOG_DEC("", login.main_window->x);
    SERIAL_LOG("[LOGIN] Window position: y=");
    SERIAL_LOG_DEC("", login.main_window->y);
    SERIAL_LOG("[LOGIN] Window size: w=");
    SERIAL_LOG_DEC("", login.main_window->size.width);
    SERIAL_LOG("[LOGIN] Window size: h=");
    SERIAL_LOG_DEC("", login.main_window->size.height);
    
    // Initialize state
    login.login_failed = false;
    login.on_login_success = NULL;
    
    // Create UI controls (lightweight - not windows)
    int center_x = LOGIN_WINDOW_WIDTH / 2;
    int start_y = 60;
    
    // Username label (coordinates relative to window)
    SERIAL_LOG("[LOGIN] Initializing username label\n");
    textbox_init(&login.username_field_ctrl, 
                 center_x - FIELD_WIDTH / 2, 
                 start_y + LABEL_HEIGHT + 5,
                 FIELD_WIDTH, FIELD_HEIGHT, false);
    login.username_field_ctrl.on_change = on_username_change;
    login.username_field_ctrl.on_enter = on_username_enter;
    login.username_field_ctrl.user_data = &login;
    textbox_set_focus(&login.username_field_ctrl, true);
    
    label_init(&login.username_label_ctrl,
               center_x - FIELD_WIDTH / 2,
               start_y,
               "Username:", COLOR_TEXT_LABEL);
    
    // Password label and field
    int pass_y = start_y + LABEL_HEIGHT + FIELD_HEIGHT + SPACING + 5;
    label_init(&login.password_label_ctrl,
               center_x - FIELD_WIDTH / 2,
               pass_y,
               "Password:", COLOR_TEXT_LABEL);
    
    textbox_init(&login.password_field_ctrl,
                 center_x - FIELD_WIDTH / 2,
                 pass_y + LABEL_HEIGHT + 5,
                 FIELD_WIDTH, FIELD_HEIGHT, true);
    login.password_field_ctrl.on_change = on_password_change;
    login.password_field_ctrl.on_enter = on_password_enter;
    login.password_field_ctrl.user_data = &login;
    
    // Login and Cancel buttons
    int button_y = pass_y + LABEL_HEIGHT + FIELD_HEIGHT + SPACING + 10;
    button_init(&login.login_button_ctrl,
                center_x - BUTTON_WIDTH - 10,
                button_y,
                BUTTON_WIDTH, BUTTON_HEIGHT, "Login");
    login.login_button_ctrl.on_click = on_login_click;
    login.login_button_ctrl.user_data = &login;
    
    button_init(&login.cancel_button_ctrl,
                center_x + 10,
                button_y,
                BUTTON_WIDTH, BUTTON_HEIGHT, "Cancel");
    login.cancel_button_ctrl.on_click = on_cancel_click;
    login.cancel_button_ctrl.user_data = &login;
    
    // Error label
    label_init(&login.error_label_ctrl, 60, LOGIN_WINDOW_HEIGHT - 40, "", COLOR_ERROR);
    
    // Register event listeners for window (store for cleanup)
    login.mouse_click_listener = qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_MOUSE_CLICK, login.main_window,
        login_screen_event_handler, &login, 50);
    login.key_down_listener = qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_KEY_DOWN, login.main_window,
        login_screen_event_handler, &login, 50);
    login.key_press_listener = qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_KEY_PRESS, login.main_window,
        login_screen_event_handler, &login, 50);
    login.mouse_move_listener = qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_MOUSE_MOVE, login.main_window,
        login_screen_event_handler, &login, 50);
    
    // Set initial focus to username field
    textbox_set_focus(&login.username_field_ctrl, true);
    
    SERIAL_LOG("[LOGIN] Login screen created with simple controls\n");
    return &login;
}

void login_screen_destroy(LoginScreen* login) {
    if (!login) return;
    
    SERIAL_LOG("[LOGIN] Destroying login screen\n");
    
    // Unregister event listeners
    if (login->mouse_click_listener) qarma_input_event_unlisten(login->mouse_click_listener);
    if (login->key_down_listener) qarma_input_event_unlisten(login->key_down_listener);
    if (login->key_press_listener) qarma_input_event_unlisten(login->key_press_listener);
    if (login->mouse_move_listener) qarma_input_event_unlisten(login->mouse_move_listener);
    
    // Destroy main window
    if (login->main_window) {
        qarma_window_manager.remove_window(&qarma_window_manager, login->main_window->id);
        login->main_window = NULL;  // Set to NULL so kernel loop exits
    }
    
    SERIAL_LOG("[LOGIN] Login screen destroyed\n");
}

void login_screen_set_callback(LoginScreen* login, void (*callback)(const char* username)) {
    if (login) {
        login->on_login_success = callback;
    }
}

void login_screen_handle_event(LoginScreen* login, QARMA_INPUT_EVENT* event) {
    if (!login || !event) return;
    login_screen_event_handler(event, login);
}

void login_screen_update(LoginScreen* login) {
    if (!login) return;
    
    // Update controls (for cursor blinking, etc.)
    textbox_update(&login->username_field_ctrl);
    textbox_update(&login->password_field_ctrl);
}

void login_screen_render(LoginScreen* login) {
    if (!login || !login->main_window) return;
    
    uint32_t* buffer = login->main_window->pixel_buffer;
    int width = login->main_window->size.width;
    int height = login->main_window->size.height;
    
    // Fill background
    for (int i = 0; i < width * height; i++) {
        buffer[i] = COLOR_BACKGROUND;
    }
    
    // Render all controls to window buffer
    label_render(&login->username_label_ctrl, buffer, width, height);
    textbox_render(&login->username_field_ctrl, buffer, width, height);
    label_render(&login->password_label_ctrl, buffer, width, height);
    textbox_render(&login->password_field_ctrl, buffer, width, height);
    button_render(&login->login_button_ctrl, buffer, width, height);
    button_render(&login->cancel_button_ctrl, buffer, width, height);
    label_render(&login->error_label_ctrl, buffer, width, height);
    
    // Mark main window as needing refresh
    login->main_window->dirty = true;
}

// ============================================================================
// Helper Functions
// ============================================================================

bool login_screen_authenticate(const char* username, const char* password) {
    // Simple authentication for now - can be replaced with proper user database
    if (strcmp(username, "admin") == 0 && strcmp(password, "admin") == 0) {
        return true;
    }
    if (strcmp(username, "user") == 0 && strcmp(password, "password") == 0) {
        return true;
    }
    return false;
}

static void attempt_login(LoginScreen* login) {
    const char* username = textbox_get_text(&login->username_field_ctrl);
    const char* password = textbox_get_text(&login->password_field_ctrl);
    
    if (login_screen_authenticate(username, password)) {
        SERIAL_LOG("[LOGIN] Login successful\n");
        login->login_failed = false;
        
        if (login->on_login_success) {
            login->on_login_success(username);
        }
        
        login_screen_destroy(login);
    } else {
        SERIAL_LOG("[LOGIN] Login failed\n");
        login->login_failed = true;
        label_set_text(&login->error_label_ctrl, "Invalid username or password");
    }
}

// ============================================================================
// Control Callbacks
// ============================================================================

static void on_username_change(void* user_data, const char* text) {
    LoginScreen* login = (LoginScreen*)user_data;
    (void)text;  // Text is already stored in textbox
    
    // Clear error message when user starts typing
    if (login->login_failed) {
        login->login_failed = false;
        label_set_text(&login->error_label_ctrl, "");
    }
}

static void on_password_change(void* user_data, const char* text) {
    LoginScreen* login = (LoginScreen*)user_data;
    (void)text;
    
    // Clear error message when user starts typing
    if (login->login_failed) {
        login->login_failed = false;
        label_set_text(&login->error_label_ctrl, "");
    }
}

static void on_username_enter(void* user_data) {
    LoginScreen* login = (LoginScreen*)user_data;
    
    // Move focus to password field
    textbox_set_focus(&login->username_field_ctrl, false);
    textbox_set_focus(&login->password_field_ctrl, true);
}

static void on_password_enter(void* user_data) {
    LoginScreen* login = (LoginScreen*)user_data;
    
    // Attempt login when Enter is pressed in password field
    attempt_login(login);
}

static void on_login_click(void* user_data) {
    LoginScreen* login = (LoginScreen*)user_data;
    
    // Attempt login
    attempt_login(login);
}

static void on_cancel_click(void* user_data) {
    LoginScreen* login = (LoginScreen*)user_data;
    
    SERIAL_LOG("[LOGIN] Cancel button clicked\n");
    
    // Clear fields and reset
    textbox_set_text(&login->username_field_ctrl, "");
    textbox_set_text(&login->password_field_ctrl, "");
    textbox_set_focus(&login->username_field_ctrl, true);
    textbox_set_focus(&login->password_field_ctrl, false);
    button_set_focus(&login->login_button_ctrl, false);
    button_set_focus(&login->cancel_button_ctrl, false);
    login->login_failed = false;
    label_set_text(&login->error_label_ctrl, "");
    
    SERIAL_LOG("[LOGIN] Fields cleared, focus reset to username\n");
}

// Event handler - routes events from window to controls
static void login_screen_event_handler(QARMA_INPUT_EVENT* event, void* user_data) {
    LoginScreen* login = (LoginScreen*)user_data;
    if (!login || !event) return;
    
    switch (event->type) {
        case QARMA_INPUT_EVENT_MOUSE_CLICK:
            // Check buttons first
            button_handle_click(&login->login_button_ctrl, event->data.mouse.x, event->data.mouse.y);
            button_handle_click(&login->cancel_button_ctrl, event->data.mouse.x, event->data.mouse.y);
            
            // Then textboxes (for focus)
            if (control_point_in_bounds(&login->username_field_ctrl.base, event->data.mouse.x, event->data.mouse.y)) {
                textbox_set_focus(&login->username_field_ctrl, true);
                textbox_set_focus(&login->password_field_ctrl, false);
            } else if (control_point_in_bounds(&login->password_field_ctrl.base, event->data.mouse.x, event->data.mouse.y)) {
                textbox_set_focus(&login->username_field_ctrl, false);
                textbox_set_focus(&login->password_field_ctrl, true);
            }
            event->handled = true;
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_MOVE:
            // Update button hover states
            button_handle_mouse_move(&login->login_button_ctrl, event->data.mouse.x, event->data.mouse.y);
            button_handle_mouse_move(&login->cancel_button_ctrl, event->data.mouse.x, event->data.mouse.y);
            event->handled = true;
            break;
            
        case QARMA_INPUT_EVENT_KEY_DOWN:
            // Handle Tab for navigation between all controls
            if (event->data.key.keycode == 0x0F) {  // Tab key
                if (login->username_field_ctrl.has_focus) {
                    textbox_set_focus(&login->username_field_ctrl, false);
                    textbox_set_focus(&login->password_field_ctrl, true);
                } else if (login->password_field_ctrl.has_focus) {
                    textbox_set_focus(&login->password_field_ctrl, false);
                    button_set_focus(&login->login_button_ctrl, true);
                } else if (login->login_button_ctrl.has_focus) {
                    button_set_focus(&login->login_button_ctrl, false);
                    button_set_focus(&login->cancel_button_ctrl, true);
                } else if (login->cancel_button_ctrl.has_focus) {
                    button_set_focus(&login->cancel_button_ctrl, false);
                    textbox_set_focus(&login->username_field_ctrl, true);
                }
            }
            // Handle Enter to activate focused button
            else if (event->data.key.keycode == 0x1C) {  // Enter key
                if (login->login_button_ctrl.has_focus) {
                    button_activate(&login->login_button_ctrl);
                } else if (login->cancel_button_ctrl.has_focus) {
                    button_activate(&login->cancel_button_ctrl);
                } else if (login->username_field_ctrl.has_focus) {
                    textbox_handle_key(&login->username_field_ctrl, event->data.key.keycode);
                } else if (login->password_field_ctrl.has_focus) {
                    textbox_handle_key(&login->password_field_ctrl, event->data.key.keycode);
                }
            }
            // Send other keys to focused textbox
            else if (login->username_field_ctrl.has_focus) {
                textbox_handle_key(&login->username_field_ctrl, event->data.key.keycode);
            } else if (login->password_field_ctrl.has_focus) {
                textbox_handle_key(&login->password_field_ctrl, event->data.key.keycode);
            }
            event->handled = true;
            break;
            
        case QARMA_INPUT_EVENT_KEY_PRESS:
            // Send character to focused textbox
            if (login->username_field_ctrl.has_focus) {
                textbox_handle_char(&login->username_field_ctrl, event->data.key.character);
            } else if (login->password_field_ctrl.has_focus) {
                textbox_handle_char(&login->password_field_ctrl, event->data.key.character);
            }
            event->handled = true;
            break;
            
        default:
            break;
    }
}
