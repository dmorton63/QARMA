#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

// #include <stdint.h>
// #include <stdbool.h>
#include "core/stdtools.h"

/* Task states */
typedef enum {
    TASK_STATE_CREATED = 0,     /* Task created but not started */
    TASK_STATE_READY,           /* Ready to run */
    TASK_STATE_RUNNING,         /* Currently executing */
    TASK_STATE_BLOCKED,         /* Waiting for I/O or resource */
    TASK_STATE_SLEEPING,        /* Sleeping for specified time */
    TASK_STATE_TERMINATED,      /* Finished execution */
    TASK_STATE_ZOMBIE           /* Terminated but not cleaned up */
} task_state_t;

/* Task priorities (lower number = higher priority) */
typedef enum {
    TASK_PRIORITY_CRITICAL = 0, /* Kernel critical tasks */
    TASK_PRIORITY_HIGH = 1,     /* System services */
    TASK_PRIORITY_NORMAL = 2,   /* User applications */
    TASK_PRIORITY_LOW = 3,      /* Background tasks */
    TASK_PRIORITY_IDLE = 4      /* Idle/cleanup tasks */
} task_priority_t;

/* Task flags */
#define TASK_FLAG_KERNEL        (1 << 0)    /* Kernel space task */
#define TASK_FLAG_USER          (1 << 1)    /* User space task */
#define TASK_FLAG_SYSTEM        (1 << 2)    /* System service */
#define TASK_FLAG_PREEMPTIBLE   (1 << 3)    /* Can be preempted */
#define TASK_FLAG_PERSISTENT    (1 << 4)    /* Don't terminate on error */

/* CPU register context for task switching */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi;
    uint32_t esp, ebp;
    uint32_t eip;
    uint32_t eflags;
    uint16_t cs, ds, es, fs, gs, ss;
} cpu_context_t;

/* Task control block */
typedef struct task {
    uint32_t task_id;               /* Unique task identifier */
    char name[32];                  /* Task name for debugging */
    
    task_state_t state;             /* Current task state */
    task_priority_t priority;       /* Task priority level */
    uint32_t flags;                 /* Task flags */
    
    cpu_context_t context;          /* CPU register context */
    void *stack_base;               /* Stack base address */
    size_t stack_size;              /* Stack size in bytes */
    
    uint32_t time_slice;            /* Time slice in ms */
    uint32_t time_remaining;        /* Remaining time in current slice */
    uint32_t total_runtime;         /* Total CPU time used */
    uint32_t wake_time;             /* Wake up time (for sleeping tasks) */
    
    void *entry_point;              /* Task entry function */
    void *user_data;                /* User-defined data pointer */
    
    /* Parent/child relationships */
    struct task *parent;            /* Parent task */
    struct task *first_child;       /* First child task */
    struct task *next_sibling;      /* Next sibling task */
    
    /* Linked list pointers for scheduling queues */
    struct task *next;              /* Next task in queue */
    struct task *prev;              /* Previous task in queue */
} task_t;

/* Task manager statistics */
typedef struct {
    uint32_t total_tasks;           /* Total tasks created */
    uint32_t active_tasks;          /* Currently active tasks */
    uint32_t tasks_by_priority[5];  /* Tasks per priority level */
    uint32_t context_switches;     /* Total context switches */
    uint32_t scheduler_calls;       /* Scheduler invocation count */
} task_manager_stats_t;

/* Task entry point function type */
typedef int (*task_entry_func_t)(void *user_data);

/* Task manager initialization and core functions */
void task_manager_init(void);
void task_manager_shutdown(void);

/* Task creation and management */
task_t* task_create(const char *name, task_entry_func_t entry_point, 
                   void *user_data, task_priority_t priority, uint32_t flags);
int task_start(task_t *task);
int task_terminate(task_t *task);
int task_kill(uint32_t task_id);
void task_exit(int exit_code);

/* Task scheduling and state management */
void task_schedule(void);           /* Main scheduler function */
void task_yield(void);             /* Voluntary yield CPU */
void task_sleep(uint32_t milliseconds);
void task_wake(task_t *task);
void task_block(task_t *task);
void task_unblock(task_t *task);

/* Task lookup and information */
task_t* task_current(void);        /* Get current running task */
task_t* task_find_by_id(uint32_t task_id);
task_t* task_find_by_name(const char *name);
uint32_t task_get_current_id(void);

/* Priority management */
int task_set_priority(task_t *task, task_priority_t new_priority);
task_priority_t task_get_priority(task_t *task);

/* Timer callbacks for scheduler */
void task_timer_tick(void);        /* Called on timer interrupt */

/* Statistics and debugging */
void task_get_stats(task_manager_stats_t *stats);
void task_manager_get_stats(task_manager_stats_t *stats);
void task_dump_info(task_t *task);
void task_dump_all_tasks(void);

/* Memory management for tasks */
void* task_allocate_stack(size_t stack_size);
void task_free_stack(void *stack_base, size_t stack_size);

/* Internal functions (for scheduler use) */
void task_switch_context(task_t *from_task, task_t *to_task);
void task_cleanup_terminated(void);

#endif /* TASK_MANAGER_H */