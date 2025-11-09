#ifndef CPU_CORE_MANAGER_H
#define CPU_CORE_MANAGER_H

#include <time.h>
#include "subsystem_registry.h"

typedef struct {
    void (*register_subsystem)(const char* name, void (*entry_point)(void));
    void (*dispatch)(void);
    void (*set_affinity)(const char* name, int core_id);
    void (*debug)(void);
} cpu_core_manager_t;

#define MAX_CORES 32
//struct DispatchSubsystem; // Forward declaration

typedef void (*dispatch_fn)(DispatchSubsystem* s);
extern dispatch_fn dispatch_table[MAX_CORES];
//typedef void (*dispatch_fn)(int core_id);

void core_dispatch(DispatchSubsystem* s);

void core_manager_init(void);

cpu_core_manager_t *get_cpu_core_manager(void);



typedef void (*subsystem_fn)(void);


void register_core(int core_id);
void load_dispatch_table(void);
void default_dispatch(DispatchSubsystem* s);
void init_dispatch_table(void);
void dispatch_to_core(int core_id);

void dispatch_subsystems(void);

void dispatch_all_subsystems(void);

DispatchSubsystem *get_subsystem_by_core(int core_id);

void register_subsystem(const char *name, subsystem_fn fn);

void test_subsystem(void);

void set_affinity(const char *name, int core_id);

void debug(void);

void dispatch_to_affinity(void);

void fallback_dispatch(DispatchSubsystem* s);

void enqueue_subsystem(const char *name);

void dispatch_queue_run(void);

void subsystem_report(void);

void dispatch_metrics_reset(void);
int most_used_core(void);
void subsystem_unbind(const char *name);
void dispatch_log_report(void);
void test_dispatch_table(int core_id);
void dispatch_log(const char* message);
void dispatch_log_with_core(const char* message, int core_id);
void dispatch_metrics_increment(const char* name, int value);
void subsystem_history_push(const char* name, const char* data);

#endif // CPU_CORE_MANAGER_H
