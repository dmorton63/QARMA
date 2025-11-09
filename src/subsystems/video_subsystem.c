#include "video_subsystem.h"
#include "framebuffer.h"         // For fb_write_string or direct framebuffer access
#include "message_manager.h"     // For message_t, register_manager, get_manager_inbox
#include "cpu_core_manager.h"    // For dispatch_log, dispatch_metrics_increment
#include "subsystem_registry.h"  // For DispatchSubsystem and macros
#include "dispatch_logf.h"       // For dispatch_logf()
#include <stdlib.h>              // For free()
#include <stdio.h>               // For printf (debugging)
#include <stdarg.h>
#include <time.h>
#include <X11/Xlib.h>            // For Display, Screen
#include "mesa_bootstrap.h"    // For mesa_bootstrap_init/shutdown()

// Declare the full subsystem data structure
DispatchSubsystem video_dispatch_subsystem = {
    .name = "Video",
    .fn = video_manager_tick,
    .core_affinity = -1,
    .dispatch_count = 0,
    .last_error_code = 0,
    .last_run = 0,
    .history_count = 0,
    .priority = PRIORITY_NORMAL
};

// Framebuffer memory and cursor state
static uint16_t* video_memory = (uint16_t*)0xB8000;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

// Clears the screen and resets cursor
void video_subsystem_clear(void) {
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i] = (uint8_t)' ' | (0x07 << 8);  // white on black
    }
    cursor_x = 0;
    cursor_y = 0;
}

// Writes a single character to the framebuffer
void video_subsystem_put_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        int pos = cursor_y * 80 + cursor_x;
        video_memory[pos] = (uint8_t)c | (0x07 << 8);
        cursor_x++;
        if (cursor_x >= 80) {
            cursor_x = 0;
            cursor_y++;
        }
    }
}

// Writes a string to the framebuffer
void video_subsystem_write(const char* str) {
    while (*str) {
        video_subsystem_put_char(*str++);
    }
}

// Optional direct invocation for diagnostics
void video_subsystem(void) {
    dispatch_log_with_core("Video subsystem invoked", video_dispatch_subsystem.core_affinity);
    INCREMENT_SUBSYSTEM_METRIC(&video_dispatch_subsystem, 1);
}

// Handles incoming messages for the video manager
void video_manager_handler(message_t* msg) {
    if (!msg) {
        dispatch_log_with_core("video_manager_handler: NULL message!", video_dispatch_subsystem.core_affinity);
        return;
    }
    if (!msg->payload) {
        dispatch_log_with_core("video_manager_handler: NULL payload!", video_dispatch_subsystem.core_affinity);
        return;
    }

    dispatch_log_with_core("Video manager handling message", video_dispatch_subsystem.core_affinity);

    switch (msg->type) {
        case MSG_RENDER_REQUEST:
            fb_write_string(msg->payload);  // Or use video_subsystem_write()
            mesa_add_text_message(msg->payload);  // Also add to OpenGL window
            dispatch_log_with_core("Video manager rendered message", video_dispatch_subsystem.core_affinity);
            break;

        case MSG_INPUT_EVENT: {
            dispatch_log_with_core("Video manager processing input event", video_dispatch_subsystem.core_affinity);
            // video_subsystem_write((char*)msg->payload);
            
            // Add input character to OpenGL window
            char input_msg[32];
            snprintf(input_msg, sizeof(input_msg), "Input: %c", *(char*)msg->payload);
            mesa_add_text_message(input_msg);
            
            LOG("Video output: %c\n", *(char*)msg->payload);  // Safer debug output
            dispatch_log_with_core("Video manager echoed input glyph", video_dispatch_subsystem.core_affinity);
            free((void*)msg->payload);  // Clean up - cast away const

            break;
        }

        default:
            dispatch_log_with_core("Video manager received unknown message type", video_dispatch_subsystem.core_affinity);
            break;
    }
}
        static bool window_initialized = false;
// Initializes the video manager and registers its handler
void video_subsystem_init(void) {
    // Only initialize mesa bootstrap once, with fullscreen dimensions
    if (!window_initialized) {
        // Get screen dimensions for fullscreen
        Display* temp_display = XOpenDisplay(NULL);
        Screen* screen = DefaultScreenOfDisplay(temp_display);
        int screen_width = screen->width;
        int screen_height = screen->height;
        printf("Detected screen dimensions: %dx%d\n", screen_width, screen_height);
        XCloseDisplay(temp_display);
        
        // Create fullscreen OpenGL window
        mesa_bootstrap_init(0, 0, screen_width, screen_height);
        window_initialized = true;
        
        // Note: Desktop messages will be added after splash screen is dismissed
        // This allows the splash screen to be shown first
    }
    
    SUBSYSTEM_INIT(&video_dispatch_subsystem, "Video");
    REGISTER_CORE_SUBSYSTEM(&video_dispatch_subsystem, 3);
    register_manager("video_manager", video_manager_handler);
    dispatch_logf("video_subsystem initialized → Core %d", video_dispatch_subsystem.core_affinity);
}

// Processes messages from the video manager's inbox
void video_manager_tick(void) {
    mesa_bootstrap_render();
    const int core = video_dispatch_subsystem.core_affinity;
    dispatch_log_with_core("▶ video_manager_tick() start", core);

    message_queue_t* inbox = get_manager_inbox("video_manager");
    if (!inbox) {
        dispatch_log_with_core("⚠️ No inbox found for video_manager", core);
        video_dispatch_subsystem.last_error_code = -1;
        return;
    }

    int processed = 0;
    while (inbox->head != inbox->tail && processed < MAX_QUEUE_SIZE) {
        message_t* msg = &inbox->messages[inbox->head];
        video_manager_handler(msg);
        inbox->head = (inbox->head + 1) % MAX_QUEUE_SIZE;
        processed++;
    }

    if (processed > 0) {
        dispatch_logf("video_manager_tick: Processed %d message%s", processed, processed == 1 ? "" : "s");
        video_dispatch_subsystem.metrics.messages_processed += processed;
        time(&video_dispatch_subsystem.last_run);
        if (video_dispatch_subsystem.history_count < MAX_HISTORY) {
            video_dispatch_subsystem.history[video_dispatch_subsystem.history_count++] = video_dispatch_subsystem.last_run;
        }
    }

    video_dispatch_subsystem.last_error_code = 0;
    dispatch_log_with_core("⏹ video_manager_tick() end", core);
}



void video_manager_shutdown(void) {
    mesa_bootstrap_shutdown();
}
