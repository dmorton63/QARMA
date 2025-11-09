
#ifndef SUBSYSTEM_REGISTRY_H
#define SUBSYSTEM_REGISTRY_H

#include <time.h>
#include <stdint.h>
#include "log.h"

#define PRIORITY_REALTIME  0
#define PRIORITY_HIGH      32
#define PRIORITY_NORMAL    128
#define PRIORITY_LOW       224
#define PRIORITY_IDLE      255

// Constants
#define MAX_HISTORY 16

// Function pointer type for subsystem ticks
typedef void (*subsystem_fn)(void);

// Full subsystem glyph
typedef struct DispatchSubsystem {
    const char* name;
    int dispatch_count;
    subsystem_fn fn;
    int core_affinity;
    int last_error_code;
    time_t last_run;
    time_t history[MAX_HISTORY];
    int history_count;
    uint8_t priority;
    struct {
        int messages_processed;
        int ticks_executed;
        int glyphs_rendered;
        int dispatch_failures;
        int max_tick_duration_ms;
        int avg_tick_duration_ms;
    } metrics;
} DispatchSubsystem;

#define MAX_SUBSYSTEMS 32

extern DispatchSubsystem* registry[MAX_SUBSYSTEMS];
extern int registry_count;

// Macro: Initialize subsystem
#define SUBSYSTEM_INIT(subsystem_ptr, name_str)                      \
    do {                                                             \
        (subsystem_ptr)->name = (name_str);                          \
        (subsystem_ptr)->last_error_code = 0;                        \
        (subsystem_ptr)->last_run = time(NULL);                      \
        (subsystem_ptr)->dispatch_count = 0;                         \
        (subsystem_ptr)->history_count = 0;                          \
        for (int i = 0; i < MAX_HISTORY; ++i)                        \
            (subsystem_ptr)->history[i] = 0;                         \
        dispatch_logf("Initialized subsystem '%s'", (name_str));     \
    } while (0)

// Macro: Register core-bound subsystem
#define REGISTER_CORE_SUBSYSTEM(subsystem_ptr, core_id)             \
    do {                                                             \
        (subsystem_ptr)->core_affinity = (core_id);                  \
        register_with_message_manager(subsystem_ptr);                \
        register_with_cpu_core_manager(subsystem_ptr);               \
        dispatch_logf("Registered core subsystem '%s' on core %d",   \
                      (subsystem_ptr)->name, (core_id));             \
    } while (0)

// Macro: Register message-only subsystem
#define REGISTER_MESSAGE_ONLY_SUBSYSTEM(subsystem_ptr)              \
    do {                                                             \
        (subsystem_ptr)->core_affinity = -1;                         \
        register_with_message_manager(subsystem_ptr);                \
        dispatch_logf("Registered message-only subsystem '%s'",      \
                      (subsystem_ptr)->name);                        \
    } while (0)

// Macro: Push dispatch history
#define PUSH_SUBSYSTEM_HISTORY(subsystem_ptr, timestamp)            \
    do {                                                             \
        if ((subsystem_ptr)->history_count < MAX_HISTORY) {          \
            (subsystem_ptr)->history[(subsystem_ptr)->history_count++] = (timestamp); \
        }                                                            \
    } while (0)

// Macro: Increment subsystem metric
#define INCREMENT_SUBSYSTEM_METRIC(subsystem_ptr, value)            \
    do {                                                             \
        dispatch_metrics_increment((subsystem_ptr)->name, (value));  \
    } while (0)

// External functions (you'll define these elsewhere)
void register_with_message_manager(DispatchSubsystem* subsystem);
void register_with_cpu_core_manager(DispatchSubsystem* subsystem);
DispatchSubsystem *get_subsystem(const char *name);
void registry_subsystem_report(void);
void dispatch_logf(const char* format, ...);
void dispatch_metrics_increment(const char* name, int value);

#endif // SUBSYSTEM_REGISTRY_H