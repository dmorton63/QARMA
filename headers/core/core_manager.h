/**
 * QARMA - Core Allocation Manager
 * 
 * Hybrid CPU core allocation system with dynamic allocation and priority reservations.
 * Provides subsystems with guaranteed minimum cores while allowing dynamic scaling.
 */

#ifndef CORE_MANAGER_H
#define CORE_MANAGER_H

#include "kernel_types.h"

// Subsystem identifiers
typedef enum {
    SUBSYSTEM_KERNEL        = 0,    // Kernel core operations
    SUBSYSTEM_AI            = 1,    // AI subsystem
    SUBSYSTEM_QUANTUM       = 2,    // Quantum subsystem
    SUBSYSTEM_PARALLEL      = 3,    // General parallel tasks
    SUBSYSTEM_SECURITY      = 4,    // Security manager
    SUBSYSTEM_VIDEO         = 5,    // Video/graphics subsystem
    SUBSYSTEM_IO            = 6,    // I/O operations
    SUBSYSTEM_NETWORK       = 7,    // Network stack (future)
    SUBSYSTEM_MAX           = 8
} subsystem_id_t;

// Core allocation request flags
typedef enum {
    CORE_ALLOC_PREFER_NUMA  = 0x01, // Prefer specific NUMA node
    CORE_ALLOC_EXCLUSIVE    = 0x02, // Request exclusive core access
    CORE_ALLOC_SHARED       = 0x04, // Allow shared core usage
    CORE_ALLOC_PERSISTENT   = 0x08, // Keep allocation across requests
    CORE_ALLOC_HIGH_PRIORITY= 0x10  // High priority allocation
} core_alloc_flags_t;

// Core allocation status
typedef enum {
    CORE_STATUS_FREE        = 0,    // Core is available
    CORE_STATUS_RESERVED    = 1,    // Reserved for subsystem minimum
    CORE_STATUS_ALLOCATED   = 2,    // Actively allocated to subsystem
    CORE_STATUS_SHARED      = 3,    // Shared between subsystems
    CORE_STATUS_OFFLINE     = 4     // Core is offline/disabled
} core_status_t;

// Core allocation entry
typedef struct core_allocation {
    uint32_t core_id;               // Physical core ID
    subsystem_id_t subsystem;       // Owning subsystem
    core_status_t status;           // Current status
    uint32_t flags;                 // Allocation flags
    uint32_t numa_node;             // NUMA node
    
    // Usage tracking
    uint64_t allocated_time;        // Time when allocated
    uint64_t total_usage_time;      // Total time used
    uint32_t task_count;            // Number of tasks run
    
    // Sharing information (for shared cores)
    subsystem_id_t sharing_with[4]; // Up to 4 subsystems can share
    uint32_t share_count;           // Number of subsystems sharing
    
    struct core_allocation* next;
} core_allocation_t;

// Subsystem core reservation policy
typedef struct subsystem_policy {
    subsystem_id_t subsystem;       // Subsystem ID
    uint32_t min_cores;             // Guaranteed minimum cores
    uint32_t max_cores;             // Maximum cores allowed
    uint32_t preferred_numa;        // Preferred NUMA node
    uint32_t priority;              // Priority level (0=highest)
    bool allow_sharing;             // Allow core sharing
    bool allow_preemption;          // Allow core preemption
} subsystem_policy_t;

// Core allocation request
typedef struct core_request {
    subsystem_id_t subsystem;       // Requesting subsystem
    uint32_t core_count;            // Number of cores requested
    uint32_t preferred_numa;        // Preferred NUMA node (or UINT32_MAX)
    uint32_t flags;                 // Allocation flags
    
    // Callback for task execution (optional)
    void (*task_function)(void* data);
    void* task_data;
    size_t task_data_size;
} core_request_t;

// Core allocation response
typedef struct core_response {
    bool success;                   // Request successful
    uint32_t cores_allocated;       // Number of cores allocated
    uint32_t core_ids[32];          // Allocated core IDs (max 32)
    char error_message[64];         // Error message if failed
} core_response_t;

// Core manager statistics
typedef struct {
    uint32_t total_cores;           // Total CPU cores
    uint32_t available_cores;       // Currently available cores
    uint32_t reserved_cores;        // Cores reserved for minimums
    uint32_t allocated_cores;       // Actively allocated cores
    uint32_t shared_cores;          // Cores in shared mode
    
    // Per-subsystem statistics
    uint32_t subsystem_cores[SUBSYSTEM_MAX];        // Cores per subsystem
    uint64_t subsystem_usage_time[SUBSYSTEM_MAX];   // Usage time per subsystem
    uint32_t subsystem_requests[SUBSYSTEM_MAX];     // Total requests
    uint32_t subsystem_failures[SUBSYSTEM_MAX];     // Failed requests
    
    // Global statistics
    uint64_t total_allocations;     // Total allocations made
    uint64_t total_deallocations;   // Total deallocations
    uint32_t preemptions;           // Number of preemptions
    uint32_t sharing_events;        // Core sharing events
} core_manager_stats_t;

// Function declarations

// Initialization
void core_manager_init(void);
void core_manager_set_topology(uint32_t total_cores, uint32_t numa_nodes);

// Policy management
void core_manager_set_policy(subsystem_id_t subsystem, subsystem_policy_t* policy);
subsystem_policy_t* core_manager_get_policy(subsystem_id_t subsystem);
void core_manager_apply_default_policies(void);

// Core allocation API
core_response_t core_request_allocate(core_request_t* request);
bool core_release(subsystem_id_t subsystem, uint32_t core_id);
bool core_release_all(subsystem_id_t subsystem);

// Core pinning (for specific task execution)
bool core_pin_task(uint32_t core_id, void (*function)(void*), void* data);
bool core_pin_task_subsystem(subsystem_id_t subsystem, void (*function)(void*), void* data);

// Core querying
uint32_t core_get_available_count(subsystem_id_t subsystem);
uint32_t core_get_allocated_count(subsystem_id_t subsystem);
bool core_is_available(uint32_t core_id);
subsystem_id_t core_get_owner(uint32_t core_id);
uint32_t core_get_load(uint32_t core_id);

// Core sharing
bool core_request_shared(subsystem_id_t subsystem, uint32_t core_id);
bool core_release_shared(subsystem_id_t subsystem, uint32_t core_id);

// NUMA awareness
uint32_t core_get_numa_node(uint32_t core_id);
uint32_t core_get_available_on_numa(uint32_t numa_node);
core_response_t core_request_numa_local(subsystem_id_t subsystem, uint32_t numa_node, uint32_t count);

// Load balancing and rebalancing
void core_manager_rebalance(void);
void core_manager_update_loads(void);
bool core_can_preempt(uint32_t core_id, subsystem_id_t requesting_subsystem);

// Statistics and monitoring
core_manager_stats_t* core_manager_get_stats(void);
void core_manager_print_allocation_map(void);
void core_manager_print_subsystem_info(subsystem_id_t subsystem);

// Utility functions
const char* subsystem_id_to_string(subsystem_id_t subsystem);
const char* core_status_to_string(core_status_t status);

#endif // CORE_MANAGER_H
