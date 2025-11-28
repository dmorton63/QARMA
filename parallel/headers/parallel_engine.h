/**
 * QARMA - Parallel Processing Engine
 * 
 * Advanced multi-core and distributed processing engine with NUMA awareness,
 * lock-free data structures, and adaptive load balancing.
 */

#ifndef PARALLEL_ENGINE_H
#define PARALLEL_ENGINE_H

#include "kernel_types.h"
#include "core/kernel.h"

// CPU core information
typedef struct cpu_core {
    uint32_t core_id;                   // Physical core ID
    uint32_t numa_node;                 // NUMA node this core belongs to
    bool online;                        // Core is online and available
    uint32_t frequency;                 // Current frequency in MHz
    uint32_t cache_size_l1;             // L1 cache size in KB
    uint32_t cache_size_l2;             // L2 cache size in KB
    uint32_t cache_size_l3;             // L3 cache size in KB (shared)
    
    // Current workload
    uint32_t current_tasks;             // Number of tasks running
    uint32_t load_percentage;           // Load percentage (0-100)
    uint64_t total_cycles;              // Total CPU cycles used
    
    struct cpu_core* next;              // Next core in list
} cpu_core_t;

// NUMA node information
typedef struct numa_node {
    uint32_t node_id;                   // NUMA node ID
    uint32_t core_count;                // Number of cores in this node
    cpu_core_t* cores;                  // List of cores in this node
    
    // Memory information
    uint64_t total_memory;              // Total memory in bytes
    uint64_t available_memory;          // Available memory in bytes
    uint32_t memory_bandwidth;          // Memory bandwidth in MB/s
    
    struct numa_node* next;             // Next NUMA node
} numa_node_t;

// Parallel task states
typedef enum {
    PARALLEL_TASK_READY     = 0,        // Ready to run
    PARALLEL_TASK_RUNNING   = 1,        // Currently executing
    PARALLEL_TASK_WAITING   = 2,        // Waiting for dependency
    PARALLEL_TASK_COMPLETED = 3,        // Task completed
    PARALLEL_TASK_FAILED    = 4         // Task failed
} parallel_task_state_t;

// Task priority levels
typedef enum {
    PARALLEL_PRIORITY_CRITICAL = 0,     // Critical system tasks
    PARALLEL_PRIORITY_HIGH     = 1,     // High priority tasks
    PARALLEL_PRIORITY_NORMAL   = 2,     // Normal priority tasks
    PARALLEL_PRIORITY_LOW      = 3,     // Low priority tasks
    PARALLEL_PRIORITY_BATCH    = 4      // Batch/background tasks
} parallel_priority_t;

// Parallel task structure
typedef struct parallel_task {
    uint32_t task_id;                   // Unique task ID
    char name[32];                      // Task name
    
    // Task function and data
    void (*function)(void* data);       // Function to execute
    void* data;                         // Task data
    size_t data_size;                   // Size of task data
    
    // Scheduling information
    parallel_task_state_t state;        // Current task state
    parallel_priority_t priority;       // Task priority
    uint32_t assigned_core;             // Core assigned to run this task
    uint32_t preferred_numa_node;       // Preferred NUMA node
    
    // Dependencies
    uint32_t* dependencies;             // Array of task IDs this task depends on
    uint32_t dependency_count;          // Number of dependencies
    uint32_t completed_dependencies;    // Number of completed dependencies
    
    // Execution tracking
    uint64_t start_time;                // Start execution time
    uint64_t end_time;                  // End execution time
    uint64_t cpu_cycles_used;           // CPU cycles consumed
    
    // Queue pointers
    struct parallel_task* next;         // Next task in queue
    struct parallel_task* prev;         // Previous task in queue
    
} parallel_task_t;

// Work stealing queue (lock-free)
typedef struct work_queue {
    parallel_task_t** tasks;            // Array of task pointers
    volatile uint32_t head;             // Head index (consumer end)
    volatile uint32_t tail;             // Tail index (producer end)
    uint32_t capacity;                  // Queue capacity
    uint32_t mask;                      // Capacity mask for wrap-around
} work_queue_t;

// Per-core scheduler data
typedef struct core_scheduler {
    uint32_t core_id;                   // Core this scheduler belongs to
    work_queue_t local_queue;           // Local work queue
    parallel_task_t* current_task;      // Currently executing task
    
    // Statistics
    uint64_t tasks_executed;            // Total tasks executed
    uint64_t tasks_stolen;              // Tasks stolen from other cores
    uint64_t steal_attempts;            // Work stealing attempts
    uint64_t idle_time;                 // Time spent idle
    
} core_scheduler_t;

// Parallel engine statistics
typedef struct {
    uint32_t total_cores;               // Total CPU cores
    uint32_t active_cores;              // Currently active cores
    uint32_t numa_nodes;                // Number of NUMA nodes
    uint32_t total_tasks_created;       // Total tasks created
    uint32_t total_tasks_completed;     // Total tasks completed
    uint32_t tasks_in_flight;           // Currently executing tasks
    uint64_t total_cpu_cycles;          // Total CPU cycles used
    uint32_t work_stealing_events;      // Work stealing events
} parallel_engine_stats_t;

// Function declarations

// Engine initialization
void parallel_engine_init(void);
void parallel_drivers_init(void);
void parallel_scheduler_init(void);

// CPU topology detection
void detect_cpu_topology(void);
uint32_t get_cpu_core_count(void);
uint32_t get_numa_node_count(void);
cpu_core_t* get_cpu_core_info(uint32_t core_id);
numa_node_t* get_numa_node_info(uint32_t node_id);

// Task management
parallel_task_t* parallel_task_create(const char* name, void (*function)(void*), 
                                     void* data, size_t data_size);
void parallel_task_destroy(parallel_task_t* task);
void parallel_task_add_dependency(parallel_task_t* task, uint32_t dependency_id);
void parallel_task_submit(parallel_task_t* task);
bool parallel_task_is_ready(parallel_task_t* task);

// Scheduling and execution
void parallel_engine_tick(void);
void parallel_schedule_task(parallel_task_t* task);
parallel_task_t* parallel_get_next_task(uint32_t core_id);
void parallel_execute_task(parallel_task_t* task, uint32_t core_id);

// Work stealing
parallel_task_t* work_stealing_attempt(uint32_t stealing_core, uint32_t victim_core);
void work_queue_push(work_queue_t* queue, parallel_task_t* task);
parallel_task_t* work_queue_pop(work_queue_t* queue);
parallel_task_t* work_queue_steal(work_queue_t* queue);

// Load balancing
void adaptive_load_balance(void);
uint32_t calculate_core_load(uint32_t core_id);
uint32_t select_best_core_for_task(parallel_task_t* task);
void migrate_task(parallel_task_t* task, uint32_t from_core, uint32_t to_core);

// NUMA awareness
uint32_t get_numa_node_for_core(uint32_t core_id);
bool is_numa_local_memory(void* ptr, uint32_t numa_node);
void* numa_alloc_memory(size_t size, uint32_t preferred_node);
void numa_free_memory(void* ptr, size_t size);

// Statistics and monitoring
parallel_engine_stats_t* parallel_get_engine_stats(void);
void parallel_print_core_info(uint32_t core_id);
void parallel_debug_dump_queues(void);

// Core manager integration hooks
void parallel_register_core_allocation(uint32_t core_id, uint32_t subsystem_id);
void parallel_unregister_core_allocation(uint32_t core_id, uint32_t subsystem_id);
void parallel_update_core_affinity(parallel_task_t* task, uint32_t core_id);
bool parallel_is_core_busy(uint32_t core_id);

// Atomic operations for lock-free data structures
uint32_t atomic_compare_exchange(volatile uint32_t* ptr, uint32_t expected, uint32_t desired);
uint32_t atomic_fetch_add(volatile uint32_t* ptr, uint32_t value);
uint32_t atomic_load(volatile uint32_t* ptr);
void atomic_store(volatile uint32_t* ptr, uint32_t value);

#endif // PARALLEL_ENGINE_H