#include "priority_scheduler.h"
#include "subsystem_registry.h"
#include "dispatch_logf.h"
#include <stdint.h>
#include <stdlib.h>
#include "cpu_core_manager.h"
#include "qtime.h"

#define MAX_SUBSYSTEMS 32

extern DispatchSubsystem* registry[MAX_SUBSYSTEMS];
extern int registry_count;

// Internal comparator for sorting by priority (lower = higher priority)
static int compare_priority(const void* a, const void* b) {
    DispatchSubsystem* sa = *(DispatchSubsystem**)a;
    DispatchSubsystem* sb = *(DispatchSubsystem**)b;
    return (int)sa->priority - (int)sb->priority;
}

// Dispatches all subsystems in priority order
void dispatch_by_priority(void) {
    if (registry_count == 0) return;

    // Create a temporary array of pointers
    DispatchSubsystem* sorted[MAX_SUBSYSTEMS];
    for (int i = 0; i < registry_count; ++i) {
        sorted[i] = registry[i];
    }

    // Sort by priority
    qsort(sorted, registry_count, sizeof(DispatchSubsystem*), compare_priority);

    // Dispatch each subsystem
    for (int i = 0; i < registry_count; ++i) {
        DispatchSubsystem* s = sorted[i];
        dispatch_logf("Dispatching '%s' → Core %d (Priority %d)", s->name, s->core_affinity, s->priority);

        if (s->core_affinity >= 0 && dispatch_table[s->core_affinity]) {
            dispatch_table[s->core_affinity](s);
        } else {
            s->fn();  // fallback
            dispatch_logf("Direct dispatch of '%s' (no core handler)", s->name);
        }

        s->last_run = get_system_time();
        PUSH_SUBSYSTEM_HISTORY(s, s->last_run);
        INCREMENT_SUBSYSTEM_METRIC(s, 1);
    }
}


void set_subsystem_priority(const char* name, uint8_t priority) {
    DispatchSubsystem* s = get_subsystem(name);
    if (s) {
        s->priority = priority;
        dispatch_logf("Set priority of '%s' to %d", name, priority);
    } else {
        dispatch_logf("⚠️ Subsystem '%s' not found for priority assignment", name);
    }
}

