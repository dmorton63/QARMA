#include "task_manager.h"
#include "../kernel.h"
#include "../memory/heap.h"
#include "../timer.h"
#include "config.h"
#include "../string.h"

/* Task manager global state */
static struct {
    bool initialized;
    uint32_t next_task_id;
    task_t *current_task;
    task_manager_stats_t stats;
    
    /* Ready queues per priority level */
    task_t *ready_queue_head[5];
    task_t *ready_queue_tail[5];
    
    /* Other state queues */
    task_t *blocked_queue;
    task_t *sleeping_queue;
    task_t *terminated_queue;
    
    /* Idle task */
    task_t *idle_task;
    
    /* Scheduler state */
    bool scheduler_enabled;
    uint32_t preempt_ticks;
} task_mgr;

/* Default stack size for tasks */
#define DEFAULT_STACK_SIZE (8192)  /* 8KB default stack */
#define IDLE_STACK_SIZE (2048)     /* 2KB for idle task */

/* Time slice durations per priority (in timer ticks) */
static const uint32_t priority_time_slices[] = {
    [TASK_PRIORITY_CRITICAL] = 50,   /* 50 ticks for critical */
    [TASK_PRIORITY_HIGH]     = 20,   /* 20 ticks for high */
    [TASK_PRIORITY_NORMAL]   = 10,   /* 10 ticks for normal */
    [TASK_PRIORITY_LOW]      = 5,    /* 5 ticks for low */
    [TASK_PRIORITY_IDLE]     = 1     /* 1 tick for idle */
};

/* Forward declarations */
static task_t* task_alloc(void);
static void task_free(task_t *task);
static void task_queue_add(task_t **head, task_t **tail, task_t *task);
static void task_queue_remove(task_t **head, task_t **tail, task_t *task);
static task_t* task_queue_pop(task_t **head, task_t **tail);
static task_t* task_select_next(void);
static void task_add_to_ready_queue(task_t *task);
static void task_remove_from_ready_queue(task_t *task);
static int idle_task_entry(void *data);
static void task_setup_initial_stack(task_t *task, task_entry_func_t entry_point, void *user_data);

/* Assembly function from task_switch.asm */
extern void task_switch_context_asm(task_t *from_task, task_t *to_task);

/**
 * Initialize the task manager system
 */
void task_manager_init(void)
{
    SERIAL_LOG("TASK: Initializing task manager with real switching\\n");
    
    /* Clear task manager state */
    memset(&task_mgr, 0, sizeof(task_mgr));
    
    /* Initialize queues */
    for (int i = 0; i < 5; i++) {
        task_mgr.ready_queue_head[i] = NULL;
        task_mgr.ready_queue_tail[i] = NULL;
    }
    
    task_mgr.blocked_queue = NULL;
    task_mgr.sleeping_queue = NULL;
    task_mgr.terminated_queue = NULL;
    
    /* Start with task ID 1 (0 reserved for kernel) */
    task_mgr.next_task_id = 1;
    task_mgr.current_task = NULL;
    task_mgr.scheduler_enabled = true;
    task_mgr.preempt_ticks = 0;
    
    /* Mark as initialized */
    task_mgr.initialized = true;
    
    /* Create idle task */
    task_mgr.idle_task = task_create("idle", idle_task_entry, NULL, 
                                     TASK_PRIORITY_IDLE, TASK_FLAG_KERNEL);
    if (task_mgr.idle_task) {
        task_start(task_mgr.idle_task);
        SERIAL_LOG("TASK: Idle task created and started\\n");
    }
    
    SERIAL_LOG("TASK: Task manager initialized with real switching\\n");
}

/**
 * Create a new task
 */
task_t* task_create(const char *name, task_entry_func_t entry_point, 
                   void *user_data, task_priority_t priority, uint32_t flags)
{
    if (!task_mgr.initialized) {
        SERIAL_LOG("TASK: ERROR - Task manager not initialized\\n");
        return NULL;
    }
    
    if (!name || !entry_point) {
        SERIAL_LOG("TASK: ERROR - Invalid parameters for task creation\\n");
        return NULL;
    }
    
    /* Allocate task structure */
    task_t *task = task_alloc();
    if (!task) {
        SERIAL_LOG("TASK: ERROR - Failed to allocate task structure\\n");
        return NULL;
    }
    
    /* Initialize task fields */
    task->task_id = task_mgr.next_task_id++;
    
    /* Copy name safely */
    size_t name_len = strlen(name);
    if (name_len >= sizeof(task->name)) {
        name_len = sizeof(task->name) - 1;
    }
    for (size_t i = 0; i < name_len; i++) {
        task->name[i] = name[i];
    }
    task->name[name_len] = '\0';
    
    task->state = TASK_STATE_CREATED;
    task->priority = priority;
    task->flags = flags;
    
    /* Allocate stack */
    size_t stack_size = (flags & TASK_FLAG_KERNEL) ? DEFAULT_STACK_SIZE : DEFAULT_STACK_SIZE;
    if (task == task_mgr.idle_task) {
        stack_size = IDLE_STACK_SIZE;
    }
    
    task->stack_base = task_allocate_stack(stack_size);
    if (!task->stack_base) {
        SERIAL_LOG("TASK: ERROR - Failed to allocate stack\\n");
        task_free(task);
        return NULL;
    }
    task->stack_size = stack_size;
    
    /* Set up initial context and stack */
    task_setup_initial_stack(task, entry_point, user_data);
    
    /* Set up segments for kernel/user space */
    if (flags & TASK_FLAG_KERNEL) {
        task->context.cs = 0x08;  /* Kernel code segment */
        task->context.ds = task->context.es = task->context.fs = 
        task->context.gs = task->context.ss = 0x10;  /* Kernel data segment */
    } else {
        task->context.cs = 0x1B;  /* User code segment */
        task->context.ds = task->context.es = task->context.fs = 
        task->context.gs = task->context.ss = 0x23;  /* User data segment */
    }
    
    /* Initialize timing */
    task->time_slice = priority_time_slices[priority];
    task->time_remaining = task->time_slice;
    task->total_runtime = 0;
    task->wake_time = 0;
    
    /* Set entry point and user data */
    task->entry_point = entry_point;
    task->user_data = user_data;
    
    /* Initialize family relationships */
    task->parent = task_mgr.current_task;  /* Current task is parent */
    task->first_child = NULL;
    task->next_sibling = NULL;
    
    /* Add to parent's children if there's a current task */
    if (task_mgr.current_task) {
        task->next_sibling = task_mgr.current_task->first_child;
        task_mgr.current_task->first_child = task;
    }
    
    /* Initialize queue pointers */
    task->next = NULL;
    task->prev = NULL;
    
    /* Update statistics */
    task_mgr.stats.total_tasks++;
    task_mgr.stats.active_tasks++;
    task_mgr.stats.tasks_by_priority[priority]++;
    
    SERIAL_LOG_HEX("TASK: Created task ID=", task->task_id);
    SERIAL_LOG(" name=");
    SERIAL_LOG(task->name);
    SERIAL_LOG("\\n");
    
    return task;
}

/**
 * Set up initial stack for new task
 */
static void task_setup_initial_stack(task_t *task, task_entry_func_t entry_point, void *user_data)
{
    /* Clear context */
    memset(&task->context, 0, sizeof(cpu_context_t));
    
    /* Set up stack pointer at end of stack */
    uint32_t *stack_ptr = (uint32_t*)((char*)task->stack_base + task->stack_size);
    
    /* Push initial values onto stack for context switch */
    *(--stack_ptr) = (uint32_t)entry_point;  /* Return address (EIP) */
    *(--stack_ptr) = 0;                       /* EBP */
    *(--stack_ptr) = 0;                       /* EDI */
    *(--stack_ptr) = 0;                       /* ESI */
    *(--stack_ptr) = 0;                       /* EDX */
    *(--stack_ptr) = 0;                       /* ECX */
    *(--stack_ptr) = 0;                       /* EBX */
    *(--stack_ptr) = (uint32_t)user_data;    /* EAX (first parameter) */
    
    /* Set initial context for task switching */
    task->context.esp = (uint32_t)stack_ptr;
    task->context.eip = (uint32_t)entry_point;
    task->context.eflags = 0x202;  /* Enable interrupts */
}

/**
 * Start a task (move from CREATED to READY state)
 */
int task_start(task_t *task)
{
    if (!task || task->state != TASK_STATE_CREATED) {
        return -1;
    }
    
    task->state = TASK_STATE_READY;
    task_add_to_ready_queue(task);
    
    SERIAL_LOG_HEX("TASK: Started task ID=", task->task_id);
    SERIAL_LOG("\\n");
    
    return 0;
}

/**
 * Main scheduler function - select and switch to next task
 */
void task_schedule(void)
{
    if (!task_mgr.initialized || !task_mgr.scheduler_enabled) {
        return;
    }
    
    task_mgr.stats.scheduler_calls++;
    
    /* Clean up any terminated tasks */
    task_cleanup_terminated();
    
    /* Select next task to run */
    task_t *next_task = task_select_next();
    if (!next_task) {
        /* Fallback to idle task */
        next_task = task_mgr.idle_task;
        if (!next_task) {
            SERIAL_LOG("TASK: CRITICAL - No tasks available!\\n");
            return;
        }
    }
    
    /* Perform actual task switching */
    if (next_task != task_mgr.current_task) {
        task_t *prev_task = task_mgr.current_task;
        
        /* Update task states */
        if (prev_task) {
            if (prev_task->state == TASK_STATE_RUNNING) {
                prev_task->state = TASK_STATE_READY;
                task_add_to_ready_queue(prev_task);
            }
        }
        
        next_task->state = TASK_STATE_RUNNING;
        task_remove_from_ready_queue(next_task);
        
        /* Update current task pointer */
        task_mgr.current_task = next_task;
        
        /* Reset time slice */
        next_task->time_remaining = next_task->time_slice;
        
        /* Perform context switch */
        task_switch_context(prev_task, next_task);
        
        task_mgr.stats.context_switches++;
    }
}

/**
 * Context switch between tasks
 */
void task_switch_context(task_t *from_task, task_t *to_task)
{
    if (!to_task) {
        SERIAL_LOG("TASK: ERROR - Cannot switch to NULL task\\n");
        return;
    }
    
    SERIAL_LOG("TASK: Switching to task ");
    SERIAL_LOG(to_task->name);
    SERIAL_LOG("\\n");
    
    /* Call assembly context switch function */
    task_switch_context_asm(from_task, to_task);
}

/**
 * Timer tick handler - handle preemption and sleeping tasks
 */
void task_timer_tick(void)
{
    if (!task_mgr.initialized || !task_mgr.scheduler_enabled) {
        return;
    }
    
    /* Check sleeping tasks */
    task_t *task = task_mgr.sleeping_queue;
    task_t *next;
    while (task) {
        next = task->next;
        if (get_ticks() >= task->wake_time) {
            /* Wake up this task */
            task_queue_remove(&task_mgr.sleeping_queue, NULL, task);
            task->state = TASK_STATE_READY;
            task_add_to_ready_queue(task);
        }
        task = next;
    }
    
    /* Handle preemption for current task */
    if (task_mgr.current_task && task_mgr.current_task->time_remaining > 0) {
        task_mgr.current_task->time_remaining--;
        
        /* Check if time slice expired */
        if (task_mgr.current_task->time_remaining == 0) {
            /* Force reschedule */
            task_schedule();
        }
    }
    
    /* Increment preemption counter */
    task_mgr.preempt_ticks++;
}

/**
 * Select next task to run based on priority scheduling with round-robin
 */
static task_t* task_select_next(void)
{
    /* Check ready queues from highest to lowest priority */
    for (int i = 0; i < 5; i++) {
        task_t *task = task_mgr.ready_queue_head[i];
        if (task) {
            /* Move to end for round-robin within priority */
            task_queue_remove(&task_mgr.ready_queue_head[i], 
                             &task_mgr.ready_queue_tail[i], task);
            return task;
        }
    }
    
    return task_mgr.idle_task;
}

/**
 * Voluntary yield CPU to other tasks
 */
void task_yield(void)
{
    if (task_mgr.current_task) {
        task_mgr.current_task->time_remaining = 0;
        task_schedule();
    }
}

/**
 * Sleep for specified milliseconds
 */
void task_sleep(uint32_t milliseconds)
{
    if (!task_mgr.current_task || milliseconds == 0) {
        return;
    }
    
    task_t *task = task_mgr.current_task;
    task->wake_time = get_ticks() + milliseconds;
    task->state = TASK_STATE_SLEEPING;
    
    task_queue_add(&task_mgr.sleeping_queue, NULL, task);
    task_schedule();
}

/**
 * Idle task - runs when no other tasks are ready
 */
static int idle_task_entry(void *data)
{
    (void)data;  /* Unused */
    
    SERIAL_LOG("TASK: Idle task started\\n");
    
    while (1) {
        /* Halt CPU until next interrupt */
        __asm__ volatile ("hlt");
        
        /* Yield to allow other tasks to run */
        task_yield();
    }
    
    return 0;  /* Never reached */
}

/* All other functions remain the same as original */

/**
 * Get current running task
 */
task_t* task_current(void)
{
    return task_mgr.current_task;
}

/**
 * Get current task ID
 */
uint32_t task_get_current_id(void)
{
    return task_mgr.current_task ? task_mgr.current_task->task_id : 0;
}

/**
 * Allocate a task structure
 */
static task_t* task_alloc(void)
{
    return (task_t*)heap_alloc(sizeof(task_t));
}

/**
 * Free a task structure
 */
static void task_free(task_t *task)
{
    if (task) {
        heap_free(task);
    }
}

/**
 * Allocate stack memory for a task
 */
void* task_allocate_stack(size_t stack_size)
{
    /* Align stack size to page boundary */
    stack_size = (stack_size + 0xFFF) & ~0xFFF;
    return heap_alloc(stack_size);
}

/**
 * Free stack memory
 */
void task_free_stack(void *stack_base, size_t stack_size)
{
    (void)stack_size;  /* Currently unused */
    if (stack_base) {
        heap_free(stack_base);
    }
}

/**
 * Add task to queue (generic function)
 */
static void task_queue_add(task_t **head, task_t **tail, task_t *task)
{
    if (!task) return;
    
    task->next = NULL;
    task->prev = *tail;
    
    if (*tail) {
        (*tail)->next = task;
    } else {
        *head = task;
    }
    if (tail) *tail = task;
}

/**
 * Remove task from queue (generic function)
 */
static void task_queue_remove(task_t **head, task_t **tail, task_t *task)
{
    if (!task) return;
    
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        *head = task->next;
    }
    
    if (task->next) {
        task->next->prev = task->prev;
    } else if (tail) {
        *tail = task->prev;
    }
    
    task->next = task->prev = NULL;
}

/**
 * Pop first task from queue
 */
static task_t* task_queue_pop(task_t **head, task_t **tail)
{
    task_t *task = *head;
    if (task) {
        task_queue_remove(head, tail, task);
    }
    return task;
}

/**
 * Add task to appropriate ready queue based on priority
 */
static void task_add_to_ready_queue(task_t *task)
{
    if (!task || task->priority >= 5) return;
    
    task_queue_add(&task_mgr.ready_queue_head[task->priority],
                   &task_mgr.ready_queue_tail[task->priority], task);
}

/**
 * Remove task from ready queue
 */
static void task_remove_from_ready_queue(task_t *task)
{
    if (!task || task->priority >= 5) return;
    
    task_queue_remove(&task_mgr.ready_queue_head[task->priority],
                      &task_mgr.ready_queue_tail[task->priority], task);
}

/**
 * Clean up terminated tasks
 */
void task_cleanup_terminated(void)
{
    task_t *task = task_mgr.terminated_queue;
    while (task) {
        task_t *next = task->next;
        
        /* Free stack and task structure */
        if (task->stack_base) {
            task_free_stack(task->stack_base, task->stack_size);
        }
        task_free(task);
        
        /* Update statistics */
        task_mgr.stats.active_tasks--;
        if (task->priority < 5) {
            task_mgr.stats.tasks_by_priority[task->priority]--;
        }
        
        task = next;
    }
    task_mgr.terminated_queue = NULL;
}

/**
 * Get task manager statistics
 */
void task_manager_get_stats(task_manager_stats_t *stats)
{
    if (!stats || !task_mgr.initialized) {
        return;
    }
    
    /* Copy current stats */
    stats->total_tasks = task_mgr.stats.total_tasks;
    stats->active_tasks = task_mgr.stats.active_tasks;
    stats->context_switches = task_mgr.stats.context_switches;
    stats->scheduler_calls = task_mgr.stats.scheduler_calls;
    
    /* Count tasks by priority */
    for (int i = 0; i < 5; i++) {
        uint32_t count = 0;
        task_t *task = task_mgr.ready_queue_head[i];
        while (task) {
            count++;
            task = task->next;
        }
        stats->tasks_by_priority[i] = count;
    }
}

/**
 * Terminate a task
 */
int task_terminate(task_t *task)
{
    if (!task) return -1;
    
    /* Remove from current queue */
    switch (task->state) {
        case TASK_STATE_READY:
            task_remove_from_ready_queue(task);
            break;
        case TASK_STATE_BLOCKED:
            task_queue_remove(&task_mgr.blocked_queue, NULL, task);
            break;
        case TASK_STATE_SLEEPING:
            task_queue_remove(&task_mgr.sleeping_queue, NULL, task);
            break;
        default:
            break;
    }
    
    /* Mark as terminated */
    task->state = TASK_STATE_TERMINATED;
    task_queue_add(&task_mgr.terminated_queue, NULL, task);
    
    /* If terminating current task, schedule next */
    if (task == task_mgr.current_task) {
        task_mgr.current_task = NULL;
        task_schedule();
    }
    
    return 0;
}

/**
 * Find task by ID
 */
task_t* task_find_by_id(uint32_t task_id)
{
    /* Check ready queues */
    for (int i = 0; i < 5; i++) {
        task_t *task = task_mgr.ready_queue_head[i];
        while (task) {
            if (task->task_id == task_id) return task;
            task = task->next;
        }
    }
    
    /* Check current task */
    if (task_mgr.current_task && task_mgr.current_task->task_id == task_id) {
        return task_mgr.current_task;
    }
    
    /* Check blocked and sleeping queues */
    task_t *task = task_mgr.blocked_queue;
    while (task) {
        if (task->task_id == task_id) return task;
        task = task->next;
    }
    
    task = task_mgr.sleeping_queue;
    while (task) {
        if (task->task_id == task_id) return task;
        task = task->next;
    }
    
    return NULL;  /* Not found */
}

/* Shutdown function */
void task_manager_shutdown(void)
{
    SERIAL_LOG("TASK: Shutting down task manager\\n");
    
    /* Terminate all tasks */
    for (int i = 0; i < 5; i++) {
        while (task_mgr.ready_queue_head[i]) {
            task_t *task = task_queue_pop(&task_mgr.ready_queue_head[i], 
                                         &task_mgr.ready_queue_tail[i]);
            task_terminate(task);
        }
    }
    
    /* Clean up terminated tasks */
    task_cleanup_terminated();
    
    task_mgr.initialized = false;
    SERIAL_LOG("TASK: Task manager shutdown complete\\n");
}

/* Simple stats function for backward compatibility */
void task_get_stats(task_manager_stats_t *stats)
{
    task_manager_get_stats(stats);
}