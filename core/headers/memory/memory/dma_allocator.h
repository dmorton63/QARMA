#ifndef DMA_ALLOCATOR_H
#define DMA_ALLOCATOR_H

#include "core/stdtools.h"

/**
 * DMA Allocator - Manages physically contiguous memory pools for DMA operations
 * 
 * This allocator maintains pre-allocated pools of physically contiguous memory
 * blocks suitable for DMA transfers. It ensures proper alignment and boundary
 * restrictions required by hardware controllers (XHCI, AHCI, etc).
 */

typedef struct {
    size_t block_size;        // Size of each block in this pool
    size_t alignment;         // Alignment requirement (must be >= block_size)
    size_t max_blocks;        // Maximum number of blocks in this pool
    uint64_t phys_base;       // Physical base address (DMA-accessible)
    uintptr_t virt_base;      // Virtual base address (CPU-accessible)
    size_t used_block_count;  // Number of currently allocated blocks
    uint64_t* used_blocks;    // Bitmap tracking allocated blocks (1 bit per block)
} dma_pool_t;

typedef struct {
    dma_pool_t* pools;        // Array of DMA pools
    size_t pool_count;        // Number of pools
    size_t pool_capacity;     // Capacity of pools array
} dma_allocator_t;

/**
 * Initialize the DMA allocator with default pools
 */
void dma_allocator_init(void);

/**
 * Get the global DMA allocator instance
 */
dma_allocator_t* dma_allocator_get(void);

/**
 * Create a new DMA pool with specified parameters
 * 
 * @param block_size Size of each block in bytes
 * @param alignment Alignment requirement in bytes (must be >= block_size)
 * @param max_blocks Maximum number of blocks to allocate
 */
void dma_allocator_create_pool(size_t block_size, size_t alignment, size_t max_blocks);

/**
 * Allocate DMA-safe memory
 * 
 * @param size Size in bytes to allocate
 * @param alignment Alignment requirement in bytes (default: size)
 * @param boundary Boundary restriction - address must not cross this boundary
 * @return Virtual address of allocated memory, or NULL on failure
 */
void* dma_allocator_alloc(size_t size, size_t alignment, size_t boundary);

/**
 * Get physical address for a DMA allocation
 * 
 * @param virt_addr Virtual address returned by dma_allocator_alloc
 * @return Physical address suitable for DMA, or 0 on error
 */
uint64_t dma_allocator_get_phys(void* virt_addr);

/**
 * Free previously allocated DMA memory
 * 
 * @param ptr Virtual address returned by dma_allocator_alloc
 */
void dma_allocator_free(void* ptr);

/**
 * Print debug information about all DMA pools
 */
void dma_allocator_debug(void);

#endif // DMA_ALLOCATOR_H
