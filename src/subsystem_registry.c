#include "subsystem_registry.h"
#include <string.h>
#include <stdio.h>

#define MAX_SUBSYSTEMS 32

DispatchSubsystem* registry[MAX_SUBSYSTEMS];
int registry_count = 0;

// Registers a subsystem pointer into the registry
void register_with_message_manager(DispatchSubsystem* subsystem) {
    if (registry_count >= MAX_SUBSYSTEMS) {
        dispatch_logf("⚠️ Subsystem registry full. Cannot register '%s'", subsystem->name);
        return;
    }

    // Prevent duplicate names
    for (int i = 0; i < registry_count; ++i) {
        if (strcmp(registry[i]->name, subsystem->name) == 0) {
            dispatch_logf("⚠️ Subsystem '%s' already registered", subsystem->name);
            return;
        }
    }

    registry[registry_count++] = subsystem;
    dispatch_logf("✅ Subsystem '%s' registered in message manager", subsystem->name);
}

// Registers subsystem with core manager (placeholder)
void register_with_cpu_core_manager(DispatchSubsystem* subsystem) {
    // You can expand this to bind to dispatch_table or affinity map
    dispatch_logf("🔗 Subsystem '%s' bound to core %d", subsystem->name, subsystem->core_affinity);
}

// Returns pointer to a subsystem by name
DispatchSubsystem* get_subsystem(const char* name) {
    for (int i = 0; i < registry_count; ++i) {
        if (strcmp(registry[i]->name, name) == 0) {
            return registry[i];
        }
    }
    return NULL;
}

// Prints a diagnostic report of all registered subsystems
void registry_subsystem_report(void) {
    #ifdef DEBUG
    LOG("=== Subsystem Report ===\n");
    LOG("Total Subsystems: %d\n", registry_count);

    for (int i = 0; i < registry_count; ++i) {
        DispatchSubsystem* s = registry[i];
        LOG("Subsystem %d:\n", i);
        LOG("  Name         : %s\n", s->name);
        LOG("  Core Affinity: %d\n", s->core_affinity);
        LOG("  Last Run     : %s", ctime(&s->last_run));
        LOG("  Last Error   : %d\n", s->last_error_code);
        LOG("  Dispatch History:\n");

        for (int j = 0; j < s->history_count; ++j) {
            LOG("    [%d] %s", j, ctime(&s->history[j]));
        }
    }
    LOG("=========================\n");
    #endif
}
