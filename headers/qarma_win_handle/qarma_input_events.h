#pragma once

#include "core/stdtools.h"

// ============================================================================
// QARMA Event System
// A comprehensive event handling architecture for QARMA
// ============================================================================

// Event Types
typedef enum {
    // Input Events
    QARMA_INPUT_EVENT_MOUSE_MOVE       = 0x0100,
    QARMA_INPUT_EVENT_MOUSE_DOWN       = 0x0101,
    QARMA_INPUT_EVENT_MOUSE_UP         = 0x0102,
    QARMA_INPUT_EVENT_MOUSE_CLICK      = 0x0103,
    QARMA_INPUT_EVENT_MOUSE_DBLCLICK   = 0x0104,
    QARMA_INPUT_EVENT_MOUSE_SCROLL     = 0x0105,
    QARMA_INPUT_EVENT_MOUSE_ENTER      = 0x0106,
    QARMA_INPUT_EVENT_MOUSE_LEAVE      = 0x0107,
    
    QARMA_INPUT_EVENT_KEY_DOWN         = 0x0200,
    QARMA_INPUT_EVENT_KEY_UP           = 0x0201,
    QARMA_INPUT_EVENT_KEY_PRESS        = 0x0202,  // Key down + up
    QARMA_INPUT_EVENT_CHAR_INPUT       = 0x0203,  // Character typed
    
    // Window Events
    QARMA_INPUT_EVENT_WIN_CREATED      = 0x0300,
    QARMA_INPUT_EVENT_WIN_DESTROYED    = 0x0301,
    QARMA_INPUT_EVENT_WIN_MOVED        = 0x0302,
    QARMA_INPUT_EVENT_WIN_RESIZED      = 0x0303,
    QARMA_INPUT_EVENT_WIN_FOCUS_GAINED = 0x0304,
    QARMA_INPUT_EVENT_WIN_FOCUS_LOST   = 0x0305,
    QARMA_INPUT_EVENT_WIN_SHOWN        = 0x0306,
    QARMA_INPUT_EVENT_WIN_HIDDEN       = 0x0307,
    QARMA_INPUT_EVENT_WIN_MINIMIZED    = 0x0308,
    QARMA_INPUT_EVENT_WIN_MAXIMIZED    = 0x0309,
    QARMA_INPUT_EVENT_WIN_CLOSE        = 0x030A,
    
    // Display/Render Events
    QARMA_INPUT_EVENT_DISPLAY_REFRESH  = 0x0400,
    QARMA_INPUT_EVENT_DISPLAY_RESIZE   = 0x0401,
    QARMA_INPUT_EVENT_RENDER_NEEDED    = 0x0402,
    
    // System Events
    QARMA_INPUT_EVENT_TIMER            = 0x0500,
    QARMA_INPUT_EVENT_TICK             = 0x0501,
    QARMA_INPUT_EVENT_SHUTDOWN         = 0x0502,
    QARMA_INPUT_EVENT_MEMORY_LOW       = 0x0503,
    
    // UI Control Events
    QARMA_INPUT_EVENT_BUTTON_CLICK     = 0x0600,
    QARMA_INPUT_EVENT_BUTTON_PRESS     = 0x0601,
    QARMA_INPUT_EVENT_BUTTON_RELEASE   = 0x0602,
    QARMA_INPUT_EVENT_CHECKBOX_TOGGLE  = 0x0603,
    QARMA_INPUT_EVENT_SLIDER_CHANGE    = 0x0604,
    QARMA_INPUT_EVENT_TEXT_CHANGE      = 0x0605,
    
    // Custom/User Events
    QARMA_INPUT_EVENT_CUSTOM           = 0x1000,
    
} QARMA_INPUT_EVENT_TYPE;

// Mouse Button Identifiers
typedef enum {
    QARMA_MOUSE_BUTTON_NONE   = 0,
    QARMA_MOUSE_BUTTON_LEFT   = 1,
    QARMA_MOUSE_BUTTON_MIDDLE = 2,
    QARMA_MOUSE_BUTTON_RIGHT  = 3,
} QARMA_MOUSE_BUTTON;

// Keyboard Modifiers
typedef enum {
    QARMA_MOD_NONE     = 0x00,
    QARMA_MOD_SHIFT    = 0x01,
    QARMA_MOD_CTRL     = 0x02,
    QARMA_MOD_ALT      = 0x04,
    QARMA_MOD_SUPER    = 0x08,
    QARMA_MOD_CAPS     = 0x10,
    QARMA_MOD_NUM      = 0x20,
} QARMA_KEY_MOD;

// Event Data Structures

typedef struct {
    int32_t x;              // Mouse X position
    int32_t y;              // Mouse Y position
    int32_t delta_x;        // X movement since last event
    int32_t delta_y;        // Y movement since last event
    QARMA_MOUSE_BUTTON button;
    uint32_t modifiers;     // Keyboard modifiers (QARMA_KEY_MOD)
} QARMA_MOUSE_EVENT_DATA;

typedef struct {
    uint32_t scancode;      // Hardware scan code
    uint32_t keycode;       // Logical key code
    uint32_t character;     // Unicode character (if printable)
    uint32_t modifiers;     // Active modifiers
    bool is_repeat;         // True if key is held down
} QARMA_KEY_EVENT_DATA;

typedef struct {
    int32_t old_x;
    int32_t old_y;
    int32_t new_x;
    int32_t new_y;
} QARMA_MOVE_EVENT_DATA;

typedef struct {
    uint32_t old_width;
    uint32_t old_height;
    uint32_t new_width;
    uint32_t new_height;
} QARMA_RESIZE_EVENT_DATA;

typedef struct {
    uint32_t timer_id;
    uint64_t tick_count;
    float elapsed_seconds;
} QARMA_TIMER_EVENT_DATA;

typedef struct {
    uint32_t control_id;    // ID of button/control
    void* control_ptr;      // Pointer to control structure
    void* user_data;        // User-defined data
} QARMA_CONTROL_EVENT_DATA;

// Generic Event Structure
typedef struct QARMA_INPUT_EVENT {
    QARMA_INPUT_EVENT_TYPE type;          // Event type
    uint64_t timestamp;             // When event occurred (ticks)
    void* target;                   // Target object (window, control, etc)
    void* source;                   // Source object that generated event
    
    // Event-specific data (union for efficiency)
    union {
        QARMA_MOUSE_EVENT_DATA mouse;
        QARMA_KEY_EVENT_DATA key;
        QARMA_MOVE_EVENT_DATA move;
        QARMA_RESIZE_EVENT_DATA resize;
        QARMA_TIMER_EVENT_DATA timer;
        QARMA_CONTROL_EVENT_DATA control;
        void* custom_data;          // For custom events
    } data;
    
    bool handled;                   // Mark as handled to stop propagation
    bool cancelled;                 // Cancel default behavior
} QARMA_INPUT_EVENT;

// Event Handler Callback Type
typedef void (*QARMA_INPUT_EVENT_HANDLER)(QARMA_INPUT_EVENT* event, void* user_data);

// Event Listener Structure
typedef struct QARMA_INPUT_EVENT_LISTENER {
    QARMA_INPUT_EVENT_TYPE event_type;        // Which event to listen for (0 = all)
    QARMA_INPUT_EVENT_HANDLER handler;        // Callback function
    void* user_data;                    // User-provided context
    void* target_filter;                // Only handle events for this target (NULL = all)
    uint32_t priority;                  // Higher priority handlers run first
    bool enabled;                       // Can temporarily disable
    struct QARMA_INPUT_EVENT_LISTENER* next;  // Linked list
} QARMA_INPUT_EVENT_LISTENER;

// Event Queue (for deferred processing)
#define QARMA_INPUT_EVENT_QUEUE_SIZE 256

typedef struct {
    QARMA_INPUT_EVENT events[QARMA_INPUT_EVENT_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} QARMA_INPUT_EVENT_QUEUE;

// Event System API

// Initialize the event system
void qarma_input_events_init(void);

// Shutdown the event system
void qarma_input_events_shutdown(void);

// Register an event listener
// Returns listener handle (can be used to unregister)
QARMA_INPUT_EVENT_LISTENER* qarma_input_event_listen(
    QARMA_INPUT_EVENT_TYPE event_type,
    QARMA_INPUT_EVENT_HANDLER handler,
    void* user_data,
    uint32_t priority
);

// Register a filtered event listener (only for specific target)
QARMA_INPUT_EVENT_LISTENER* qarma_input_event_listen_filtered(
    QARMA_INPUT_EVENT_TYPE event_type,
    void* target_filter,
    QARMA_INPUT_EVENT_HANDLER handler,
    void* user_data,
    uint32_t priority
);

// Unregister an event listener
void qarma_input_event_unlisten(QARMA_INPUT_EVENT_LISTENER* listener);

// Dispatch an event immediately (synchronous)
void qarma_input_event_dispatch(QARMA_INPUT_EVENT* event);

// Queue an event for later processing (asynchronous)
bool qarma_input_event_queue(QARMA_INPUT_EVENT* event);

// Process all queued events
void qarma_input_event_process_queue(void);

// Create event helper functions
QARMA_INPUT_EVENT qarma_input_event_create_mouse_move(int32_t x, int32_t y, int32_t dx, int32_t dy, void* target);
QARMA_INPUT_EVENT qarma_input_event_create_mouse_button(QARMA_INPUT_EVENT_TYPE type, int32_t x, int32_t y, QARMA_MOUSE_BUTTON button, void* target);
QARMA_INPUT_EVENT qarma_input_event_create_key(QARMA_INPUT_EVENT_TYPE type, uint32_t scancode, uint32_t keycode, uint32_t modifiers, void* target);
QARMA_INPUT_EVENT qarma_input_event_create_window(QARMA_INPUT_EVENT_TYPE type, void* window);
QARMA_INPUT_EVENT qarma_input_event_create_timer(uint32_t timer_id, uint64_t tick_count);

// Utility functions
const char* qarma_input_event_type_to_string(QARMA_INPUT_EVENT_TYPE type);
// Note: get_ticks() is defined in core/timer.c
