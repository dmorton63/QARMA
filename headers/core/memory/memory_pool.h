/**
 * QARMA - Memory Pool Manager
 * 
 * Per-subsystem memory pools with NUMA awareness and PMM/VMM integration
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "kernel_types.h"
#include "../core_manager.h"

// Memory pool flags
typedef enum {
    POOL_FLAG_NUMA_LOCAL    = 0x01,  // Prefer NUMA-local memory
    POOL_FLAG_CONTIGUOUS    = 0x02,  // Require physically contiguous
    POOL_FLAG_CACHEABLE     = 0x04,  // Memory is cacheable
    POOL_FLAG_EXECUTABLE    = 0x08,  // Memory can contain code
    POOL_FLAG_DMA_CAPABLE   = 0x10,  // Suitable for DMA operations
    POOL_FLAG_ZERO_INIT     = 0x20   // Zero-initialize on allocation
} memory_pool_flags_t;

// Memory block tracking
typedef struct memory_block {
    void* virtual_addr;              // Virtual address
    uint32_t physical_addr;          // Physical address
    size_t size;                     // Size in bytes
    uint32_t flags;                  // Allocation flags
    subsystem_id_t owner;            // Owning subsystem
    uint32_t numa_node;              // NUMA node
    bool from_heap;                  // True if from heap, false if from PMM
    
    struct memory_block* next;       // Next block in list
} memory_block_t;

// Per-subsystem memory pool
typedef struct memory_pool {
    subsystem_id_t subsystem;        // Subsystem ID
    size_t total_allocated;          // Total bytes allocated
    size_t peak_usage;               // Peak memory usage
    uint32_t allocation_count;       // Number of allocations
    uint32_t preferred_numa;         // Preferred NUMA node
    
    memory_block_t* blocks;          // Linked list of blocks
    
    // Limits
    size_t max_allocation;           // Maximum pool size
    bool enforce_limits;             // Enforce allocation limits
} memory_pool_t;

// Memory pool statistics
typedef struct {
    size_t total_physical_pages;     // Total physical pages
    size_t used_physical_pages;      // Used physical pages
    size_t total_virtual_space;      // Total virtual address space
    size_t used_virtual_space;       // Used virtual address space
    
    // Per-subsystem stats
    size_t subsystem_allocated[SUBSYSTEM_MAX];
    uint32_t subsystem_blocks[SUBSYSTEM_MAX];
} memory_pool_stats_t;

// Function declarations

// Initialization
void memory_pool_init(void);

// Pool management
memory_pool_t* memory_pool_create(subsystem_id_t subsystem, size_t max_size, uint32_t numa_node);
void memory_pool_destroy(memory_pool_t* pool);

// Memory allocation
void* memory_pool_alloc(subsystem_id_t subsystem, size_t size, uint32_t flags);
void* memory_pool_alloc_aligned(subsystem_id_t subsystem, size_t size, size_t alignment, uint32_t flags);
void* memory_pool_alloc_pages(subsystem_id_t subsystem, uint32_t page_count, uint32_t flags);
void memory_pool_free(subsystem_id_t subsystem, void* ptr);

// Large buffer allocation (for things like PNG decoding)
void* memory_pool_alloc_large(subsystem_id_t subsystem, size_t size, uint32_t numa_node);

// Query functions
size_t memory_pool_get_allocated(subsystem_id_t subsystem);
size_t memory_pool_get_available(subsystem_id_t subsystem);
uint32_t memory_pool_get_block_count(subsystem_id_t subsystem);
memory_block_t* memory_pool_find_block(void* ptr);

// NUMA-aware allocation
void* memory_pool_alloc_numa(subsystem_id_t subsystem, size_t size, uint32_t numa_node, uint32_t flags);

// DMA buffer allocation (physically contiguous)
void* memory_pool_alloc_dma(subsystem_id_t subsystem, size_t size, uint32_t* physical_addr_out);

// Statistics
memory_pool_stats_t* memory_pool_get_stats(void);
void memory_pool_print_stats(subsystem_id_t subsystem);
void memory_pool_print_all_stats(void);

// Utility
const char* memory_pool_flags_to_string(uint32_t flags);

#endif // MEMORY_POOL_H
