#ifndef MOUSE_SUBSYSTEM_H
#define MOUSE_SUBSYSTEM_H

#include "mouse.h"
#include "stdtools.h"

// Mouse event types
typedef enum {
    MOUSE_EVENT_MOVE,
    MOUSE_EVENT_DOWN,
    MOUSE_EVENT_UP,
    MOUSE_EVENT_CLICK,
    MOUSE_EVENT_DOUBLE_CLICK,
    MOUSE_EVENT_ENTER,
    MOUSE_EVENT_LEAVE,
    MOUSE_EVENT_SCROLL
} mouse_event_type_t;

// Mouse button identifiers
typedef enum {
    MOUSE_BUTTON_NONE = 0,
    MOUSE_BUTTON_LEFT = 1,
    MOUSE_BUTTON_RIGHT = 2,
    MOUSE_BUTTON_MIDDLE = 4
} mouse_button_t;

// Mouse event structure
typedef struct {
    mouse_event_type_t type;
    int x;
    int y;
    int dx;
    int dy;
    mouse_button_t button;
    bool pressed;
    int scroll_delta;
    uint32_t timestamp;
} mouse_event_t;

// Mouse event handler callback
typedef void (*mouse_event_handler_t)(mouse_event_t* event, void* context);

// Mouse driver interface (similar to video subsystem)
typedef struct {
    const char* name;
    int (*init)(void);
    void (*shutdown)(void);
    void (*get_position)(int* x, int* y);
    void (*set_position)(int x, int y);
    bool (*get_button_state)(mouse_button_t button);
} mouse_driver_t;

// Mouse subsystem registration
void mouse_subsystem_init(void);
int mouse_subsystem_register_driver(mouse_driver_t* driver);
void mouse_subsystem_set_active_driver(const char* name);

// Event registration and dispatch
void mouse_register_event_handler(mouse_event_handler_t handler, void* context);
void mouse_unregister_event_handler(mouse_event_handler_t handler);
void mouse_dispatch_event(mouse_event_t* event);

// Cursor management
void mouse_set_cursor_visible(bool visible);
bool mouse_is_cursor_visible(void);
void mouse_set_cursor_bounds(int x, int y, int width, int height);

#endif // MOUSE_SUBSYSTEM_H
