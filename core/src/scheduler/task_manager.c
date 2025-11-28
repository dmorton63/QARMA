#include "../../headers/scheduler/task_manager.h" --- IGNORE ---
#include "kernel.h"
#include "../../headers/memory/heap.h"
#include "timer.h"
#include "config.h"
#include "string.h"
#include "quantum_kernel.h"
#include "quantum_ai_observer.h"
#include "quantum_register.h"

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
    
    /* Initialize quantum AI tracking */
    task->execution_count = 0;
    task->runtime_sum = 0;
    task->runtime_variance = 0;
    task->last_execution_time = 0;
    task->memory_usage = stack_size;
    
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
    
    /* Set up stack pointer at end of stack (64-bit aligned) */
    uint64_t *stack_ptr = (uint64_t*)((char*)task->stack_base + task->stack_size);
    
    /* Push initial values onto stack for context switch (64-bit) */
    *(--stack_ptr) = (uint64_t)entry_point;  /* Return address (RIP) */
    *(--stack_ptr) = 0;                       /* RBP */
    *(--stack_ptr) = 0;                       /* RDI */
    *(--stack_ptr) = 0;                       /* RSI */
    *(--stack_ptr) = 0;                       /* RDX */
    *(--stack_ptr) = 0;                       /* RCX */
    *(--stack_ptr) = 0;                       /* RBX */
    *(--stack_ptr) = (uint64_t)user_data;    /* RAX (first parameter) */
    
    /* Set initial context for task switching (64-bit) */
    task->context.rsp = (uint64_t)stack_ptr;
    task->context.rip = (uint64_t)entry_point;
    task->context.rflags = 0x202;  /* Enable interrupts */
}

/**
 * Create a quantum workload profile from task characteristics
 */
static quantum_workload_profile_t task_create_workload_profile(task_t *task)
{
    quantum_workload_profile_t profile;
    
    /* Calculate qubit count based on task complexity */
    /* Factors: priority (inverse), stack size, execution variance */
    uint32_t complexity = 2;  /* Base complexity */
    
    /* Higher priority = more complex quantum state (inverse priority value) */
    complexity += (TASK_PRIORITY_IDLE - task->priority);
    
    /* Large stack = more complex task */
    if (task->stack_size >= 8192) complexity += 2;
    else if (task->stack_size >= 4096) complexity += 1;
    
    /* High variance = needs more qubits to represent uncertainty */
    if (task->runtime_variance > 1000) complexity += 1;
    
    profile.qubit_count = (complexity < 2) ? 2 : ((complexity > 8) ? 8 : complexity);
    
    /* Average execution time (in microseconds for precision) */
    if (task->execution_count > 0) {
        /* Safe 64-bit division on 32-bit platform */
        uint32_t avg_high = (uint32_t)(task->runtime_sum >> 32);
        uint32_t avg_low = (uint32_t)(task->runtime_sum & 0xFFFFFFFF);
        if (avg_high == 0) {
            profile.avg_execution_time = avg_low / task->execution_count;
        } else {
            /* For large sums, use approximation to avoid overflow */
            profile.avg_execution_time = (avg_low / task->execution_count) + 
                                        (avg_high * (0xFFFFFFFF / task->execution_count));
        }
    } else {
        profile.avg_execution_time = task->time_slice * 1000; /* Estimate from time slice */
    }
    
    /* Variance in execution time */
    profile.variance = task->runtime_variance;
    
    /* Has evaluation if task has run before */
    profile.has_evaluation = (task->execution_count > 0) ? 1 : 0;
    
    /* Requires all results for critical/high priority tasks */
    profile.requires_all = (task->priority <= TASK_PRIORITY_HIGH) ? 1 : 0;
    
    /* Data size from memory usage */
    profile.data_size = task->memory_usage;
    
    return profile;
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
            /* Update task execution statistics */
            uint32_t elapsed = prev_task->time_slice - prev_task->time_remaining;
            prev_task->execution_count++;
            prev_task->runtime_sum += elapsed;
            prev_task->last_execution_time = elapsed;
            prev_task->total_runtime += elapsed;
            
            /* Calculate variance if we have enough samples */
            if (prev_task->execution_count > 1) {
                /* Safe 64-bit division on 32-bit platform */
                uint32_t avg;
                uint32_t sum_high = (uint32_t)(prev_task->runtime_sum >> 32);
                uint32_t sum_low = (uint32_t)(prev_task->runtime_sum & 0xFFFFFFFF);
                if (sum_high == 0) {
                    avg = sum_low / prev_task->execution_count;
                } else {
                    avg = (sum_low / prev_task->execution_count) + 
                          (sum_high * (0xFFFFFFFF / prev_task->execution_count));
                }
                int32_t diff = (int32_t)elapsed - (int32_t)avg;
                /* Simple variance update (not true variance but good enough) */
                prev_task->runtime_variance = (prev_task->runtime_variance * 3 + (diff * diff)) / 4;
            }
            
            /* Quantum AI: Observe task completion if quantum is enabled */
            if (quantum_is_enabled() && prev_task->quantum_reg) {
                QARMA_QUANTUM_REGISTER* reg = (QARMA_QUANTUM_REGISTER*)prev_task->quantum_reg;
                
                /* Calculate quality based on execution efficiency */
                /* Full time slice used = efficient (0.9), preempted early = less efficient */
                float time_usage_ratio = (float)elapsed / (float)prev_task->time_slice;
                float quality = 0.5f + (time_usage_ratio * 0.4f);  /* Range: 0.5 to 0.9 */
                
                /* Bonus for consistent execution times (low variance) */
                if (prev_task->execution_count > 5 && prev_task->runtime_variance < 500) {
                    quality += 0.1f;  /* Predictable tasks get quality boost */
                }
                
                quantum_ai_observe_complete(reg, elapsed, quality);
                qarma_quantum_register_destroy(reg);
                prev_task->quantum_reg = NULL;
            }
            
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
        
        /* Quantum AI: Observe task start if quantum is enabled */
        if (quantum_is_enabled()) {
            SERIAL_LOG("[TASK_SCHEDULE] Quantum enabled, observing task start\n");
            /* Create detailed workload profile from task characteristics */
            quantum_workload_profile_t profile = task_create_workload_profile(next_task);
            
            /* Create quantum register sized for task complexity */
            QARMA_QUANTUM_REGISTER* task_reg = qarma_quantum_register_create(profile.qubit_count);
            if (task_reg) {
                SERIAL_LOG("[TASK_SCHEDULE] Quantum register created, starting observation\n");
                /* Register the profile with the AI observer (extracts profile from register) */
                quantum_ai_profile_register(task_reg);
                
                /* Start observation with full profile context */
                quantum_ai_observe_start(task_reg);
                
                /* Store register pointer in task for completion observation */
                next_task->quantum_reg = (void*)task_reg;
            } else {
                SERIAL_LOG("[TASK_SCHEDULE] ERROR: Failed to create quantum register\n");
            }
        } else {
            SERIAL_LOG("[TASK_SCHEDULE] Quantum disabled, skipping observation\n");
        }
        
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
 * Enhanced with quantum AI recommendations when enabled
 */
static task_t* task_select_next(void)
{
    task_t *best_candidate = NULL;
    int best_priority = -1;
    float best_ai_score = 0.0f;
    
    /* If quantum AI is enabled, use AI recommendations to optimize selection */
    if (quantum_is_enabled()) {
        /* Scan all ready queues and score candidates */
        for (int i = 0; i < 5; i++) {
            task_t *task = task_mgr.ready_queue_head[i];
            while (task) {
                /* Create profile for this candidate */
                quantum_workload_profile_t profile = task_create_workload_profile(task);
                
                /* Get AI recommendation for this workload */
                QARMA_COLLAPSE_STRATEGY strategy = quantum_ai_recommend_strategy(&profile);
                
                /* Calculate score: priority weight + AI confidence bonus */
                /* Higher priority = lower i value = better base score */
                float priority_weight = (5.0f - (float)i) * 2.0f;
                
                /* AI adds 0-3 points based on confidence in recommendation */
                float confidence = quantum_ai_get_confidence(&profile, strategy);
                float ai_bonus = 0.0f;
                if (confidence > 0.8f) {
                    ai_bonus = 3.0f;  /* AI has high confidence */
                } else if (confidence > 0.5f) {
                    ai_bonus = 1.5f;  /* AI has moderate confidence */
                }
                
                float total_score = priority_weight + ai_bonus;
                
                /* Track best candidate */
                if (total_score > best_ai_score || best_candidate == NULL) {
                    best_candidate = task;
                    best_priority = i;
                    best_ai_score = total_score;
                }
                
                task = task->next;
            }
        }
        
        /* Remove and return AI-selected task */
        if (best_candidate) {
            task_queue_remove(&task_mgr.ready_queue_head[best_priority], 
                             &task_mgr.ready_queue_tail[best_priority], best_candidate);
            return best_candidate;
        }
    } else {
        /* Traditional priority-based round-robin when quantum disabled */
        for (int i = 0; i < 5; i++) {
            task_t *task = task_mgr.ready_queue_head[i];
            if (task) {
                /* Move to end for round-robin within priority */
                task_queue_remove(&task_mgr.ready_queue_head[i], 
                                 &task_mgr.ready_queue_tail[i], task);
                return task;
            }
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