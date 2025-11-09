#include <X11/Xlib.h>
#include <X11/Xutil.h>  // For extended window attributes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "window_subsystem.h"
#include "subsystem_registry.h"  // For proper subsystem registration
#include "message_manager.h"     // For message handling
#include "cpu_core_manager.h"    // For core affinity and dispatch logging
#include "dispatch_logf.h"       // For logging
#include <GLES2/gl2.h>           // For OpenGL ES 2.0 functions

// Declare the full window subsystem data structure
DispatchSubsystem window_dispatch_subsystem = {
    .name = "WindowManager",
    .fn = window_manager_tick,
    .core_affinity = -1,
    .dispatch_count = 0,
    .last_error_code = 0,
    .last_run = 0,
    .history_count = 0,
    .priority = PRIORITY_HIGH
};

Display* x_display;
Window win;

void create_splash_window(void) {
    
    x_display = XOpenDisplay(NULL);
    if (!x_display) {
        fprintf(stderr, "X11: Failed to open display\n");
        exit(1);
    }

    Window root = DefaultRootWindow(x_display);
    win = XCreateSimpleWindow(x_display, root, 0, 0, 640, 480, 1,
                              BlackPixel(x_display, 0), WhitePixel(x_display, 0));
    XSelectInput(x_display, win, ExposureMask | KeyPressMask);
    XMapWindow(x_display, win);

    XEvent event;
    while (1) {
        XNextEvent(x_display, &event);
        if (event.type == KeyPress) {
            break;  // Exit on any key press
        }
    }

    XDestroyWindow(x_display, win);
    XCloseDisplay(x_display);
}

void draw_char(char c, float x, float y, float scale) {
 
    int col = c % 16;
    int row = c / 16;
    float tx = col / 16.0f;
    float ty = row / 16.0f;
    float tw = 1.0f / 16.0f;
    float th = 1.0f / 16.0f;

    float w = scale;
    float h = scale;

    GLfloat vertices[] = {
        x,     y,     tx,     ty,
        x + w, y,     tx + tw,ty,
        x,     y + h, tx,     ty + th,
        x + w, y + h, tx + tw,ty + th
    };

    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // texcoord

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

void window_subsystem_init(void) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    // Initialize subsystem using the framework macros
    SUBSYSTEM_INIT(&window_dispatch_subsystem, "WindowManager");
    REGISTER_CORE_SUBSYSTEM(&window_dispatch_subsystem, 2);  // Assign to core 2
    register_manager("window_manager", window_manager_handler);
    dispatch_logf("window_subsystem initialized → Core %d", window_dispatch_subsystem.core_affinity);

    // Note: Actual window creation is now handled by mesa_bootstrap
    // This subsystem handles window management and events
}

// Handles incoming messages for the window manager
void window_manager_handler(message_t* msg) {
    if (!msg) {
        dispatch_log_with_core("window_manager_handler: NULL message!", window_dispatch_subsystem.core_affinity);
        return;
    }

    const int core = window_dispatch_subsystem.core_affinity;
    dispatch_log_with_core("Window manager handling message", core);

    switch (msg->type) {
        case MSG_WINDOW_CREATE:
            dispatch_log_with_core("Window manager: Create window request", core);
            // Window creation logic here
            break;

        case MSG_WINDOW_DESTROY:
            dispatch_log_with_core("Window manager: Destroy window request", core);
            // Window destruction logic here
            break;

        case MSG_WINDOW_RESIZE:
            dispatch_log_with_core("Window manager: Resize window request", core);
            // Window resize logic here
            break;

        default:
            dispatch_log_with_core("Window manager received unknown message type", core);
            break;
    }
}

// Main tick function for window manager
void window_manager_tick(void) {
    const int core = window_dispatch_subsystem.core_affinity;
    dispatch_log_with_core("▶ window_manager_tick() start", core);

    message_queue_t* inbox = get_manager_inbox("window_manager");
    if (!inbox) {
        dispatch_log_with_core("⚠️ No inbox found for window_manager", core);
        window_dispatch_subsystem.last_error_code = -1;
        return;
    }

    int processed = 0;
    while (inbox->head != inbox->tail && processed < MAX_QUEUE_SIZE) {
        message_t* msg = &inbox->messages[inbox->head];
        window_manager_handler(msg);
        inbox->head = (inbox->head + 1) % MAX_QUEUE_SIZE;
        processed++;
    }

    if (processed > 0) {
        dispatch_logf("window_manager_tick: Processed %d message%s", processed, processed == 1 ? "" : "s");
        window_dispatch_subsystem.metrics.messages_processed += processed;
        time(&window_dispatch_subsystem.last_run);
        if (window_dispatch_subsystem.history_count < MAX_HISTORY) {
            window_dispatch_subsystem.history[window_dispatch_subsystem.history_count++] = window_dispatch_subsystem.last_run;
        }
    }

    window_dispatch_subsystem.last_error_code = 0;
    dispatch_log_with_core("⏹ window_manager_tick() end", core);
}

void window_subsystem_init_once(void) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    create_main_window();  // ← Fullscreen, persistent
}

void window_subsystem_tick(void) {
    if (!x_display || !win) return;

    while (XPending(x_display)) {
        XEvent event;
        XNextEvent(x_display, &event);

        if (event.type == ButtonPress) {
            if (mouse_in_shutdown_region(event.xbutton.x, event.xbutton.y)) {
                dispatch_log("Shutdown button clicked");
                // Create proper message_t structure for shutdown request
                message_t shutdown_msg = {
                    .origin = "WindowManager",
                    .target = "Shutdown",
                    .type = MSG_SHUTDOWN_REQUEST,
                    .payload = "user_requested",
                    .timestamp = 0,  // Could use real timestamp if needed
                    .priority = 5    // High priority for shutdown
                };
                enqueue_message("Shutdown", &shutdown_msg);
            }
        }

        if (event.type == KeyPress) {
            dispatch_log("Key pressed in window");
        }
    }
}

void create_main_window(void) {
    x_display = XOpenDisplay(NULL);
    if (!x_display) {
        fprintf(stderr, "X11: Failed to open display\n");
        exit(1);
    }

    Window root = DefaultRootWindow(x_display);
    Screen* screen = DefaultScreenOfDisplay(x_display);
    int width = screen->width;
    int height = screen->height;

    XSetWindowAttributes attrs;
    attrs.override_redirect = True;

    win = XCreateWindow(x_display, root, 0, 0, width, height, 0,
                        CopyFromParent, InputOutput, CopyFromParent,
                        CWOverrideRedirect, &attrs);

    XSelectInput(x_display, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(x_display, win);
}

bool mouse_in_shutdown_region(int x, int y) {
    // Example: bottom-right corner of a 640x480 window
    int button_x = 540;
    int button_y = 420;
    int button_width = 100;
    int button_height = 40;

    return (x >= button_x && x <= button_x + button_width &&
            y >= button_y && y <= button_y + button_height);
}