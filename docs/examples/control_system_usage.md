# Control System Usage Examples

## Overview

The QARMA control system uses a polymorphic approach where:
- Controls are first-class citizens with render/event function pointers
- Windows own a list of controls
- Visibility is per-control (no window-level feature flags needed)
- Clean separation of concerns

## Basic Control Integration

### 1. Adding a Control to a Window

```c
// Create a button
Button* my_button = button_create(10, 10, 100, 30, "Click Me");

// Wrap it in a ControlBase with function pointers
ControlBase* control = malloc(sizeof(ControlBase));
control->x = my_button->base.x;
control->y = my_button->base.y;
control->width = my_button->base.width;
control->height = my_button->base.height;
control->visible = true;
control->enabled = true;
control->id = control_generate_id();
control->instance = my_button;
control->render = my_button_render_wrapper;
control->handle_event = my_button_event_wrapper;
control->destroy = my_button_destroy_wrapper;

// Add to window
qarma_win_add_control(window, control);
```

### 2. Creating Control Wrapper Functions

```c
// Render wrapper for Button
void my_button_render_wrapper(void* instance, uint32_t* buffer, int width, int height) {
    Button* button = (Button*)instance;
    button_render(button, buffer, width, height);
}

// Event wrapper for Button
bool my_button_event_wrapper(void* instance, QARMA_INPUT_EVENT* event) {
    Button* button = (Button*)instance;
    return button_handle_input(button, event);
}

// Destroy wrapper for Button
void my_button_destroy_wrapper(void* instance) {
    Button* button = (Button*)instance;
    button_destroy(button);
    // Note: The ControlBase itself must be freed separately
}
```

### 3. Window Rendering

```c
// In your window render function
void my_window_render(QARMA_WIN_HANDLE* win) {
    // Draw window background, title bar, etc.
    draw_window_background(win);
    
    // Render all visible controls
    qarma_win_render_controls(win);
    
    // Any post-processing
    draw_window_border(win);
}
```

### 4. Event Handling

```c
// In your window event handler
void my_window_handle_event(QARMA_WIN_HANDLE* win, QARMA_INPUT_EVENT* event) {
    // Try to dispatch to controls first
    if (qarma_win_dispatch_event(win, event)) {
        return;  // Control handled it
    }
    
    // Handle window-level events (resize, move, etc.)
    handle_window_event(win, event);
}
```

## Advanced Examples

### Showing/Hiding Controls Dynamically

```c
// Hide a control by ID
ControlBase* control = qarma_win_get_control(window, control_id);
if (control) {
    control->visible = false;
    window->dirty = true;  // Mark for redraw
}

// Show it again
control->visible = true;
window->dirty = true;
```

### Adding a Status Bar

```c
// Create status bar
StatusBar* statusbar = status_bar_create(window->size.width, 32);
status_bar_add_button(statusbar, "File", STATUS_ALIGN_LEFT, NULL, 0);
status_bar_add_button(statusbar, "Edit", STATUS_ALIGN_LEFT, NULL, 0);
status_bar_add_label(statusbar, "Ready", STATUS_ALIGN_RIGHT);

// Wrap in ControlBase
ControlBase* sb_control = malloc(sizeof(ControlBase));
sb_control->x = 0;
sb_control->y = window->size.height - 32;
sb_control->width = window->size.width;
sb_control->height = 32;
sb_control->visible = true;
sb_control->enabled = true;
sb_control->id = control_generate_id();
sb_control->instance = statusbar;
sb_control->render = statusbar_render_wrapper;
sb_control->handle_event = statusbar_event_wrapper;
sb_control->destroy = statusbar_destroy_wrapper;

qarma_win_add_control(window, sb_control);
```

### Removing a Control

```c
// Find and remove
ControlBase* control = qarma_win_get_control(window, control_id);
if (control) {
    qarma_win_remove_control(window, control);
    
    // Clean up
    if (control->destroy) {
        control->destroy(control->instance);
    }
    free(control);
}
```

## Benefits of This Design

1. **Dynamic UI Composition**: Add/remove controls at runtime
2. **Reusable Controls**: Same control can be used in multiple windows
3. **Clean Separation**: Window doesn't need to know control internals
4. **Polymorphism**: All controls use the same interface
5. **Z-Order**: Event dispatch in reverse order (top-down)
6. **Visibility Management**: Per-control visibility flags
7. **Event Bubbling**: Controls can choose to handle or pass events

## Best Practices

- Always check `visible` before rendering
- Always check `enabled` before processing events
- Return `true` from event handler if event was consumed
- Set `window->dirty = true` when controls change
- Clean up both the control instance and the ControlBase wrapper
- Use reverse iteration for event dispatch (top control first)
