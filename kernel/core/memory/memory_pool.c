/**
 * QARMA - Memory Pool Manager Implementation
 * 
 * Integrates PMM/VMM with subsystem memory management
 */

#include "memory_pool.h"
#include "../memory.h"
#include "../kernel.h"
#include "graphics/graphics.h"
#include "config.h"
#include "pmm/pmm.h"
#include "heap.h"

// Global memory pools
static memory_pool_t g_pools[SUBSYSTEM_MAX];
static memory_pool_stats_t g_stats = {0};
static bool g_initialized = false;

/**
 * Initialize memory pool manager
 */
void memory_pool_init(void) {
    if (g_initialized) return;
    
    gfx_print("Initializing Memory Pool Manager...\n");
    SERIAL_LOG("=== Memory Pool Manager Init ===\n");
    
    // Initialize all subsystem pools
    for (uint32_t i = 0; i < SUBSYSTEM_MAX; i++) {
        g_pools[i].subsystem = (subsystem_id_t)i;
        g_pools[i].total_allocated = 0;
        g_pools[i].peak_usage = 0;
        g_pools[i].allocation_count = 0;
        g_pools[i].blocks = NULL;
        g_pools[i].enforce_limits = true;
        
        // Set default limits and NUMA preferences
        switch (i) {
            case SUBSYSTEM_KERNEL:
                g_pools[i].max_allocation = 64 * 1024 * 1024;  // 64 MB
                g_pools[i].preferred_numa = 0;
                break;
            case SUBSYSTEM_AI:
                g_pools[i].max_allocation = 256 * 1024 * 1024; // 256 MB
                g_pools[i].preferred_numa = 0;
                break;
            case SUBSYSTEM_QUANTUM:
                g_pools[i].max_allocation = 256 * 1024 * 1024; // 256 MB
                g_pools[i].preferred_numa = 1;
                break;
            case SUBSYSTEM_SECURITY:
                g_pools[i].max_allocation = 32 * 1024 * 1024;  // 32 MB
                g_pools[i].preferred_numa = 0;
                break;
            case SUBSYSTEM_VIDEO:
                g_pools[i].max_allocation = 128 * 1024 * 1024; // 128 MB (framebuffers)
                g_pools[i].preferred_numa = 0;
                break;
            default:
                g_pools[i].max_allocation = 64 * 1024 * 1024;  // 64 MB default
                g_pools[i].preferred_numa = 0;
                break;
        }
    }
    
    // Get PMM stats
    extern void pmm_print_stats(void);  // Will update this to return struct
    
    g_initialized = true;
    gfx_print("Memory Pool Manager initialized.\n");
    
    // Display initial pool configuration
    gfx_print("  VIDEO pool: max ");
    gfx_print_hex(g_pools[SUBSYSTEM_VIDEO].max_allocation / (1024*1024));
    gfx_print(" MB\n");
}

/**
 * Allocate memory from a subsystem pool
 */
void* memory_pool_alloc(subsystem_id_t subsystem, size_t size, uint32_t flags) {
    if (!g_initialized || subsystem >= SUBSYSTEM_MAX) return NULL;
    
    memory_pool_t* pool = &g_pools[subsystem];
    
    // Check limits
    if (pool->enforce_limits && 
        pool->total_allocated + size > pool->max_allocation) {
        return NULL;  // Exceeded pool limit
    }
    
    // For large allocations without proper VMM support, fall back to heap
    // This is a temporary solution until we have proper contiguous physical allocation
    void* virtual_addr = NULL;
    uint32_t physical_addr = 0;
    
    if (size > 64 * 1024) {
        // Large allocation - use kernel heap
        SERIAL_LOG("Memory Pool: Attempting large allocation via heap: ");
        char size_buf[16];
        uint32_t temp = size / 1024;
        int idx = 0;
        if (temp == 0) size_buf[idx++] = '0';
        else {
            char digits[16];
            int d = 0;
            while (temp > 0) { digits[d++] = '0' + (temp % 10); temp /= 10; }
            for (int i = d-1; i >= 0; i--) size_buf[idx++] = digits[i];
        }
        size_buf[idx] = '\0';
        SERIAL_LOG(size_buf);
        SERIAL_LOG(" KB\n");
        
        virtual_addr = heap_alloc(size);
        if (!virtual_addr) {
            SERIAL_LOG("Memory Pool: heap_alloc FAILED for large allocation\n");
            return NULL;
        }
        SERIAL_LOG("Memory Pool: heap_alloc succeeded\n");
        // For heap allocations, physical address is the same (identity mapped)
        physical_addr = (uint32_t)virtual_addr;
    } else {
        // Small allocation - use PMM
        physical_addr = pmm_alloc_page();
        if (physical_addr == 0) {
            SERIAL_LOG("Memory Pool: pmm_alloc_page failed\n");
            return NULL;
        }
        virtual_addr = (void*)physical_addr;
    }
    
    // Zero-initialize if requested
    if (flags & POOL_FLAG_ZERO_INIT) {
        memset(virtual_addr, 0, size);
    }
    
    // Create tracking block
    memory_block_t* block = (memory_block_t*)heap_alloc(sizeof(memory_block_t));
    if (!block) {
        // Cleanup - free the allocated memory
        if (size > 64 * 1024) {
            heap_free(virtual_addr);
        } else {
            pmm_free_page(physical_addr);
        }
        return NULL;
    }
    
    block->virtual_addr = virtual_addr;
    block->physical_addr = physical_addr;
    block->size = size;
    block->from_heap = (size > 64 * 1024); // Track allocation source
    block->flags = flags;
    block->owner = subsystem;
    block->numa_node = pool->preferred_numa;
    
    // Add to pool's block list
    block->next = pool->blocks;
    pool->blocks = block;
    
    // Update stats
    pool->total_allocated += size;
    pool->allocation_count++;
    if (pool->total_allocated > pool->peak_usage) {
        pool->peak_usage = pool->total_allocated;
    }
    
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    g_stats.subsystem_allocated[subsystem] += size;
    g_stats.subsystem_blocks[subsystem]++;
    g_stats.used_physical_pages += pages;
    g_stats.used_virtual_space += size;
    
    return virtual_addr;
}

/**
 * Allocate large buffer (optimized for big allocations like PNG buffers)
 */
void* memory_pool_alloc_large(subsystem_id_t subsystem, size_t size, uint32_t numa_node) {
    (void)numa_node; // Will use later for NUMA optimization
    
    // Large allocations should be contiguous and zero-initialized
    return memory_pool_alloc(subsystem, size, 
                            POOL_FLAG_CONTIGUOUS | POOL_FLAG_ZERO_INIT);
}

/**
 * Free memory from pool
 */
void memory_pool_free(subsystem_id_t subsystem, void* ptr) {
    if (!g_initialized || !ptr || subsystem >= SUBSYSTEM_MAX) return;
    
    memory_pool_t* pool = &g_pools[subsystem];
    memory_block_t* prev = NULL;
    memory_block_t* block = pool->blocks;
    
    // Find the block
    while (block) {
        if (block->virtual_addr == ptr) {
            // Found it, remove from list
            if (prev) {
                prev->next = block->next;
            } else {
                pool->blocks = block->next;
            }
            
            // Calculate pages for stats
            uint32_t pages = (block->size + PAGE_SIZE - 1) / PAGE_SIZE;
            
            // Free memory based on allocation source
            if (block->from_heap) {
                heap_free(block->virtual_addr);
            } else {
                pmm_free_page(block->physical_addr);
            }
            
            // Update stats
            pool->total_allocated -= block->size;
            pool->allocation_count--;
            g_stats.subsystem_allocated[subsystem] -= block->size;
            g_stats.subsystem_blocks[subsystem]--;
            g_stats.used_physical_pages -= pages;
            g_stats.used_virtual_space -= block->size;
            
            // Free the tracking block
            heap_free(block);
            return;
        }
        
        prev = block;
        block = block->next;
    }
}

/**
 * Get allocated size for subsystem
 */
size_t memory_pool_get_allocated(subsystem_id_t subsystem) {
    if (!g_initialized || subsystem >= SUBSYSTEM_MAX) return 0;
    return g_pools[subsystem].total_allocated;
}

/**
 * Get available size for subsystem
 */
size_t memory_pool_get_available(subsystem_id_t subsystem) {
    if (!g_initialized || subsystem >= SUBSYSTEM_MAX) return 0;
    
    memory_pool_t* pool = &g_pools[subsystem];
    if (pool->total_allocated >= pool->max_allocation) return 0;
    
    return pool->max_allocation - pool->total_allocated;
}

/**
 * Get block count for subsystem
 */
uint32_t memory_pool_get_block_count(subsystem_id_t subsystem) {
    if (!g_initialized || subsystem >= SUBSYSTEM_MAX) return 0;
    return g_stats.subsystem_blocks[subsystem];
}

/**
 * Find block by pointer
 */
memory_block_t* memory_pool_find_block(void* ptr) {
    if (!g_initialized || !ptr) return NULL;
    
    for (uint32_t i = 0; i < SUBSYSTEM_MAX; i++) {
        memory_block_t* block = g_pools[i].blocks;
        while (block) {
            if (block->virtual_addr == ptr) {
                return block;
            }
            block = block->next;
        }
    }
    
    return NULL;
}

/**
 * Get statistics
 */
memory_pool_stats_t* memory_pool_get_stats(void) {
    return &g_stats;
}

/**
 * Print pool statistics for a subsystem
 */
void memory_pool_print_stats(subsystem_id_t subsystem) {
    if (!g_initialized || subsystem >= SUBSYSTEM_MAX) return;
    
    extern const char* subsystem_id_to_string(subsystem_id_t);
    memory_pool_t* pool = &g_pools[subsystem];
    
    SERIAL_LOG_HEX("[MEMPOOL] Stats - allocated: ", pool->total_allocated / 1024);
    SERIAL_LOG_HEX(" KB, peak: ", pool->peak_usage / 1024);
    SERIAL_LOG(" KB\n");
    
    gfx_print("=== Memory Pool: ");
    gfx_print(subsystem_id_to_string(subsystem));
    gfx_print(" ===\n");
    
    gfx_print("Current: ");
    gfx_print_hex(pool->total_allocated / 1024);
    gfx_print(" KB  Peak: ");
    gfx_print_hex(pool->peak_usage / 1024);
    gfx_print(" KB  Limit: ");
    gfx_print_hex(pool->max_allocation / 1024);
    gfx_print(" KB\n");
    
    gfx_print("Active allocations: ");
    gfx_print_hex(pool->allocation_count);
    gfx_print("  Blocks: ");
    gfx_print_hex(g_stats.subsystem_blocks[subsystem]);
    gfx_print("\n");
}

/**
 * Print all pool statistics
 */
void memory_pool_print_all_stats(void) {
    SERIAL_LOG("[MEMPOOL] print_all_stats called\n");
    gfx_print("=== Memory Pool Manager Statistics ===\n");
    
    gfx_print("Physical pages used: ");
    gfx_print_hex(g_stats.used_physical_pages);
    gfx_print("\nVirtual space used: ");
    gfx_print_hex(g_stats.used_virtual_space / 1024);
    gfx_print(" KB\n\n");
    
    for (uint32_t i = 0; i < SUBSYSTEM_MAX; i++) {
        // Show stats if currently allocated OR if peak usage > 0
        if (g_pools[i].allocation_count > 0 || g_pools[i].peak_usage > 0) {
            SERIAL_LOG("[MEMPOOL] Showing stats for subsystem\n");
            memory_pool_print_stats((subsystem_id_t)i);
            gfx_print("\n");
        }
    }
    SERIAL_LOG("[MEMPOOL] print_all_stats finished\n");
}
