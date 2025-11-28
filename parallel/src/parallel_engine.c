/**
 * QARMA - Parallel Processing Engine Implementation
 * 
 * Implementation of advanced multi-core processing with NUMA awareness
 * and lock-free work stealing algorithms.
 */

#include "parallel_engine.h"
#include "core/kernel.h"
#include "graphics/graphics.h"
#include "core/memory.h"
#include "core/memory/heap.h"

// Global parallel engine state
static cpu_core_t* g_cpu_cores = NULL;
static numa_node_t* g_numa_nodes = NULL;
static core_scheduler_t* g_core_schedulers = NULL;
static parallel_engine_stats_t g_engine_stats = {0};
static uint32_t g_next_task_id = 1;

// Constants
#define MAX_CORES 64
#define WORK_QUEUE_SIZE 1024
#define WORK_STEALING_THRESHOLD 4

/**
 * Initialize parallel processing engine
 */
void parallel_engine_init(void) {
    gfx_print("Initializing parallel processing engine...\n");
    
    // Reset statistics first
    memset(&g_engine_stats, 0, sizeof(parallel_engine_stats_t));
    
    // Detect CPU topology (sets total_cores, active_cores, numa_nodes)
    detect_cpu_topology();
    
    // Initialize per-core schedulers
    parallel_scheduler_init();
    
    gfx_print("Parallel processing engine initialized.\n");
}

/**
 * Initialize parallel processing drivers
 */
void parallel_drivers_init(void) {
    gfx_print("Loading parallel processing drivers...\n");
    
    // Initialize SMP (Symmetric Multi-Processing) support
    // Initialize NUMA topology detection
    // Initialize CPU frequency scaling
    
    gfx_print("Parallel processing drivers loaded.\n");
}

/**
 * Initialize parallel scheduler
 */
void parallel_scheduler_init(void) {
    gfx_print("Initializing parallel scheduler...\n");
    
    // Allocate scheduler data for each core
    g_core_schedulers = (core_scheduler_t*)heap_alloc(sizeof(core_scheduler_t) * MAX_CORES);
    memset(g_core_schedulers, 0, sizeof(core_scheduler_t) * MAX_CORES);
    
    // Initialize work queues for each core
    for (uint32_t i = 0; i < g_engine_stats.total_cores && i < MAX_CORES; i++) {
        core_scheduler_t* scheduler = &g_core_schedulers[i];
        scheduler->core_id = i;
        
        // Initialize work queue
        work_queue_t* queue = &scheduler->local_queue;
        queue->tasks = (parallel_task_t**)heap_alloc(sizeof(parallel_task_t*) * WORK_QUEUE_SIZE);
        queue->capacity = WORK_QUEUE_SIZE;
        queue->mask = WORK_QUEUE_SIZE - 1;  // Assuming power of 2
        queue->head = 0;
        queue->tail = 0;
    }
    
    gfx_print("Parallel scheduler initialized.\n");
}

/**
 * Detect CPU topology using CPUID
 */
void detect_cpu_topology(void) {
    uint32_t eax, ebx, ecx, edx;
    uint32_t logical_cores = 1;
    
    // Get max basic CPUID function
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0)
                     : );
    
    uint32_t max_basic = eax;
    
    if (max_basic >= 1) {
        // Get logical processor count (CPUID.1.EBX[23:16])
        __asm__ volatile("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(1)
                         : );
        
        logical_cores = (ebx >> 16) & 0xFF;
        
        gfx_print("CPUID.1: EBX=");
        gfx_print_hex(ebx);
        gfx_print(" logical_cores=");
        gfx_print_hex(logical_cores);
        gfx_print("\n");
        
        // Sanity check - limit to reasonable max to avoid issues
        if (logical_cores == 0) {
            gfx_print("Invalid core count (0), defaulting to 8\n");
            logical_cores = 8;
        } else if (logical_cores > 64) {
            gfx_print("Core count too high (");
            gfx_print_hex(logical_cores);
            gfx_print("), capping at 16 for stability\n");
            logical_cores = 16;  // Cap at 16 to avoid memory/timing issues
        }
    } else {
        gfx_print("CPUID not supported properly, defaulting to 8 cores\n");
        logical_cores = 8;
    }
    
    g_engine_stats.total_cores = logical_cores;
    g_engine_stats.active_cores = logical_cores;
    
    // Estimate NUMA nodes (1 node per 8 cores)
    g_engine_stats.numa_nodes = (logical_cores + 7) / 8;
    if (g_engine_stats.numa_nodes == 0) g_engine_stats.numa_nodes = 1;
    
    gfx_print("Detected ");
    gfx_print_hex(logical_cores);
    gfx_print(" logical cores, ");
    gfx_print_hex(g_engine_stats.numa_nodes);
    gfx_print(" NUMA nodes\n");
    
    // Create CPU core structures
    for (uint32_t i = 0; i < g_engine_stats.total_cores; i++) {
        cpu_core_t* core = (cpu_core_t*)heap_alloc(sizeof(cpu_core_t));
        memset(core, 0, sizeof(cpu_core_t));
        
        core->core_id = i;
        core->numa_node = i / 4;  // 4 cores per NUMA node
        core->online = true;
        core->frequency = 3000;   // 3 GHz
        core->cache_size_l1 = 32; // 32 KB
        core->cache_size_l2 = 256;// 256 KB
        core->cache_size_l3 = 8192; // 8 MB shared
        
        // Add to core list
        core->next = g_cpu_cores;
        g_cpu_cores = core;
    }
    
    // Create NUMA node structures
    for (uint32_t i = 0; i < g_engine_stats.numa_nodes; i++) {
        numa_node_t* node = (numa_node_t*)heap_alloc(sizeof(numa_node_t));
        memset(node, 0, sizeof(numa_node_t));
        
        node->node_id = i;
        node->core_count = 4;  // 4 cores per node
        node->total_memory = 16ULL * 1024 * 1024 * 1024;  // 16 GB per node
        node->available_memory = node->total_memory;
        node->memory_bandwidth = 25600;  // 25.6 GB/s
        
        // Add to node list
        node->next = g_numa_nodes;
        g_numa_nodes = node;
    }
}

/**
 * Create a new parallel task
 */
parallel_task_t* parallel_task_create(const char* name, void (*function)(void*), 
                                     void* data, size_t data_size) {
    parallel_task_t* task = (parallel_task_t*)heap_alloc(sizeof(parallel_task_t));
    if (!task) {
        return NULL;
    }
    
    memset(task, 0, sizeof(parallel_task_t));
    
    task->task_id = g_next_task_id++;
    task->function = function;
    task->data = data;
    task->data_size = data_size;
    task->state = PARALLEL_TASK_READY;
    task->priority = PARALLEL_PRIORITY_NORMAL;
    task->assigned_core = UINT32_MAX;  // Not assigned yet
    task->preferred_numa_node = 0;     // Default to node 0
    
    // Copy task name
    size_t name_len = strlen(name);
    if (name_len >= sizeof(task->name)) {
        name_len = sizeof(task->name) - 1;
    }
    memcpy(task->name, name, name_len);
    task->name[name_len] = '\0';
    
    g_engine_stats.total_tasks_created++;
    
    return task;
}

/**
 * Submit task for execution
 */
void parallel_task_submit(parallel_task_t* task) {
    if (!task) return;
    
    // Select best core for this task
    uint32_t best_core = select_best_core_for_task(task);
    task->assigned_core = best_core;
    
    // Add to core's work queue
    if (best_core < MAX_CORES) {
        work_queue_push(&g_core_schedulers[best_core].local_queue, task);
        g_engine_stats.tasks_in_flight++;
    }
}

/**
 * Parallel engine tick - called from main kernel loop
 */
void parallel_engine_tick(void) {
    // Update load balancing
    adaptive_load_balance();
    
    // Process tasks on each core (simplified - would be done in parallel)
    for (uint32_t core_id = 0; core_id < g_engine_stats.total_cores && core_id < MAX_CORES; core_id++) {
        core_scheduler_t* scheduler = &g_core_schedulers[core_id];
        
        // If no current task, get next task
        if (!scheduler->current_task) {
            scheduler->current_task = parallel_get_next_task(core_id);
        }
        
        // Execute current task
        if (scheduler->current_task) {
            parallel_execute_task(scheduler->current_task, core_id);
            
            // Check if task completed
            if (scheduler->current_task->state == PARALLEL_TASK_COMPLETED ||
                scheduler->current_task->state == PARALLEL_TASK_FAILED) {
                
                g_engine_stats.total_tasks_completed++;
                g_engine_stats.tasks_in_flight--;
                scheduler->tasks_executed++;
                scheduler->current_task = NULL;
            }
        } else {
            // Core is idle, try work stealing
            if (scheduler->steal_attempts % WORK_STEALING_THRESHOLD == 0) {
                uint32_t victim_core = (core_id + 1) % g_engine_stats.total_cores;
                parallel_task_t* stolen_task = work_stealing_attempt(core_id, victim_core);
                if (stolen_task) {
                    scheduler->current_task = stolen_task;
                    scheduler->tasks_stolen++;
                    g_engine_stats.work_stealing_events++;
                }
            }
            scheduler->steal_attempts++;
            scheduler->idle_time++;
        }
    }
}

/**
 * Get next task for a core
 */
parallel_task_t* parallel_get_next_task(uint32_t core_id) {
    if (core_id >= MAX_CORES) return NULL;
    
    core_scheduler_t* scheduler = &g_core_schedulers[core_id];
    return work_queue_pop(&scheduler->local_queue);
}

/**
 * Execute a task on specified core
 */
void parallel_execute_task(parallel_task_t* task, uint32_t core_id) {
    (void)core_id; // Suppress unused parameter warning
    if (!task || !task->function) return;
    
    task->state = PARALLEL_TASK_RUNNING;
    
    // Record start time (simplified)
    task->start_time = g_engine_stats.total_cpu_cycles;
    
    // Execute the task function
    task->function(task->data);
    
    // Record end time
    task->end_time = g_engine_stats.total_cpu_cycles + 100;  // Simulate execution time
    task->cpu_cycles_used = task->end_time - task->start_time;
    
    task->state = PARALLEL_TASK_COMPLETED;
    
    g_engine_stats.total_cpu_cycles += task->cpu_cycles_used;
}

/**
 * Select best core for a task
 */
uint32_t select_best_core_for_task(parallel_task_t* task) {
    uint32_t best_core = 0;
    uint32_t lowest_load = UINT32_MAX;
    
    // Find core with lowest load in preferred NUMA node
    for (uint32_t i = 0; i < g_engine_stats.total_cores && i < MAX_CORES; i++) {
        if (get_numa_node_for_core(i) == task->preferred_numa_node) {
            uint32_t load = calculate_core_load(i);
            if (load < lowest_load) {
                lowest_load = load;
                best_core = i;
            }
        }
    }
    
    return best_core;
}

/**
 * Calculate load for a core
 */
uint32_t calculate_core_load(uint32_t core_id) {
    if (core_id >= MAX_CORES) return 100;
    
    core_scheduler_t* scheduler = &g_core_schedulers[core_id];
    work_queue_t* queue = &scheduler->local_queue;
    
    // Simple load calculation based on queue size
    uint32_t queue_size = (queue->tail - queue->head) & queue->mask;
    return (queue_size * 100) / queue->capacity;
}

/**
 * Get CPU core count
 */
uint32_t get_cpu_core_count(void) {
    return g_engine_stats.total_cores;
}

/**
 * Get NUMA node count
 */
uint32_t get_numa_node_count(void) {
    return g_engine_stats.numa_nodes;
}

/**
 * Get NUMA node for a core
 */
uint32_t get_numa_node_for_core(uint32_t core_id) {
    cpu_core_t* core = g_cpu_cores;
    while (core) {
        if (core->core_id == core_id) {
            return core->numa_node;
        }
        core = core->next;
    }
    return 0;  // Default to node 0
}

/**
 * Adaptive load balancing
 */
void adaptive_load_balance(void) {
    // Simple load balancing - in real implementation would be more sophisticated
    
    for (uint32_t i = 0; i < g_engine_stats.total_cores && i < MAX_CORES; i++) {
        uint32_t load = calculate_core_load(i);
        
        // If core is heavily loaded, try to migrate some tasks
        if (load > 80) {
            // Find a less loaded core
            for (uint32_t j = 0; j < g_engine_stats.total_cores && j < MAX_CORES; j++) {
                if (i != j && calculate_core_load(j) < 20) {
                    // Migrate task if possible (simplified)
                    parallel_task_t* task = work_queue_steal(&g_core_schedulers[i].local_queue);
                    if (task) {
                        work_queue_push(&g_core_schedulers[j].local_queue, task);
                        task->assigned_core = j;
                        break;
                    }
                }
            }
        }
    }
}

/**
 * Work stealing attempt
 */
parallel_task_t* work_stealing_attempt(uint32_t stealing_core, uint32_t victim_core) {
    if (stealing_core >= MAX_CORES || victim_core >= MAX_CORES) return NULL;
    
    return work_queue_steal(&g_core_schedulers[victim_core].local_queue);
}

/**
 * Push task to work queue
 */
void work_queue_push(work_queue_t* queue, parallel_task_t* task) {
    if (!queue || !task) return;
    
    uint32_t tail = atomic_load(&queue->tail);
    uint32_t next_tail = (tail + 1) & queue->mask;
    
    // Check if queue is full
    if (next_tail == atomic_load(&queue->head)) {
        return;  // Queue full
    }
    
    queue->tasks[tail] = task;
    atomic_store(&queue->tail, next_tail);
}

/**
 * Pop task from work queue (local end)
 */
parallel_task_t* work_queue_pop(work_queue_t* queue) {
    if (!queue) return NULL;
    
    uint32_t head = atomic_load(&queue->head);
    uint32_t tail = atomic_load(&queue->tail);
    
    if (head == tail) {
        return NULL;  // Queue empty
    }
    
    parallel_task_t* task = queue->tasks[head];
    atomic_store(&queue->head, (head + 1) & queue->mask);
    
    return task;
}

/**
 * Steal task from work queue (remote end)
 */
parallel_task_t* work_queue_steal(work_queue_t* queue) {
    if (!queue) return NULL;
    
    uint32_t tail = atomic_load(&queue->tail);
    uint32_t head = atomic_load(&queue->head);
    
    if (head >= tail) {
        return NULL;  // Queue empty or being modified
    }
    
    // Try to steal from tail end
    uint32_t new_tail = (tail - 1) & queue->mask;
    parallel_task_t* task = queue->tasks[new_tail];
    
    // Atomic compare-exchange to ensure we got the task
    if (atomic_compare_exchange(&queue->tail, tail, new_tail)) {
        return task;
    }
    
    return NULL;  // Failed to steal
}

/**
 * Get parallel engine statistics
 */
parallel_engine_stats_t* parallel_get_engine_stats(void) {
    return &g_engine_stats;
}

// Atomic operations (simplified implementations)
uint32_t atomic_compare_exchange(volatile uint32_t* ptr, uint32_t expected, uint32_t desired) {
    // Simplified - would use actual atomic instructions
    if (*ptr == expected) {
        *ptr = desired;
        return expected;
    }
    return *ptr;
}

uint32_t atomic_fetch_add(volatile uint32_t* ptr, uint32_t value) {
    uint32_t old = *ptr;
    *ptr += value;
    return old;
}

uint32_t atomic_load(volatile uint32_t* ptr) {
    return *ptr;
}

void atomic_store(volatile uint32_t* ptr, uint32_t value) {
    *ptr = value;
}

/**
 * Core manager integration - register core allocation
 */
void parallel_register_core_allocation(uint32_t core_id, uint32_t subsystem_id) {
    (void)subsystem_id; // Track subsystem ownership if needed
    
    if (core_id >= MAX_CORES) return;
    
    // Mark core as actively managed
    cpu_core_t* core = g_cpu_cores;
    while (core) {
        if (core->core_id == core_id) {
            // Core is now allocated to a specific subsystem
            core->online = true;
            break;
        }
        core = core->next;
    }
}

/**
 * Core manager integration - unregister core allocation
 */
void parallel_unregister_core_allocation(uint32_t core_id, uint32_t subsystem_id) {
    (void)subsystem_id; // Track subsystem ownership if needed
    (void)core_id;
    
    // Core is being released back to the pool
    // Keep core online for future allocations
}

/**
 * Update task affinity to specific core
 */
void parallel_update_core_affinity(parallel_task_t* task, uint32_t core_id) {
    if (!task || core_id >= MAX_CORES) return;
    
    task->assigned_core = core_id;
    task->preferred_numa_node = get_numa_node_for_core(core_id);
}

/**
 * Check if core is busy
 */
bool parallel_is_core_busy(uint32_t core_id) {
    if (core_id >= MAX_CORES) return true;
    
    core_scheduler_t* scheduler = &g_core_schedulers[core_id];
    if (!scheduler) return false;
    
    // Core is busy if it has a current task or pending work
    return (scheduler->current_task != NULL) || 
           (atomic_load(&scheduler->local_queue.head) != atomic_load(&scheduler->local_queue.tail));
}