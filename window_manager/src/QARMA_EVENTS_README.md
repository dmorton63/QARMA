# QARMA Input Event System

A comprehensive event handling architecture for QARMA that enables responsive, event-driven programming for windows, controls, and user interactions.

## Overview

The QARMA Input Event System provides:
- **Mouse Events**: Move, click, double-click, scroll, enter/leave
- **Keyboard Events**: Key down/up/press, character input with modifiers
- **Window Events**: Created, destroyed, moved, resized, focus, close
- **Display Events**: Refresh, resize, render requests
- **System Events**: Timers, ticks, shutdown, memory warnings
- **UI Control Events**: Button clicks, checkbox toggles, slider changes
- **Custom Events**: User-defined event types

## Key Features

- **Priority-based dispatch**: Higher priority handlers run first
- **Event propagation control**: Mark events as handled to stop propagation
- **Target filtering**: Listen only to events for specific windows/controls
- **Queued processing**: Queue events for batch processing
- **Static allocation**: No dynamic memory needed (uses fixed-size pool)

## Quick Start

### 1. Initialize the Event System

```c
#include "qarma_win_handle/qarma_input_events.h"

// Initialize at boot
qarma_input_events_init();
```

### 2. Register an Event Handler

```c
void on_mouse_click(QARMA_INPUT_EVENT* event, void* user_data) {
    int32_t x = event->data.mouse.x;
    int32_t y = event->data.mouse.y;
    
    // Handle the click
    printf("Mouse clicked at (%d, %d)\n", x, y);
    
    // Mark as handled to stop propagation
    event->handled = true;
}

// Register the handler (priority 50)
qarma_input_event_listen(
    QARMA_INPUT_EVENT_MOUSE_CLICK,
    on_mouse_click,
    NULL,  // user_data
    50     // priority
);
```

### 3. Dispatch Events

```c
// Create and dispatch mouse event
QARMA_INPUT_EVENT event = qarma_input_event_create_mouse_button(
    QARMA_INPUT_EVENT_MOUSE_CLICK,
    100, 200,  // x, y
    QARMA_MOUSE_BUTTON_LEFT,
    my_window  // target
);

qarma_input_event_dispatch(&event);
```

## Common Patterns

### Window-Specific Events

Only handle events for a specific window:

```c
qarma_input_event_listen_filtered(
    QARMA_INPUT_EVENT_MOUSE_MOVE,
    my_window,  // Only events for this window
    on_mouse_move,
    NULL,
    50
);
```

### Keyboard Shortcuts

Detect key combinations with modifiers:

```c
void on_key_press(QARMA_INPUT_EVENT* event, void* user_data) {
    uint32_t key = event->data.key.keycode;
    uint32_t mods = event->data.key.modifiers;
    
    // Ctrl+S
    if ((mods & QARMA_MOD_CTRL) && key == 's') {
        save_file();
        event->handled = true;
    }
}
```

### Queued Processing

For high-frequency events, queue them for batch processing:

```c
// In interrupt handler or high-frequency code
QARMA_INPUT_EVENT event = qarma_input_event_create_mouse_move(x, y, dx, dy, NULL);
qarma_input_event_queue(&event);

// Later, in main loop
qarma_input_event_process_queue();
```

### Event Logger

Log all events with a high-priority handler:

```c
void event_logger(QARMA_INPUT_EVENT* event, void* user_data) {
    printf("Event: %s\n", qarma_input_event_type_to_string(event->type));
    // Don't mark as handled - let other handlers process it
}

// Register with high priority (100) to run first
qarma_input_event_listen(0, event_logger, NULL, 100);
```

## Event Types

### Mouse Events
- `QARMA_INPUT_EVENT_MOUSE_MOVE` - Mouse cursor moved
- `QARMA_INPUT_EVENT_MOUSE_DOWN` - Mouse button pressed
- `QARMA_INPUT_EVENT_MOUSE_UP` - Mouse button released
- `QARMA_INPUT_EVENT_MOUSE_CLICK` - Mouse button clicked
- `QARMA_INPUT_EVENT_MOUSE_DBLCLICK` - Mouse button double-clicked
- `QARMA_INPUT_EVENT_MOUSE_SCROLL` - Mouse wheel scrolled
- `QARMA_INPUT_EVENT_MOUSE_ENTER` - Mouse entered window/control
- `QARMA_INPUT_EVENT_MOUSE_LEAVE` - Mouse left window/control

### Keyboard Events
- `QARMA_INPUT_EVENT_KEY_DOWN` - Key pressed down
- `QARMA_INPUT_EVENT_KEY_UP` - Key released
- `QARMA_INPUT_EVENT_KEY_PRESS` - Key press (down + up)
- `QARMA_INPUT_EVENT_CHAR_INPUT` - Character typed

### Window Events
- `QARMA_INPUT_EVENT_WIN_CREATED` - Window created
- `QARMA_INPUT_EVENT_WIN_DESTROYED` - Window destroyed
- `QARMA_INPUT_EVENT_WIN_MOVED` - Window moved
- `QARMA_INPUT_EVENT_WIN_RESIZED` - Window resized
- `QARMA_INPUT_EVENT_WIN_FOCUS_GAINED` - Window gained focus
- `QARMA_INPUT_EVENT_WIN_FOCUS_LOST` - Window lost focus
- `QARMA_INPUT_EVENT_WIN_CLOSE` - Window close requested

## Integration

### With Mouse Driver

```c
// In USB mouse driver callback
void usb_mouse_update_callback(int dx, int dy, uint8_t buttons) {
    static int mouse_x = 0, mouse_y = 0;
    
    mouse_x += dx;
    mouse_y += dy;
    
    // Dispatch move event
    QARMA_INPUT_EVENT event = qarma_input_event_create_mouse_move(
        mouse_x, mouse_y, dx, dy, NULL);
    qarma_input_event_dispatch(&event);
    
    // Dispatch button events if changed
    // ... button logic here ...
}
```

### With Window System

```c
// In QARMA window manager
void qarma_window_manager_update(void) {
    // Process queued events
    qarma_input_event_process_queue();
    
    // Update all windows
    for (int i = 0; i < window_count; i++) {
        windows[i]->update(windows[i]);
    }
}
```

## Configuration

Edit `qarma_input_events.c` to adjust:
- `MAX_LISTENERS` - Maximum event listeners (default: 64)
- `QARMA_INPUT_EVENT_QUEUE_SIZE` - Event queue size (default: 256)

## API Reference

See `qarma_input_events.h` for complete API documentation.

## Notes

- Event handlers run synchronously in the dispatch thread
- Use queued processing for high-frequency events
- Higher priority numbers run first (100 > 50 > 10)
- Mark events as `handled` to stop propagation
- Set `cancelled` to prevent default behavior
- Static allocation means no dynamic memory overhead

## Future Enhancements

- Touch/gesture events for touch screens
- Drag and drop events
- Animation/transition events
- Audio events
- Network events
