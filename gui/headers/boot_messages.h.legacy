#pragma once

#include "qarma_win_handle/qarma_win_handle.h"
#include "qarma_win_handle/qarma_input_events.h"
#include "gui.h"
#include "controls/close_button.h"
#include "core/stdtools.h"

#define MAX_BOOT_MESSAGES 100
#define MAX_MESSAGE_LENGTH 120

// Boot messages window state
typedef struct {
    QARMA_WIN_HANDLE* main_window;     // Window container
    
    // UI Controls
    CloseButton close_button_ctrl;
    
    // Message buffer
    char messages[MAX_BOOT_MESSAGES][MAX_MESSAGE_LENGTH];
    int message_count;
    int scroll_offset;  // For scrolling through messages
    
    // Event listeners
    QARMA_INPUT_EVENT_LISTENER* mouse_click_listener;
    QARMA_INPUT_EVENT_LISTENER* key_down_listener;
    QARMA_INPUT_EVENT_LISTENER* mouse_move_listener;
    
    // Callback for close button
    void (*on_close)(void* user_data);
    void* close_user_data;
    
} BootMessagesWindow;

// API Functions

// Create and show boot messages window
BootMessagesWindow* boot_messages_create(int x, int y, int width, int height);

// Destroy boot messages window
void boot_messages_destroy(BootMessagesWindow* bmw);

// Add a message to the window
void boot_messages_add(BootMessagesWindow* bmw, const char* message);

// Clear all messages
void boot_messages_clear(BootMessagesWindow* bmw);

// Render the window
void boot_messages_render(BootMessagesWindow* bmw);

// Update (for animations)
void boot_messages_update(BootMessagesWindow* bmw);

// Set close callback
void boot_messages_set_close_callback(BootMessagesWindow* bmw, void (*callback)(void* user_data), void* user_data);

// Handle input event
void boot_messages_handle_event(BootMessagesWindow* bmw, QARMA_INPUT_EVENT* event);
