#pragma once

#include "qarma_win_handle.h"
#include "qarma_input_events.h"
#include "gui/gui.h"
#include "core/stdtools.h"

// Login screen state
typedef struct {
    QARMA_WIN_HANDLE* main_window;     // Parent window (single window)
    
    // UI Controls (lightweight - not windows)
    Label username_label_ctrl;
    TextBox username_field_ctrl;
    Label password_label_ctrl;
    TextBox password_field_ctrl;
    Button login_button_ctrl;
    Button cancel_button_ctrl;
    Label error_label_ctrl;
    
    // Event listeners (for cleanup)
    QARMA_INPUT_EVENT_LISTENER* mouse_click_listener;
    QARMA_INPUT_EVENT_LISTENER* key_down_listener;
    QARMA_INPUT_EVENT_LISTENER* key_press_listener;
    QARMA_INPUT_EVENT_LISTENER* mouse_move_listener;
    
    // State
    bool login_failed;
    
    // Callback for successful login
    void (*on_login_success)(const char* username);
    
} LoginScreen;

// API Functions

// Create and show login screen
LoginScreen* login_screen_create(void);

// Destroy login screen
void login_screen_destroy(LoginScreen* login);

// Update (for cursor blink, etc)
void login_screen_update(LoginScreen* login);

// Render the login screen
void login_screen_render(LoginScreen* login);

// Set login success callback
void login_screen_set_callback(LoginScreen* login, void (*callback)(const char* username));

// Handle input event directly (for direct keyboard routing)
void login_screen_handle_event(LoginScreen* login, QARMA_INPUT_EVENT* event);

// Helper functions
bool login_screen_authenticate(const char* username, const char* password);
