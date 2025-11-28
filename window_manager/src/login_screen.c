/*
 * QARMA - Login Screen Implementation (New Architecture)
 */

#include "login_screen.h"
#include "memory/heap.h"
#include "core/string.h"
#include "core/kernel.h"
#include "core/handle_manager.h"
#include "graphics/graphics.h"
#include "config.h"

#define LOGIN_WINDOW_WIDTH 400
#define LOGIN_WINDOW_HEIGHT 300
#define CONTROL_SPACING 15
#define LABEL_HEIGHT 20
#define TEXTBOX_HEIGHT 30
#define BUTTON_HEIGHT 35
#define BUTTON_WIDTH 100

// Colors
#define LOGIN_BG_COLOR 0xFF2C3E50
#define LOGIN_BORDER_COLOR 0xFF3498DB
#define ERROR_COLOR 0xFFE74C3C

// Message handler for login frame
static int32_t login_frame_message_handler(qarma_handle_t recipient, qarma_message_t* msg) {
    if (!msg) return 0;
    
    qarma_frame_t* frame = (qarma_frame_t*)handle_get_object(recipient);
    if (!frame) return 0;
    
    login_screen_t* login = (login_screen_t*)frame->user_data;
    if (!login) return 0;
    
    switch (msg->type) {
        case MSG_PAINT:
            login_screen_render(login);
            return 1;
            
        case MSG_KEYDOWN: {
            // Check for Enter key
            uint8_t scancode = (uint8_t)(msg->wparam & 0xFF);
            if (scancode == 0x1C) {  // Enter
                login_screen_do_login(login);
                return 1;
            }
            // Check for Escape
            if (scancode == 0x01) {  // ESC
                login_screen_do_cancel(login);
                return 1;
            }
            return 0;
        }
        
        default:
            return 0;
    }
}

// Button click handlers
static void on_login_button_click(qarma_control_t* button, void* user_data) {
    login_screen_t* login = (login_screen_t*)user_data;
    if (login) {
        login_screen_do_login(login);
    }
}

static void on_cancel_button_click(qarma_control_t* button, void* user_data) {
    login_screen_t* login = (login_screen_t*)user_data;
    if (login) {
        login_screen_do_cancel(login);
    }
}

login_screen_t* login_screen_create(void) {
    SERIAL_LOG("[LOGIN_NEW] Creating login screen\n");
    
    // Get screen dimensions for centering
    display_info_t* display = graphics_get_info();
    int screen_w = display ? display->width : 1024;
    int screen_h = display ? display->height : 768;
    
    int x = (screen_w - LOGIN_WINDOW_WIDTH) / 2;
    int y = (screen_h - LOGIN_WINDOW_HEIGHT) / 2;
    
    login_screen_t* login = (login_screen_t*)heap_alloc(sizeof(login_screen_t));
    if (!login) {
        SERIAL_LOG("[LOGIN_NEW] ERROR: Failed to allocate login_screen_t\n");
        return NULL;
    }
    
    memset(login, 0, sizeof(login_screen_t));
    login->login_failed = false;
    login->active = true;
    
    // Create main frame
    login->main_frame = frame_create(
        NULL,  // No parent
        x, y, LOGIN_WINDOW_WIDTH, LOGIN_WINDOW_HEIGHT,
        FRAME_STYLE_BORDER | FRAME_STYLE_TITLE_BAR,
        "QARMA OS Login"
    );
    
    if (!login->main_frame) {
        SERIAL_LOG("[LOGIN_NEW] ERROR: Failed to create frame\n");
        heap_free(login);
        return NULL;
    }
    
    // Set frame colors
    login->main_frame->background.red = (LOGIN_BG_COLOR >> 16) & 0xFF;
    login->main_frame->background.green = (LOGIN_BG_COLOR >> 8) & 0xFF;
    login->main_frame->background.blue = LOGIN_BG_COLOR & 0xFF;
    login->main_frame->background.alpha = 255;
    
    login->main_frame->border_color.red = (LOGIN_BORDER_COLOR >> 16) & 0xFF;
    login->main_frame->border_color.green = (LOGIN_BORDER_COLOR >> 8) & 0xFF;
    login->main_frame->border_color.blue = LOGIN_BORDER_COLOR & 0xFF;
    login->main_frame->border_color.alpha = 255;
    
    // Set message handler
    login->main_frame->message_handler = login_frame_message_handler;
    login->main_frame->user_data = login;
    
    // Layout controls vertically
    int current_y = 60;  // Start below title bar
    int label_x = 30;
    int field_x = 30;
    int field_width = LOGIN_WINDOW_WIDTH - 60;
    
    // Title label
    login->title_label = label_create(
        login->main_frame,
        "title_label",
        (LOGIN_WINDOW_WIDTH - 200) / 2, 30,
        200, LABEL_HEIGHT,
        "QARMA OS - Login"
    );
    if (login->title_label) {
        frame_add_control(login->main_frame, login->title_label);
    }
    
    // Username label
    login->username_label = label_create(
        login->main_frame,
        "username_label",
        label_x, current_y,
        100, LABEL_HEIGHT,
        "Username:"
    );
    if (login->username_label) {
        frame_add_control(login->main_frame, login->username_label);
    }
    current_y += LABEL_HEIGHT + 5;
    
    // Username textbox
    login->username_field = textbox_create(
        login->main_frame,
        "username_field",
        field_x, current_y,
        field_width, TEXTBOX_HEIGHT
    );
    if (login->username_field) {
        textbox_data_t* data = (textbox_data_t*)login->username_field->implementation_data;
        if (data) {
            data->max_length = 32;
        }
        frame_add_control(login->main_frame, login->username_field);
    }
    current_y += TEXTBOX_HEIGHT + CONTROL_SPACING;
    
    // Password label
    login->password_label = label_create(
        login->main_frame,
        "password_label",
        label_x, current_y,
        100, LABEL_HEIGHT,
        "Password:"
    );
    if (login->password_label) {
        frame_add_control(login->main_frame, login->password_label);
    }
    current_y += LABEL_HEIGHT + 5;
    
    // Password textbox (password mode)
    login->password_field = textbox_create(
        login->main_frame,
        "password_field",
        field_x, current_y,
        field_width, TEXTBOX_HEIGHT
    );
    if (login->password_field) {
        textbox_data_t* data = (textbox_data_t*)login->password_field->implementation_data;
        if (data) {
            data->password_mode = true;
            data->password_char = '*';
            data->max_length = 32;
        }
        frame_add_control(login->main_frame, login->password_field);
    }
    current_y += TEXTBOX_HEIGHT + CONTROL_SPACING;
    
    // Error label (initially empty)
    login->error_label = label_create(
        login->main_frame,
        "error_label",
        label_x, current_y,
        field_width, LABEL_HEIGHT,
        ""
    );
    if (login->error_label) {
        frame_add_control(login->main_frame, login->error_label);
    }
    current_y += LABEL_HEIGHT + CONTROL_SPACING;
    
    // Buttons (centered at bottom)
    int button_y = LOGIN_WINDOW_HEIGHT - BUTTON_HEIGHT - 40;
    int button_spacing = 20;
    int total_button_width = (BUTTON_WIDTH * 2) + button_spacing;
    int button_start_x = (LOGIN_WINDOW_WIDTH - total_button_width) / 2;
    
    // Login button
    login->login_button = button_create(
        login->main_frame,
        "login_button",
        button_start_x, button_y,
        BUTTON_WIDTH, BUTTON_HEIGHT,
        "Login"
    );
    if (login->login_button) {
        button_data_t* btn_data = (button_data_t*)login->login_button->implementation_data;
        if (btn_data) {
            btn_data->on_click = on_login_button_click;
        }
        login->login_button->user_data = login;
        frame_add_control(login->main_frame, login->login_button);
    }
    
    // Cancel button
    login->cancel_button = button_create(
        login->main_frame,
        "cancel_button",
        button_start_x + BUTTON_WIDTH + button_spacing, button_y,
        BUTTON_WIDTH, BUTTON_HEIGHT,
        "Cancel"
    );
    if (login->cancel_button) {
        button_data_t* btn_data = (button_data_t*)login->cancel_button->implementation_data;
        if (btn_data) {
            btn_data->on_click = on_cancel_button_click;
        }
        login->cancel_button->user_data = login;
        frame_add_control(login->main_frame, login->cancel_button);
    }
    
    SERIAL_LOG("[LOGIN_NEW] Login screen created successfully\n");
    return login;
}

void login_screen_destroy(login_screen_t* login) {
    if (!login) return;
    
    SERIAL_LOG("[LOGIN_NEW] Destroying login screen\n");
    
    // Controls are owned by frame and will be destroyed with it
    if (login->main_frame) {
        frame_destroy(login->main_frame);
    }
    
    heap_free(login);
}

void login_screen_set_callback(login_screen_t* login, void (*callback)(const char* username)) {
    if (!login) return;
    login->on_login_success = callback;
}

void login_screen_set_visible(login_screen_t* login, bool visible) {
    if (!login || !login->main_frame) return;
    
    login->main_frame->visible = visible;
    login->active = visible;
}

void login_screen_handle_event(login_screen_t* login, QARMA_INPUT_EVENT* event) {
    if (!login || !event || !login->active) return;
    
    // Convert event to message and send to frame
    qarma_message_t msg = {0};
    msg.target = login->main_frame->handle;
    
    switch (event->type) {
        case QARMA_INPUT_EVENT_KEY_DOWN:
            msg.type = MSG_KEYDOWN;
            msg.wparam = event->data.key.scancode | (event->data.key.modifiers << 8);
            msg.lparam = event->data.key.character;
            break;
            
        case QARMA_INPUT_EVENT_KEY_PRESS:
            msg.type = MSG_CHAR;
            msg.wparam = event->data.key.scancode;
            msg.lparam = event->data.key.character;
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_MOVE:
            msg.type = MSG_MOUSEMOVE;
            msg.wparam = event->data.mouse.x | (event->data.mouse.y << 16);
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_DOWN:
            msg.type = MSG_LBUTTONDOWN;
            msg.wparam = event->data.mouse.x | (event->data.mouse.y << 16);
            break;
            
        default:
            return;
    }
    
    // Send to frame
    if (login->main_frame->message_handler) {
        login->main_frame->message_handler(login->main_frame->handle, &msg);
    }
}

bool login_screen_authenticate(const char* username, const char* password) {
    if (!username || !password) return false;
    
    // Simple authentication - accept "admin" with any password
    // or "user" with password "pass"
    if (strcmp(username, "admin") == 0) {
        return true;
    }
    
    if (strcmp(username, "user") == 0 && strcmp(password, "pass") == 0) {
        return true;
    }
    
    return false;
}

void login_screen_do_login(login_screen_t* login) {
    if (!login) return;
    
    SERIAL_LOG("[LOGIN_NEW] Login attempt\n");
    
    // Get username and password
    textbox_data_t* username_data = (textbox_data_t*)login->username_field->implementation_data;
    textbox_data_t* password_data = (textbox_data_t*)login->password_field->implementation_data;
    
    if (!username_data || !password_data) {
        SERIAL_LOG("[LOGIN_NEW] ERROR: Invalid textbox data\n");
        return;
    }
    
    const char* username = username_data->text;
    const char* password = password_data->text;
    
    // Authenticate
    if (login_screen_authenticate(username, password)) {
        SERIAL_LOG("[LOGIN_NEW] Login successful for user: ");
        SERIAL_LOG(username);
        SERIAL_LOG("\n");
        
        // Clear error
        if (login->error_label) {
            label_set_text(login->error_label, "");
        }
        
        // Call success callback
        if (login->on_login_success) {
            login->on_login_success(username);
        }
        
        // Hide login screen
        login_screen_set_visible(login, false);
    } else {
        SERIAL_LOG("[LOGIN_NEW] Login failed\n");
        
        // Show error
        if (login->error_label) {
            label_set_text(login->error_label, "Invalid username or password");
        }
        
        login->login_failed = true;
        
        // Clear password field
        if (password_data) {
            password_data->text[0] = '\0';
            password_data->cursor_pos = 0;
        }
    }
}

void login_screen_do_cancel(login_screen_t* login) {
    if (!login) return;
    
    SERIAL_LOG("[LOGIN_NEW] Login cancelled\n");
    
    // Hide login screen
    login_screen_set_visible(login, false);
    login->active = false;
}

void login_screen_update(login_screen_t* login) {
    if (!login || !login->active) return;
    
    // Update for cursor blink, animations, etc.
    // Currently nothing to update
}

void login_screen_render(login_screen_t* login) {
    if (!login || !login->main_frame || !login->main_frame->visible) return;
    
    // Render frame (which will render all child controls)
    frame_render(login->main_frame);
}
