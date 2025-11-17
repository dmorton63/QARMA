#include "qarma_input_events.h"
#include "core/kernel.h"
#include "../memory.h"
#include "config.h"

// Static storage for event listeners (no dynamic allocation needed)
#define MAX_LISTENERS 64
static QARMA_INPUT_EVENT_LISTENER listener_pool[MAX_LISTENERS];
static uint32_t listener_count = 0;

// Global event system state
static struct {
    QARMA_INPUT_EVENT_LISTENER* listeners;    // Linked list head
    QARMA_INPUT_EVENT_QUEUE queue;            // Event queue
    bool initialized;
} event_system = { NULL, {0}, false };

// ============================================================================
// Initialization & Shutdown
// ============================================================================

void qarma_input_events_init(void) {
    if (event_system.initialized) {
        return;
    }
    
    event_system.listeners = NULL;
    event_system.queue.head = 0;
    event_system.queue.tail = 0;
    event_system.queue.count = 0;
    event_system.initialized = true;
    
    SERIAL_LOG("[QARMA_INPUT_EVENTS] Event system initialized\n");
}

void qarma_input_events_shutdown(void) {
    if (!event_system.initialized) {
        return;
    }
    
    // Clear all listeners (no deallocation needed with static pool)
    event_system.listeners = NULL;
    listener_count = 0;
    event_system.initialized = false;
    
    SERIAL_LOG("[QARMA_INPUT_EVENTS] Event system shutdown\n");
}

// ============================================================================
// Event Listener Registration
// ============================================================================

QARMA_INPUT_EVENT_LISTENER* qarma_input_event_listen(
    QARMA_INPUT_EVENT_TYPE event_type,
    QARMA_INPUT_EVENT_HANDLER handler,
    void* user_data,
    uint32_t priority
) {
    return qarma_input_event_listen_filtered(event_type, NULL, handler, user_data, priority);
}

QARMA_INPUT_EVENT_LISTENER* qarma_input_event_listen_filtered(
    QARMA_INPUT_EVENT_TYPE event_type,
    void* target_filter,
    QARMA_INPUT_EVENT_HANDLER handler,
    void* user_data,
    uint32_t priority
) {
    if (!event_system.initialized || !handler) {
        return NULL;
    }
    
    // Check if pool is full
    if (listener_count >= MAX_LISTENERS) {
        SERIAL_LOG("[QARMA_INPUT_EVENTS] Listener pool full\n");
        return NULL;
    }
    
    // Allocate from static pool
    QARMA_INPUT_EVENT_LISTENER* listener = &listener_pool[listener_count++];
    
    listener->event_type = event_type;
    listener->handler = handler;
    listener->user_data = user_data;
    listener->target_filter = target_filter;
    listener->priority = priority;
    listener->enabled = true;
    listener->next = NULL;
    
    // Insert into list sorted by priority (highest first)
    if (!event_system.listeners || event_system.listeners->priority < priority) {
        listener->next = event_system.listeners;
        event_system.listeners = listener;
    } else {
        QARMA_INPUT_EVENT_LISTENER* prev = event_system.listeners;
        while (prev->next && prev->next->priority >= priority) {
            prev = prev->next;
        }
        listener->next = prev->next;
        prev->next = listener;
    }
    
    return listener;
}

void qarma_input_event_unlisten(QARMA_INPUT_EVENT_LISTENER* listener) {
    if (!event_system.initialized || !listener) {
        return;
    }
    
    // Just disable it instead of removing (simpler with static pool)
    listener->enabled = false;
    
    // Remove from linked list
    if (event_system.listeners == listener) {
        event_system.listeners = listener->next;
    } else {
        QARMA_INPUT_EVENT_LISTENER* prev = event_system.listeners;
        while (prev && prev->next != listener) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = listener->next;
        }
    }
}

// ============================================================================
// Event Dispatch & Processing
// ============================================================================

void qarma_input_event_dispatch(QARMA_INPUT_EVENT* event) {
    if (!event_system.initialized || !event) {
        return;
    }
    
    event->handled = false;
    event->cancelled = false;
    
    // Iterate through all listeners
    QARMA_INPUT_EVENT_LISTENER* listener = event_system.listeners;
    while (listener) {
        // Check if listener should handle this event
        bool should_handle = listener->enabled &&
                           (listener->event_type == 0 || listener->event_type == event->type) &&
                           (listener->target_filter == NULL || listener->target_filter == event->target);
        
        if (should_handle) {
            listener->handler(event, listener->user_data);
            
            // Stop propagation if event was marked as handled
            if (event->handled) {
                break;
            }
        }
        
        listener = listener->next;
    }
}

bool qarma_input_event_queue(QARMA_INPUT_EVENT* event) {
    if (!event_system.initialized || !event) {
        return false;
    }
    
    // Check if queue is full
    if (event_system.queue.count >= QARMA_INPUT_EVENT_QUEUE_SIZE) {
        SERIAL_LOG("[QARMA_INPUT_EVENTS] Event queue full, dropping event\n");
        return false;
    }
    
    // Add to queue
    event_system.queue.events[event_system.queue.tail] = *event;
    event_system.queue.tail = (event_system.queue.tail + 1) % QARMA_INPUT_EVENT_QUEUE_SIZE;
    event_system.queue.count++;
    
    return true;
}

void qarma_input_event_process_queue(void) {
    if (!event_system.initialized) {
        return;
    }
    
    // Process all queued events
    while (event_system.queue.count > 0) {
        QARMA_INPUT_EVENT* event = &event_system.queue.events[event_system.queue.head];
        qarma_input_event_dispatch(event);
        
        event_system.queue.head = (event_system.queue.head + 1) % QARMA_INPUT_EVENT_QUEUE_SIZE;
        event_system.queue.count--;
    }
}

// ============================================================================
// Event Creation Helpers
// ============================================================================

QARMA_INPUT_EVENT qarma_input_event_create_mouse_move(int32_t x, int32_t y, int32_t dx, int32_t dy, void* target) {
    QARMA_INPUT_EVENT event = {0};
    event.type = QARMA_INPUT_EVENT_MOUSE_MOVE;
    event.timestamp = get_ticks();
    event.target = target;
    event.source = NULL;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.delta_x = dx;
    event.data.mouse.delta_y = dy;
    event.data.mouse.button = QARMA_MOUSE_BUTTON_NONE;
    event.data.mouse.modifiers = 0;
    event.handled = false;
    event.cancelled = false;
    return event;
}

QARMA_INPUT_EVENT qarma_input_event_create_mouse_button(
    QARMA_INPUT_EVENT_TYPE type, 
    int32_t x, 
    int32_t y, 
    QARMA_MOUSE_BUTTON button, 
    void* target
) {
    QARMA_INPUT_EVENT event = {0};
    event.type = type;
    event.timestamp = get_ticks();
    event.target = target;
    event.source = NULL;
    event.data.mouse.x = x;
    event.data.mouse.y = y;
    event.data.mouse.delta_x = 0;
    event.data.mouse.delta_y = 0;
    event.data.mouse.button = button;
    event.data.mouse.modifiers = 0;
    event.handled = false;
    event.cancelled = false;
    return event;
}

QARMA_INPUT_EVENT qarma_input_event_create_key(
    QARMA_INPUT_EVENT_TYPE type,
    uint32_t scancode,
    uint32_t keycode,
    uint32_t modifiers,
    void* target
) {
    QARMA_INPUT_EVENT event = {0};
    event.type = type;
    event.timestamp = get_ticks();
    event.target = target;
    event.source = NULL;
    event.data.key.scancode = scancode;
    event.data.key.keycode = keycode;
    event.data.key.character = 0;
    event.data.key.modifiers = modifiers;
    event.data.key.is_repeat = false;
    event.handled = false;
    event.cancelled = false;
    return event;
}

QARMA_INPUT_EVENT qarma_input_event_create_window(QARMA_INPUT_EVENT_TYPE type, void* window) {
    QARMA_INPUT_EVENT event = {0};
    event.type = type;
    event.timestamp = get_ticks();
    event.target = window;
    event.source = window;
    event.handled = false;
    event.cancelled = false;
    return event;
}

QARMA_INPUT_EVENT qarma_input_event_create_timer(uint32_t timer_id, uint64_t tick_count) {
    QARMA_INPUT_EVENT event = {0};
    event.type = QARMA_INPUT_EVENT_TIMER;
    event.timestamp = get_ticks();
    event.target = NULL;
    event.source = NULL;
    event.data.timer.timer_id = timer_id;
    event.data.timer.tick_count = tick_count;
    event.data.timer.elapsed_seconds = 0.0f;
    event.handled = false;
    event.cancelled = false;
    return event;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* qarma_input_event_type_to_string(QARMA_INPUT_EVENT_TYPE type) {
    switch (type) {
        // Mouse events
        case QARMA_INPUT_EVENT_MOUSE_MOVE:      return "MOUSE_MOVE";
        case QARMA_INPUT_EVENT_MOUSE_DOWN:      return "MOUSE_DOWN";
        case QARMA_INPUT_EVENT_MOUSE_UP:        return "MOUSE_UP";
        case QARMA_INPUT_EVENT_MOUSE_CLICK:     return "MOUSE_CLICK";
        case QARMA_INPUT_EVENT_MOUSE_DBLCLICK:  return "MOUSE_DBLCLICK";
        case QARMA_INPUT_EVENT_MOUSE_SCROLL:    return "MOUSE_SCROLL";
        case QARMA_INPUT_EVENT_MOUSE_ENTER:     return "MOUSE_ENTER";
        case QARMA_INPUT_EVENT_MOUSE_LEAVE:     return "MOUSE_LEAVE";
        
        // Keyboard events
        case QARMA_INPUT_EVENT_KEY_DOWN:        return "KEY_DOWN";
        case QARMA_INPUT_EVENT_KEY_UP:          return "KEY_UP";
        case QARMA_INPUT_EVENT_KEY_PRESS:       return "KEY_PRESS";
        case QARMA_INPUT_EVENT_CHAR_INPUT:      return "CHAR_INPUT";
        
        // Window events
        case QARMA_INPUT_EVENT_WIN_CREATED:     return "WIN_CREATED";
        case QARMA_INPUT_EVENT_WIN_DESTROYED:   return "WIN_DESTROYED";
        case QARMA_INPUT_EVENT_WIN_MOVED:       return "WIN_MOVED";
        case QARMA_INPUT_EVENT_WIN_RESIZED:     return "WIN_RESIZED";
        case QARMA_INPUT_EVENT_WIN_FOCUS_GAINED: return "WIN_FOCUS_GAINED";
        case QARMA_INPUT_EVENT_WIN_FOCUS_LOST:  return "WIN_FOCUS_LOST";
        case QARMA_INPUT_EVENT_WIN_SHOWN:       return "WIN_SHOWN";
        case QARMA_INPUT_EVENT_WIN_HIDDEN:      return "WIN_HIDDEN";
        case QARMA_INPUT_EVENT_WIN_MINIMIZED:   return "WIN_MINIMIZED";
        case QARMA_INPUT_EVENT_WIN_MAXIMIZED:   return "WIN_MAXIMIZED";
        case QARMA_INPUT_EVENT_WIN_CLOSE:       return "WIN_CLOSE";
        
        // Display events
        case QARMA_INPUT_EVENT_DISPLAY_REFRESH: return "DISPLAY_REFRESH";
        case QARMA_INPUT_EVENT_DISPLAY_RESIZE:  return "DISPLAY_RESIZE";
        case QARMA_INPUT_EVENT_RENDER_NEEDED:   return "RENDER_NEEDED";
        
        // System events
        case QARMA_INPUT_EVENT_TIMER:           return "TIMER";
        case QARMA_INPUT_EVENT_TICK:            return "TICK";
        case QARMA_INPUT_EVENT_SHUTDOWN:        return "SHUTDOWN";
        case QARMA_INPUT_EVENT_MEMORY_LOW:      return "MEMORY_LOW";
        
        // UI control events
        case QARMA_INPUT_EVENT_BUTTON_CLICK:    return "BUTTON_CLICK";
        case QARMA_INPUT_EVENT_BUTTON_PRESS:    return "BUTTON_PRESS";
        case QARMA_INPUT_EVENT_BUTTON_RELEASE:  return "BUTTON_RELEASE";
        case QARMA_INPUT_EVENT_CHECKBOX_TOGGLE: return "CHECKBOX_TOGGLE";
        case QARMA_INPUT_EVENT_SLIDER_CHANGE:   return "SLIDER_CHANGE";
        case QARMA_INPUT_EVENT_TEXT_CHANGE:     return "TEXT_CHANGE";
        
        // Custom
        case QARMA_INPUT_EVENT_CUSTOM:          return "CUSTOM";
        
        default:                          return "UNKNOWN";
    }
}
