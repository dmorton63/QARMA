#include "keyboard_subsystem.h"
#include "cpu_core_manager.h"
#include "message_manager.h"
#include "subsystem_registry.h" 
#include "dispatch_logf.h"        // For dispatch_logf()
#include "qtime.h"                // For get_system_time()
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>                 // For time()

// Simulated input buffer
static const char* test_inputs[] = {
    "hello", "world", "mythic", "dispatch", "glyph", "unbind"
};


static int input_index = 0;
static bool exhausted = false;

// Declare the full subsystem data structure
DispatchSubsystem keyboard_dispatch_subsystem = {
    .name = "Keyboard",
    .fn = keyboard_subsystem_tick,
    .core_affinity = -1,
    .dispatch_count = 0,
    .last_error_code = 0,
    .last_run = 0,
    .history_count = 0,
    .priority = PRIORITY_HIGH
};

// Initializes the subsystem and registers the manager

void keyboard_subsystem_init(void) {
    exhausted = false;
    input_index = 0;
    SUBSYSTEM_INIT(&keyboard_dispatch_subsystem, "Keyboard");
    REGISTER_CORE_SUBSYSTEM(&keyboard_dispatch_subsystem, 2);
    register_manager("keyboard_manager", keyboard_manager_handler);
    dispatch_logf("keyboard_subsystem initialized → Core %d", keyboard_dispatch_subsystem.core_affinity);
}



// Periodic tick: dispatches simulated input
void keyboard_subsystem_tick(void) {
    #ifdef DEBUG
     LOG("Keyboard subsystem tick: processing input index %d, exhausted = %d\n", input_index, exhausted);
    #endif
    if (input_index >= (int)(sizeof(test_inputs) / sizeof(test_inputs[0]))) {
        if (!exhausted) {
            dispatch_log_with_core("keyboard_subsystem exhausted input stream", keyboard_dispatch_subsystem.core_affinity);
            exhausted = true;
            subsystem_unbind("Keyboard");
        }
        return;
    }
    const char* input = test_inputs[input_index++];
    dispatch_log_with_core("keyboard_subsystem dispatching input", keyboard_dispatch_subsystem.core_affinity);
    PUSH_SUBSYSTEM_HISTORY(&keyboard_dispatch_subsystem, time(NULL));

    char glyph[2] = {'\0', '\0'};
    for (size_t i = 0; i < strlen(input); i++) {
        glyph[0] = input[i];

        message_t msg = {
            .origin = "keyboard_manager",
            .target = "video_manager",
            .type = MSG_INPUT_EVENT,
            .payload = strdup(glyph),
            .timestamp = get_system_time(),
            .priority = 1
        };

        if (!enqueue_message(msg.target, &msg)) {
            dispatch_log_with_core("keyboard_subsystem failed to enqueue message", keyboard_dispatch_subsystem.core_affinity);
        } else {
            dispatch_log_with_core("keyboard_subsystem enqueued input glyph", keyboard_dispatch_subsystem.core_affinity);
        }
    }

    keyboard_dispatch_subsystem.last_run = get_system_time();
    INCREMENT_SUBSYSTEM_METRIC(&keyboard_dispatch_subsystem, 1);
}

// Optional direct invocation
void keyboard_subsystem(void) {
    dispatch_log_with_core("Keyboard subsystem invoked", keyboard_dispatch_subsystem.core_affinity);
    INCREMENT_SUBSYSTEM_METRIC(&keyboard_dispatch_subsystem, 1);
}

// Handles incoming messages for keyboard_manager
void keyboard_manager_handler(message_t* msg) {
    if (msg->type == MSG_INPUT_EVENT && msg->payload) {
        dispatch_log_with_core("Keyboard manager received input event", keyboard_dispatch_subsystem.core_affinity);
        // Optional: route or process input
    }
}

bool keyboard_input_exhausted(void) {
    return exhausted;
}