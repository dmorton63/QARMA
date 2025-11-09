// cpu_core_manager.c
#include "cpu_core_manager.h"
#include "subsystem_registry.h" // For DispatchSubsystem and get_subsystem()
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>   // For sysconf
#include "macros.h"
#include "priority_scheduler.h"

//#define MAX_SUBSYSTEMS 8

#define MAX_QUEUE 16

//#define MAX_HISTORY 8

typedef struct {
    const char* name;
    int dispatch_count;
    subsystem_fn fn;
    int core_affinity;
    int last_error_code;
    time_t last_run;
    time_t history[MAX_HISTORY];
    int history_count;
} Subsystem;


static Subsystem subsystems[MAX_SUBSYSTEMS];
static Subsystem* dispatch_queue[MAX_QUEUE];
static int count = 0;
static int queue_size = 0;

static long core_count = MAX_CORES;

dispatch_fn dispatch_table[MAX_CORES];

static int dispatch_metrics[MAX_CORES] = {0};

#define MAX_LOG 64

typedef struct {
    char name[32];
    int core_id;
    time_t timestamp;
} DispatchEntry;

static DispatchEntry dispatch_entries[MAX_LOG];
static int log_count = 0;



static cpu_core_manager_t manager;

void core_manager_init(void)
{
    core_count = sysconf(_SC_NPROCESSORS_ONLN);
    #ifdef DEBUG
        LOG("Detected %ld CPU cores\n", core_count);
    #endif
}

cpu_core_manager_t* get_cpu_core_manager(void) {
    manager.register_subsystem = register_subsystem;
    manager.dispatch = dispatch_by_priority;
    manager.set_affinity = set_affinity; // stub for now
//    dispatch_to_affinity();
    manager.debug = debug;               // stub for now
    return &manager;
}


//static dispatch_fn dispatch_table[MAX_CORES];


void load_dispatch_table(void) {
    for (int i = 0; i < MAX_CORES; ++i) {
        dispatch_table[i] = core_dispatch;
    }
}

void default_dispatch(DispatchSubsystem* s){
    int core_id = s->core_affinity;
    dispatch_logf("Default dispatch for subsystem '%s' on core %d", s->name, core_id);
    if (s->fn) s->fn();
}

void init_dispatch_table(void) {
    for (int i = 0; i < MAX_CORES; ++i) {
        dispatch_table[i] = default_dispatch;
    }
    #ifdef DEBUG
    LOG("Dispatch table initialized with default handlers.\n");
    #endif
}

void register_core(int core_id) {
    if (core_id >= 0 && core_id < MAX_CORES) {
        #ifdef DEBUG
        LOG("Core %d registered.\n", core_id);
        #endif
    } else {
        #ifdef DEBUG
        LOG("Invalid core ID: %d\n", core_id);
        #endif
    }
}

void dispatch_to_core(int core_id) {
    if (core_id >= 0 && core_id < MAX_CORES && dispatch_table[core_id]) {
        dispatch_table[core_id](get_subsystem_by_core(core_id));
    } else {
        #ifdef DEBUG
        LOG("No dispatch handler for core %d\n", core_id);
        #endif
    }
}

void dispatch_subsystems(void) {
    for (int i = 0; i < count; ++i) {
        #ifdef DEBUG
        LOG("Dispatching subsystem: %s\n", subsystems[i].name);
        #endif
        subsystems[i].fn();
    }
}

void dispatch_all_subsystems(void) {
    for (int i = 0; i < count; ++i) {
        Subsystem* s = &subsystems[i];
        dispatch_logf("Dispatching subsystem '%s' → Core %d", s->name, s->core_affinity);

        if (s->core_affinity >= 0 && s->core_affinity < MAX_CORES && dispatch_table[s->core_affinity]) {
            dispatch_table[s->core_affinity](get_subsystem_by_core(s->core_affinity));
        } else {
            s->fn(); // fallback direct dispatch
            dispatch_logf("Subsystem '%s' dispatched directly (no core handler)", s->name);
        }

        s->last_run = time(NULL);
        s->dispatch_count++;
        PUSH_SUBSYSTEM_HISTORY(s, s->last_run);
        INCREMENT_SUBSYSTEM_METRIC(s, 1);
    }
}


DispatchSubsystem* get_subsystem_by_core(int core_id) {
    for (int i = 0; i < registry_count; ++i) {
        if (registry[i]->core_affinity == core_id) {
            return registry[i];
        }
    }
    return NULL;
}


void register_subsystem(const char* name, subsystem_fn fn) {
    if (count < MAX_SUBSYSTEMS) {
        Subsystem* s = &subsystems[count];
        s->name = name;
        s->fn = fn;
        s->core_affinity = -1;
        s->last_error_code = 0;
        s->last_run = 0;
        s->history_count = 0; // 🧠 Initialize scroll

            #ifdef DEBUG    
            LOG("Subsystem '%s' registered.\n", name);
            #endif
            count++;
        } else {
            #ifdef DEBUG
               LOG("Subsystem limit reached. Cannot register '%s'.\n", name);
            #endif
        }
    }

void test_subsystem(void) {
    #ifdef DEBUG
    LOG("Test subsystem activated.\n");
    #endif
}

void set_affinity(const char* name, int core_id) {
      if (core_id < 0 || core_id >= MAX_CORES) {
        #ifdef DEBUG
            LOG("Invalid core ID: %d\n", core_id);
        #endif
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (strcmp(subsystems[i].name, name) == 0) {
            subsystems[i].core_affinity = core_id;
            #ifdef DEBUG    
            LOG("Subsystem '%s' bound to core %d.\n", name, core_id);
            #endif
            return;
        }
    }

    #ifdef DEBUG
    LOG("Subsystem '%s' not found. Affinity not set.\n", name);
    #endif
}

void debug(void) {
    #ifdef DEBUG
    LOG("=== Subsystem Debug ===\n");
    #endif
    if (count == 0) {
        #ifdef DEBUG
        LOG("No subsystems registered.\n");
        #endif
        return;
    }

    for (int i = 0; i < count; ++i) {
        #ifdef DEBUG
            LOG("Subsystem %d:\n", i);
            LOG("  Name         : %s\n", subsystems[i].name);
            LOG("  Entry Point  : %p\n", (void*)subsystems[i].fn);
            LOG("  Core Affinity: %d\n", subsystems[i].core_affinity);
            LOG("  Last Error   : %d\n", subsystems[i].last_error_code);
            LOG("  Last Run     : %s", ctime(&subsystems[i].last_run));
        #endif
    }

    #ifdef DEBUG
    LOG("=======================\n");
    #endif
}


void dispatch_to_affinity(void) {
    #ifdef DEBUG
    LOG("=== Dispatching to Affinity ===\n");
    #endif

    for (int i = 0; i < count; ++i) {
        Subsystem* s = &subsystems[i];
        int core_id = s->core_affinity;

        // Timestamp and history
        time(&s->last_run);
        if (s->history_count < MAX_HISTORY) {
            s->history[s->history_count++] = s->last_run;
            #ifdef DEBUG
            LOG("Logged dispatch for '%s' at %s", s->name, ctime(&s->last_run));
            #endif
        }

        // Dispatch logic
        if (core_id >= 0 && core_id < MAX_CORES) {
            dispatch_fn handler = dispatch_table[core_id] ? dispatch_table[core_id] : default_dispatch;
            #ifdef DEBUG
            LOG("Routing '%s' to core %d...\n", s->name, core_id);
            #endif
            dispatch_metrics[core_id]++;
            handler((DispatchSubsystem*)s);  // Cast if needed
            s->last_error_code = 0;
        } else {
            #ifdef DEBUG
            LOG("Routing '%s' to fallback...\n", s->name);
            #endif
            fallback_dispatch((DispatchSubsystem*)s);  // Cast if needed
            s->last_error_code = -1;
        }

        // Optional: avoid double invocation if handler already calls s->fn()
        if (s->fn) {
            s->fn();
        }
    }
    #ifdef DEBUG
    LOG("===============================\n");
    #endif
}

void fallback_dispatch(DispatchSubsystem* s) {
    #ifdef DEBUG
    LOG("Fallback dispatch for unassigned subsystem '%s'.\n", s->name);
    #endif
}

void enqueue_subsystem(const char* name) {
    if (queue_size >= MAX_QUEUE) {
        #ifdef DEBUG
        LOG("Dispatch queue full. Cannot enqueue '%s'.\n", name);
        #endif
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (strcmp(subsystems[i].name, name) == 0) {
            dispatch_queue[queue_size++] = &subsystems[i];
            #ifdef DEBUG
            LOG("Subsystem '%s' enqueued for dispatch.\n", name);
            #endif
            return;
        }
    }
    #ifdef DEBUG
    LOG("Subsystem '%s' not found. Cannot enqueue.\n", name);
    #endif
}

void dispatch_queue_run(void) {
    #ifdef DEBUG
    LOG("=== Dispatching Queue ===\n");
    #endif

    for (int i = 0; i < queue_size; ++i) {
        Subsystem* s = dispatch_queue[i];
        int core_id = s->core_affinity;

        // Choose handler
        dispatch_fn handler = (core_id >= 0 && core_id < MAX_CORES && dispatch_table[core_id])
            ? dispatch_table[core_id]
            : fallback_dispatch;

        // Dispatch
        #ifdef DEBUG
        LOG("Routing '%s' to %s...\n", s->name,
               (core_id >= 0 && core_id < MAX_CORES) ? "assigned core" : "fallback");
        #endif
        dispatch_metrics[core_id >= 0 && core_id < MAX_CORES ? core_id : MAX_CORES - 1]++;
        handler((DispatchSubsystem*)s);  // Cast if needed
        s->last_error_code = (core_id >= 0 && core_id < MAX_CORES) ? 0 : -1;

        // Optional: avoid double invocation if handler already calls s->fn()
        if (s->fn) {
            s->fn();
        }

        // Timestamp and history
        time(&s->last_run);
        if (s->history_count < MAX_HISTORY) {
            s->history[s->history_count++] = s->last_run;
        }
    }

    queue_size = 0;
    #ifdef DEBUG
    LOG("=========================\n");
    #endif
}


void subsystem_report(void) {  
    #ifdef DEBUG
    LOG("=== Subsystem Report ===\n");
    LOG("Total Subsystems: %d\n", count);

    for (int i = 0; i < count; ++i) {
        Subsystem* s = &subsystems[i];
        LOG("Subsystem %d:\n", i);
        LOG("  Name         : %s\n", s->name);
        LOG("  Core Affinity: %d\n", s->core_affinity);
        LOG("  Last Run     : %s", ctime(&s->last_run));
        LOG("  Last Error   : %d\n", s->last_error_code);

        // 🧠 Dispatch history
        LOG("  Dispatch History:\n");
        for (int h = 0; h < s->history_count; ++h) {
            LOG("    [%d] %s", h, ctime(&s->history[h]));
        }
    }

    // Core metrics
    LOG("\n=== Core Dispatch Metrics ===\n");
    for (int i = 0; i < MAX_CORES; ++i) {
        LOG("Core %d dispatched %d time%s.\n", i, dispatch_metrics[i], dispatch_metrics[i] == 1 ? "" : "s");
    }

    LOG("=============================\n");
    #endif
}

void dispatch_metrics_reset(void) {
    for (int i = 0; i < MAX_CORES; ++i) {
        dispatch_metrics[i] = 0;
    }
    #ifdef DEBUG
    LOG("Dispatch metrics reset.\n");
    #endif
}

int most_used_core(void) {
    int max = 0;
    for (int i = 1; i < MAX_CORES; ++i) {
        if (dispatch_metrics[i] > dispatch_metrics[max]) {
            max = i;
        }
    }
    return max;
}

void subsystem_unbind(const char* name) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(subsystems[i].name, name) == 0) {
            subsystems[i].core_affinity = -1;
            #ifdef DEBUG
            LOG("Subsystem '%s' unbound from core.\n", name);
            #endif
            return;
        }
    }
    #ifdef DEBUG
    LOG("Subsystem '%s' not found.\n", name);
    #endif
}

void dispatch_log_report(void) {
    #ifdef DEBUG
    LOG("=== Dispatch Log ===\n");
    for (int i = 0; i < log_count; ++i) {
        LOG("[%d] %s → Core %d at %s", i,
               dispatch_entries[i].name,
               dispatch_entries[i].core_id,
               ctime(&dispatch_entries[i].timestamp));
    }
    LOG("====================\n");
    #endif
}

void test_dispatch_table(int core_id)
{
    #ifdef DEBUG
    LOG("Testing dispatch table for core %d\n", core_id);
    #endif
    if (dispatch_table[core_id]) {
        #ifdef DEBUG
        LOG("Dispatch handler found for core %d\n", core_id);
        #endif
        dispatch_table[core_id](get_subsystem_by_core(core_id));
    } else {
    #ifdef DEBUG
        LOG("No dispatch handler found for core %d\n", core_id);
    #endif 
    }
}

void dispatch_log(const char* message) {
    dispatch_log_with_core(message, -1); // Default to generic log entry
}

void dispatch_log_with_core(const char* message, int core_id) {
    if (log_count < MAX_LOG) {
        strncpy(dispatch_entries[log_count].name, message, sizeof(dispatch_entries[log_count].name) - 1);
        dispatch_entries[log_count].name[sizeof(dispatch_entries[log_count].name) - 1] = '\0';
        dispatch_entries[log_count].core_id = core_id;
        time(&dispatch_entries[log_count].timestamp);
        log_count++;
    }
    if (core_id >= 0) {
        
        LOG("[LOG] %s → Core %d\n", message, core_id);
        
    } else {
        
        LOG("[LOG] %s\n", message);
    }
}

void dispatch_metrics_increment(const char* name, int value) {
    // First check old subsystems array
    for (int i = 0; i < count; ++i) {
        if (strcmp(subsystems[i].name, name) == 0) {
            if (subsystems[i].core_affinity >= 0 && subsystems[i].core_affinity < MAX_CORES) {
                dispatch_metrics[subsystems[i].core_affinity] += value;
            }
            return;
        }
    }
    
    // Then check new registry system
    extern DispatchSubsystem* get_subsystem(const char* name);
    DispatchSubsystem* ds = get_subsystem(name);
    if (ds != NULL) {
        if (ds->core_affinity >= 0 && ds->core_affinity < MAX_CORES) {
            dispatch_metrics[ds->core_affinity] += value;
        }
        return;
    }    
    #ifdef DEBUG
    LOG("Metrics increment: Subsystem '%s' not found in either registry.\n", name);
    #endif

}

void subsystem_history_push(const char* name, const char* data) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(subsystems[i].name, name) == 0) {
            Subsystem* s = &subsystems[i];
            if (s->history_count < MAX_HISTORY) {
                time(&s->history[s->history_count++]);
                #ifdef DEBUG
                LOG("History push for '%s': %s\n", name, data);
                #endif
            }
            return;
        }
    }
    #ifdef DEBUG
    LOG("History push: Subsystem '%s' not found.\n", name);
    #endif
}

void core_dispatch(DispatchSubsystem* s) {
    dispatch_logf("Core %d dispatching subsystem '%s'", s->core_affinity, s->name);
    if (s->fn) s->fn();
}