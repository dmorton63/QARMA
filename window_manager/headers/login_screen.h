/*
 * QARMA - Login Screen (New Architecture)
 * 
 * A login dialog using qarma_control_t* arrays
 */

#ifndef LOGIN_SCREEN_H
#define LOGIN_SCREEN_H

#include "gui/frame.h"
#include "gui/qarma_control.h"
#include "gui/controls/qarma_textbox.h"
#include "gui/controls/qarma_button.h"
#include "gui/controls/qarma_label.h"
#include "qarma_input_events.h"
#include "kernel_types.h"

// Login screen structure
typedef struct login_screen_t {
    qarma_frame_t* main_frame;          // Container frame
    
    qarma_control_t* title_label;       // "QARMA OS Login"
    qarma_control_t* username_label;    // "Username:"
    qarma_control_t* username_field;    // Username textbox
    qarma_control_t* password_label;    // "Password:"
    qarma_control_t* password_field;    // Password textbox (password mode)
    qarma_control_t* login_button;      // "Login" button
    qarma_control_t* cancel_button;     // "Cancel" button
    qarma_control_t* error_label;       // Error message display
    
    // State
    bool login_failed;
    bool active;
    
    // Callback for successful login
    void (*on_login_success)(const char* username);
} login_screen_t;

/**
 * Create and show login screen
 */
login_screen_t* login_screen_create(void);

/**
 * Destroy login screen
 */
void login_screen_destroy(login_screen_t* login);

/**
 * Update (for cursor blink, etc)
 */
void login_screen_update(login_screen_t* login);

/**
 * Render the login screen
 */
void login_screen_render(login_screen_t* login);

/**
 * Set login success callback
 */
void login_screen_set_callback(login_screen_t* login, void (*callback)(const char* username));

/**
 * Handle input event directly
 */
void login_screen_handle_event(login_screen_t* login, QARMA_INPUT_EVENT* event);

/**
 * Show/hide login screen
 */
void login_screen_set_visible(login_screen_t* login, bool visible);

/**
 * Authenticate user credentials
 */
bool login_screen_authenticate(const char* username, const char* password);

/**
 * Handle login button click
 */
void login_screen_do_login(login_screen_t* login);

/**
 * Handle cancel button click
 */
void login_screen_do_cancel(login_screen_t* login);

#endif // LOGIN_SCREEN_H
