/**
 * QARMA - Core Allocation Manager Implementation
 * 
 * Hybrid CPU core allocation with dynamic scaling and guaranteed minimums.
 */

#include "core_manager.h"
#include "kernel.h"
#include "memory.h"
#include "memory/heap.h"
#include "graphics/graphics.h"
#include "parallel/parallel_engine.h"

// Forward declarations from core_manager.h
typedef struct core_request core_request_t;
typedef struct core_response core_response_t;

// Forward declarations
static void reserve_minimum_cores(void);
static void update_available_cores_count(void);

// Global state
static core_allocation_t* g_allocations = NULL;
static subsystem_policy_t g_policies[SUBSYSTEM_MAX];
static core_manager_stats_t g_stats = {0};
static bool g_initialized = false;

// Default policies for subsystems
static const subsystem_policy_t DEFAULT_POLICIES[SUBSYSTEM_MAX] = {
    // KERNEL: 1-2 cores, highest priority
    {SUBSYSTEM_KERNEL, 1, 2, 0, 0, false, false},
    // AI: 2-4 cores, high priority
    {SUBSYSTEM_AI, 2, 4, 0, 1, true, false},
    // QUANTUM: 2-4 cores, high priority
    {SUBSYSTEM_QUANTUM, 2, 4, 1, 1, true, false},
    // PARALLEL: 1-8 cores, normal priority
    {SUBSYSTEM_PARALLEL, 1, 8, 0, 2, true, true},
    // SECURITY: 1-2 cores, high priority
    {SUBSYSTEM_SECURITY, 1, 2, 0, 1, false, false},
    // VIDEO: 1-2 cores, normal priority
    {SUBSYSTEM_VIDEO, 1, 2, 0, 2, true, true},
    // IO: 0-2 cores, low priority
    {SUBSYSTEM_IO, 0, 2, 0, 3, true, true},
    // NETWORK: 0-2 cores, normal priority
    {SUBSYSTEM_NETWORK, 0, 2, 0, 2, true, true}
};

/**
 * Initialize core allocation manager
 */
void core_manager_init(void) {
    if (g_initialized) return;
    
    gfx_print("Initializing Core Allocation Manager...\n");
    
    // Initialize allocations list
    g_allocations = NULL;
    
    // Apply default policies
    core_manager_apply_default_policies();
    
    // Get topology from parallel engine
    uint32_t total_cores = get_cpu_core_count();
    uint32_t numa_nodes = get_numa_node_count();
    
    gfx_print("Core Manager: Detected ");
    gfx_print_hex(total_cores);
    gfx_print(" cores, ");
    gfx_print_hex(numa_nodes);
    gfx_print(" NUMA nodes\n");
    
    core_manager_set_topology(total_cores, numa_nodes);
    
    // Initialize statistics
    memset(&g_stats, 0, sizeof(core_manager_stats_t));
    g_stats.total_cores = total_cores;
    g_stats.available_cores = total_cores;
    
    // Create allocation entries for each core
    for (uint32_t i = 0; i < total_cores; i++) {
        core_allocation_t* alloc = (core_allocation_t*)heap_alloc(sizeof(core_allocation_t));
        memset(alloc, 0, sizeof(core_allocation_t));
        
        alloc->core_id = i;
        alloc->subsystem = SUBSYSTEM_MAX; // No owner
        alloc->status = CORE_STATUS_FREE;
        alloc->numa_node = get_numa_node_for_core(i);
        
        // Add to list
        alloc->next = g_allocations;
        g_allocations = alloc;
    }
    
    // Reserve cores for guaranteed minimums
    reserve_minimum_cores();
    
    g_initialized = true;
    gfx_print("Core Allocation Manager initialized.\n");
}

/**
 * Set topology information
 */
void core_manager_set_topology(uint32_t total_cores, uint32_t numa_nodes) {
    (void)numa_nodes; // Suppress unused warning
    g_stats.total_cores = total_cores;
    g_stats.available_cores = total_cores;
}

/**
 * Apply default policies
 */
void core_manager_apply_default_policies(void) {
    for (uint32_t i = 0; i < SUBSYSTEM_MAX; i++) {
        g_policies[i] = DEFAULT_POLICIES[i];
    }
}

/**
 * Set policy for a subsystem
 */
void core_manager_set_policy(subsystem_id_t subsystem, subsystem_policy_t* policy) {
    if (subsystem >= SUBSYSTEM_MAX || !policy) return;
    g_policies[subsystem] = *policy;
}

/**
 * Get policy for a subsystem
 */
subsystem_policy_t* core_manager_get_policy(subsystem_id_t subsystem) {
    if (subsystem >= SUBSYSTEM_MAX) return NULL;
    return &g_policies[subsystem];
}

/**
 * Reserve minimum cores for each subsystem
 */
static void reserve_minimum_cores(void) {
    uint32_t cores_reserved = 0;
    
    // Sort subsystems by priority (lower number = higher priority)
    for (uint32_t priority = 0; priority < 5; priority++) {
        for (uint32_t s = 0; s < SUBSYSTEM_MAX; s++) {
            subsystem_policy_t* policy = &g_policies[s];
            
            if (policy->priority == priority && policy->min_cores > 0) {
                // Try to reserve minimum cores for this subsystem
                uint32_t reserved = 0;
                core_allocation_t* alloc = g_allocations;
                
                while (alloc && reserved < policy->min_cores) {
                    if (alloc->status == CORE_STATUS_FREE) {
                        // Prefer cores on the subsystem's preferred NUMA node
                        if (alloc->numa_node == policy->preferred_numa || 
                            policy->preferred_numa == UINT32_MAX) {
                            alloc->status = CORE_STATUS_RESERVED;
                            alloc->subsystem = (subsystem_id_t)s;
                            reserved++;
                            cores_reserved++;
                        }
                    }
                    alloc = alloc->next;
                }
                
                // If we couldn't get enough on preferred NUMA, take from anywhere
                if (reserved < policy->min_cores) {
                    alloc = g_allocations;
                    while (alloc && reserved < policy->min_cores) {
                        if (alloc->status == CORE_STATUS_FREE) {
                            alloc->status = CORE_STATUS_RESERVED;
                            alloc->subsystem = (subsystem_id_t)s;
                            reserved++;
                            cores_reserved++;
                        }
                        alloc = alloc->next;
                    }
                }
            }
        }
    }
    
    g_stats.reserved_cores = cores_reserved;
    // Available = FREE + RESERVED (not actively allocated)
    update_available_cores_count();
}

/**
 * Update available cores count
 */
static void update_available_cores_count(void) {
    uint32_t available = 0;
    core_allocation_t* alloc = g_allocations;
    
    while (alloc) {
        if (alloc->status == CORE_STATUS_FREE || 
            alloc->status == CORE_STATUS_RESERVED) {
            available++;
        }
        alloc = alloc->next;
    }
    
    g_stats.available_cores = available;
}

/**
 * Allocate cores for a subsystem
 */
core_response_t core_request_allocate(core_request_t* request) {
    core_response_t response = {0};
    
    if (!request || !g_initialized) {
        response.success = false;
        strcpy(response.error_message, "Invalid request or not initialized");
        return response;
    }
    
    if (request->subsystem >= SUBSYSTEM_MAX) {
        response.success = false;
        strcpy(response.error_message, "Invalid subsystem ID");
        return response;
    }
    
    subsystem_policy_t* policy = &g_policies[request->subsystem];
    
    // Check if request exceeds maximum
    uint32_t current_allocated = core_get_allocated_count(request->subsystem);
    if (current_allocated + request->core_count > policy->max_cores) {
        response.success = false;
        strcpy(response.error_message, "Exceeds maximum cores for subsystem");
        return response;
    }
    
    uint32_t allocated = 0;
    bool exclusive = (request->flags & CORE_ALLOC_EXCLUSIVE) != 0;
    bool prefer_numa = (request->flags & CORE_ALLOC_PREFER_NUMA) != 0;
    
    // First pass: try reserved cores for this subsystem
    core_allocation_t* alloc = g_allocations;
    while (alloc && allocated < request->core_count) {
        if (alloc->subsystem == request->subsystem && 
            alloc->status == CORE_STATUS_RESERVED) {
            
            alloc->status = CORE_STATUS_ALLOCATED;
            alloc->allocated_time = g_stats.total_allocations;
            alloc->flags = request->flags;
            response.core_ids[allocated++] = alloc->core_id;
        }
        alloc = alloc->next;
    }
    
    // Second pass: try free cores on preferred NUMA
    if (allocated < request->core_count && prefer_numa) {
        alloc = g_allocations;
        while (alloc && allocated < request->core_count) {
            if (alloc->status == CORE_STATUS_FREE && 
                alloc->numa_node == request->preferred_numa) {
                
                alloc->status = CORE_STATUS_ALLOCATED;
                alloc->subsystem = request->subsystem;
                alloc->allocated_time = g_stats.total_allocations;
                alloc->flags = request->flags;
                response.core_ids[allocated++] = alloc->core_id;
            }
            alloc = alloc->next;
        }
    }
    
    // Third pass: try any free cores
    if (allocated < request->core_count) {
        alloc = g_allocations;
        while (alloc && allocated < request->core_count) {
            if (alloc->status == CORE_STATUS_FREE) {
                alloc->status = CORE_STATUS_ALLOCATED;
                alloc->subsystem = request->subsystem;
                alloc->allocated_time = g_stats.total_allocations;
                alloc->flags = request->flags;
                response.core_ids[allocated++] = alloc->core_id;
            }
            alloc = alloc->next;
        }
    }
    
    // Fourth pass: try shared cores if allowed
    if (allocated < request->core_count && !exclusive && policy->allow_sharing) {
        alloc = g_allocations;
        while (alloc && allocated < request->core_count) {
            if (alloc->status == CORE_STATUS_ALLOCATED || 
                alloc->status == CORE_STATUS_SHARED) {
                
                // Check if the owning subsystem allows sharing
                subsystem_policy_t* owner_policy = &g_policies[alloc->subsystem];
                if (owner_policy->allow_sharing && alloc->share_count < 4) {
                    // Add to sharing list
                    alloc->sharing_with[alloc->share_count++] = request->subsystem;
                    alloc->status = CORE_STATUS_SHARED;
                    response.core_ids[allocated++] = alloc->core_id;
                    g_stats.sharing_events++;
                }
            }
            alloc = alloc->next;
        }
    }
    
    response.success = (allocated > 0);
    response.cores_allocated = allocated;
    
    if (allocated > 0) {
        g_stats.subsystem_cores[request->subsystem] += allocated;
        g_stats.allocated_cores += allocated;
        g_stats.total_allocations++;
        g_stats.subsystem_requests[request->subsystem]++;
        update_available_cores_count();
    } else {
        strcpy(response.error_message, "No cores available");
        g_stats.subsystem_failures[request->subsystem]++;
    }
    
    return response;
}

/**
 * Release a specific core
 */
bool core_release(subsystem_id_t subsystem, uint32_t core_id) {
    if (subsystem >= SUBSYSTEM_MAX || !g_initialized) return false;
    
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->core_id == core_id) {
            if (alloc->subsystem == subsystem) {
                // Check if this is a shared core
                if (alloc->status == CORE_STATUS_SHARED) {
                    // Remove from sharing list
                    for (uint32_t i = 0; i < alloc->share_count; i++) {
                        if (alloc->sharing_with[i] == subsystem) {
                            // Shift remaining entries
                            for (uint32_t j = i; j < alloc->share_count - 1; j++) {
                                alloc->sharing_with[j] = alloc->sharing_with[j + 1];
                            }
                            alloc->share_count--;
                            
                            // If no more sharing, revert to allocated
                            if (alloc->share_count == 0) {
                                alloc->status = CORE_STATUS_ALLOCATED;
                            }
                            
                            g_stats.subsystem_cores[subsystem]--;
                            g_stats.total_deallocations++;
                            return true;
                        }
                    }
                }
                
                // Regular release
                subsystem_policy_t* policy = &g_policies[subsystem];
                
                // If this core is part of minimum guarantee, keep it reserved
                uint32_t current_reserved = 0;
                core_allocation_t* check = g_allocations;
                while (check) {
                    if (check->subsystem == subsystem && 
                        check->status == CORE_STATUS_RESERVED) {
                        current_reserved++;
                    }
                    check = check->next;
                }
                
                if (current_reserved < policy->min_cores) {
                    alloc->status = CORE_STATUS_RESERVED;
                } else {
                    alloc->status = CORE_STATUS_FREE;
                    alloc->subsystem = SUBSYSTEM_MAX;
                }
                
                g_stats.subsystem_cores[subsystem]--;
                g_stats.allocated_cores--;
                g_stats.total_deallocations++;
                update_available_cores_count();
                return true;
            }
            return false; // Core belongs to different subsystem
        }
        alloc = alloc->next;
    }
    
    return false; // Core not found
}

/**
 * Release all cores for a subsystem
 */
bool core_release_all(subsystem_id_t subsystem) {
    if (subsystem >= SUBSYSTEM_MAX || !g_initialized) return false;
    
    uint32_t released = 0;
    core_allocation_t* alloc = g_allocations;
    
    while (alloc) {
        if (alloc->subsystem == subsystem && 
            alloc->status == CORE_STATUS_ALLOCATED) {
            
            subsystem_policy_t* policy = &g_policies[subsystem];
            
            // Keep minimum cores reserved
            if (released < policy->min_cores) {
                alloc->status = CORE_STATUS_RESERVED;
            } else {
                alloc->status = CORE_STATUS_FREE;
                alloc->subsystem = SUBSYSTEM_MAX;
            }
            
            released++;
        }
        alloc = alloc->next;
    }
    
    if (released > 0) {
        g_stats.subsystem_cores[subsystem] = 0;
        g_stats.allocated_cores -= released;
        g_stats.total_deallocations += released;
        update_available_cores_count();
    }
    
    return released > 0;
}

/**
 * Pin a task to a specific core
 */
bool core_pin_task(uint32_t core_id, void (*function)(void*), void* data) {
    if (!g_initialized || !function) return false;
    
    // Find the core and check if it's available or allocated
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->core_id == core_id) {
            if (alloc->status == CORE_STATUS_ALLOCATED || 
                alloc->status == CORE_STATUS_SHARED) {
                
                // Create a parallel task and pin it to this core
                parallel_task_t* task = parallel_task_create("pinned_task", function, data, 0);
                if (task) {
                    task->assigned_core = core_id;
                    parallel_task_submit(task);
                    alloc->task_count++;
                    return true;
                }
            }
            return false;
        }
        alloc = alloc->next;
    }
    
    return false;
}

/**
 * Pin task to any core owned by subsystem
 */
bool core_pin_task_subsystem(subsystem_id_t subsystem, void (*function)(void*), void* data) {
    if (subsystem >= SUBSYSTEM_MAX || !g_initialized || !function) return false;
    
    // Find first allocated core for this subsystem
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->subsystem == subsystem && 
            alloc->status == CORE_STATUS_ALLOCATED) {
            
            return core_pin_task(alloc->core_id, function, data);
        }
        alloc = alloc->next;
    }
    
    return false;
}

/**
 * Get available core count for subsystem
 */
uint32_t core_get_available_count(subsystem_id_t subsystem) {
    if (subsystem >= SUBSYSTEM_MAX) return 0;
    
    subsystem_policy_t* policy = &g_policies[subsystem];
    uint32_t current = core_get_allocated_count(subsystem);
    
    return (policy->max_cores > current) ? (policy->max_cores - current) : 0;
}

/**
 * Get allocated core count for subsystem
 */
uint32_t core_get_allocated_count(subsystem_id_t subsystem) {
    if (subsystem >= SUBSYSTEM_MAX) return 0;
    return g_stats.subsystem_cores[subsystem];
}

/**
 * Check if a core is available
 */
bool core_is_available(uint32_t core_id) {
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->core_id == core_id) {
            return (alloc->status == CORE_STATUS_FREE);
        }
        alloc = alloc->next;
    }
    return false;
}

/**
 * Get owner of a core
 */
subsystem_id_t core_get_owner(uint32_t core_id) {
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->core_id == core_id) {
            return alloc->subsystem;
        }
        alloc = alloc->next;
    }
    return SUBSYSTEM_MAX;
}

/**
 * Get core load percentage
 */
uint32_t core_get_load(uint32_t core_id) {
    // Delegate to parallel engine
    return calculate_core_load(core_id);
}

/**
 * Get NUMA node for core
 */
uint32_t core_get_numa_node(uint32_t core_id) {
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        if (alloc->core_id == core_id) {
            return alloc->numa_node;
        }
        alloc = alloc->next;
    }
    return 0;
}

/**
 * Get statistics
 */
core_manager_stats_t* core_manager_get_stats(void) {
    return &g_stats;
}

/**
 * Print allocation map
 */
void core_manager_print_allocation_map(void) {
    gfx_print("=== Core Allocation Map ===\n");
    
    core_allocation_t* alloc = g_allocations;
    while (alloc) {
        gfx_print("Core ");
        gfx_print_hex(alloc->core_id);
        gfx_print(": ");
        gfx_print(core_status_to_string(alloc->status));
        gfx_print(" - ");
        gfx_print(subsystem_id_to_string(alloc->subsystem));
        gfx_print("\n");
        alloc = alloc->next;
    }
}

/**
 * Subsystem ID to string
 */
const char* subsystem_id_to_string(subsystem_id_t subsystem) {
    static const char* names[] = {
        "Kernel", "AI", "Quantum", "Parallel", 
        "Security", "Video", "I/O", "Network", "None"
    };
    
    if (subsystem >= SUBSYSTEM_MAX) return names[SUBSYSTEM_MAX];
    return names[subsystem];
}

/**
 * Core status to string
 */
const char* core_status_to_string(core_status_t status) {
    static const char* statuses[] = {
        "FREE", "RESERVED", "ALLOCATED", "SHARED", "OFFLINE"
    };
    
    if (status > CORE_STATUS_OFFLINE) return "UNKNOWN";
    return statuses[status];
}
