/**
 * DMA Allocator Implementation
 * 
 * Provides physically contiguous memory pools for DMA operations.
 * Based on pool-based allocation with bitmap tracking.
 */

#include "dma_allocator.h"
#include "heap.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "../memory.h"
#include "spinlock.h"
#include "config.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

// Global singleton instance
static dma_allocator_t g_dma_allocator = {0};
static spinlock_t g_dma_lock = SPINLOCK_INIT;
static bool g_dma_initialized = false;

dma_allocator_t* dma_allocator_get(void) {
    return &g_dma_allocator;
}

void dma_allocator_init(void) {
    spin_lock(&g_dma_lock);
    
    if (g_dma_initialized) {
        spin_unlock(&g_dma_lock);
        return;
    }
    
    SERIAL_LOG("DMA Allocator: Initializing...\n");
    
    // Initialize allocator structure
    g_dma_allocator.pools = NULL;
    g_dma_allocator.pool_count = 0;
    g_dma_allocator.pool_capacity = 0;
    
    spin_unlock(&g_dma_lock);
    
    // Create default pools (outside lock - create_pool has its own locking)
    dma_allocator_create_pool(64, 64, 16384);           // 1MB: 16384 x 64 byte blocks
    dma_allocator_create_pool(256, 256, 4096);          // 1MB: 4096 x 256 byte blocks
    dma_allocator_create_pool(1024, 1024, 2048);        // 2MB: 2048 x 1KB blocks
    dma_allocator_create_pool(4096, 4096, 1024);        // 4MB: 1024 x 4KB blocks
    dma_allocator_create_pool(64 * 1024, 64 * 1024, 64); // 4MB: 64 x 64KB blocks
    
    g_dma_initialized = true;
    
    SERIAL_LOG("DMA Allocator: Initialized with 5 default pools\n");
}

void dma_allocator_create_pool(size_t block_size, size_t alignment, size_t max_blocks) {
    spin_lock(&g_dma_lock);
    
    // Ensure alignment is at least as large as the block size
    if (alignment < block_size) {
        alignment = block_size;
    }
    
    // Calculate total size required for the pool
    size_t pool_size = block_size * max_blocks;
    
    // Calculate number of pages needed (with extra for alignment)
    size_t alignment_pages = (alignment + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t pool_pages = (pool_size + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t total_pages = pool_pages + alignment_pages;
    
    // Allocate contiguous physical pages
    uint32_t phys_base_alloc = 0;
    void* virt_base_alloc = NULL;
    
    // Try to allocate contiguous physical memory
    for (size_t i = 0; i < total_pages; i++) {
        uint32_t page = pmm_alloc_page();
        if (page == 0) {
            SERIAL_LOG("DMA pool creation failed: PMM allocation failed\n");
            // TODO: Free previously allocated pages
            spin_unlock(&g_dma_lock);
            return;
        }
        
        if (i == 0) {
            phys_base_alloc = page;
            virt_base_alloc = (void*)page;  // Identity-mapped for now
        } else {
            // Verify contiguity
            if (page != phys_base_alloc + (i * PAGE_SIZE)) {
                SERIAL_LOG("DMA pool creation failed: Non-contiguous physical memory\n");
                // TODO: Free allocated pages
                spin_unlock(&g_dma_lock);
                return;
            }
        }
    }
    
    // Find aligned physical base within allocated range
    uint32_t aligned_phys_base = (uint32_t)phys_base_alloc;
    if (aligned_phys_base % alignment != 0) {
        aligned_phys_base += alignment - (aligned_phys_base % alignment);
    }
    
    // Verify alignment fits within allocated range
    if (aligned_phys_base + pool_size > phys_base_alloc + (total_pages * PAGE_SIZE)) {
        SERIAL_LOG("DMA pool creation failed: Alignment requirement not met\n");
        // TODO: Free allocated pages
        spin_unlock(&g_dma_lock);
        return;
    }
    
    // Calculate aligned virtual base (same offset from base as physical)
    uintptr_t virt_offset = aligned_phys_base - phys_base_alloc;
    uintptr_t aligned_virt_base = (uintptr_t)virt_base_alloc + virt_offset;
    
    // Allocate bitmap for tracking used blocks
    size_t bitmap_size = (max_blocks + 63) / 64;  // 1 bit per block, packed in uint64_t
    uint64_t* used_blocks = (uint64_t*)heap_alloc(bitmap_size * sizeof(uint64_t));
    if (!used_blocks) {
        SERIAL_LOG("DMA pool creation failed: Bitmap allocation failed\n");
        // TODO: Free allocated pages
        spin_unlock(&g_dma_lock);
        return;
    }
    
    // Initialize bitmap to zero (all blocks free)
    for (size_t i = 0; i < bitmap_size; i++) {
        used_blocks[i] = 0;
    }
    
    // Expand pools array if needed
    if (g_dma_allocator.pool_count >= g_dma_allocator.pool_capacity) {
        size_t new_capacity = g_dma_allocator.pool_capacity == 0 ? 8 : g_dma_allocator.pool_capacity * 2;
        dma_pool_t* new_pools = (dma_pool_t*)heap_alloc(new_capacity * sizeof(dma_pool_t));
        
        if (!new_pools) {
            SERIAL_LOG("DMA pool creation failed: Pool array allocation failed\n");
            heap_free(used_blocks);
            spin_unlock(&g_dma_lock);
            return;
        }
        
        // Copy existing pools
        for (size_t i = 0; i < g_dma_allocator.pool_count; i++) {
            new_pools[i] = g_dma_allocator.pools[i];
        }
        
        // Free old array and update
        if (g_dma_allocator.pools) {
            heap_free(g_dma_allocator.pools);
        }
        g_dma_allocator.pools = new_pools;
        g_dma_allocator.pool_capacity = new_capacity;
    }
    
    // Add new pool
    dma_pool_t* pool = &g_dma_allocator.pools[g_dma_allocator.pool_count++];
    pool->block_size = block_size;
    pool->alignment = alignment;
    pool->max_blocks = max_blocks;
    pool->phys_base = aligned_phys_base;
    pool->virt_base = aligned_virt_base;
    pool->used_block_count = 0;
    pool->used_blocks = used_blocks;
    
    spin_unlock(&g_dma_lock);
    
    SERIAL_LOG("DMA pool created: block_size=");
    SERIAL_LOG_HEX("", block_size);
    SERIAL_LOG(" alignment=");
    SERIAL_LOG_HEX("", alignment);
    SERIAL_LOG(" max_blocks=");
    SERIAL_LOG_DEC("", max_blocks);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", aligned_phys_base);
    SERIAL_LOG(" virt=");
    SERIAL_LOG_HEX("", aligned_virt_base);
    SERIAL_LOG("\n");
}

void* dma_allocator_alloc(size_t size, size_t alignment, size_t boundary) {
    spin_lock(&g_dma_lock);
    
    if (alignment < size) {
        alignment = size;
    }
    
    // Default boundary if not specified
    if (boundary == 0) {
        boundary = 0x10000;  // 64KB default
    }
    
    // Iterate over pools in reverse order (larger blocks first)
    for (size_t i = g_dma_allocator.pool_count; i > 0; i--) {
        dma_pool_t* pool = &g_dma_allocator.pools[i - 1];
        
        // Skip if pool block size or alignment is insufficient
        if (size > pool->block_size || alignment > pool->alignment) {
            continue;
        }
        
        // Search for free block
        for (size_t j = 0; j < pool->max_blocks; j++) {
            size_t bitmap_index = j / 64;
            size_t bit_index = j % 64;
            
            // Check if block is free
            if ((pool->used_blocks[bitmap_index] & (1ULL << bit_index)) == 0) {
                uint32_t block_phys = (uint32_t)(pool->phys_base + (j * pool->block_size));
                uintptr_t block_virt = pool->virt_base + (j * pool->block_size);
                
                // Check alignment
                if (block_phys % alignment != 0) {
                    continue;
                }
                
                // Check boundary restriction (block must not cross boundary)
                if (boundary != 0) {
                    uint32_t block_end = block_phys + size - 1;
                    if ((block_phys / boundary) != (block_end / boundary)) {
                        continue;  // Crosses boundary
                    }
                }
                
                // Mark block as used
                pool->used_blocks[bitmap_index] |= (1ULL << bit_index);
                pool->used_block_count++;
                
                spin_unlock(&g_dma_lock);
                return (void*)block_virt;
            }
        }
    }
    
    spin_unlock(&g_dma_lock);
    
    SERIAL_LOG("DMA allocation failed: size=");
    SERIAL_LOG_HEX("", size);
    SERIAL_LOG(" alignment=");
    SERIAL_LOG_HEX("", alignment);
    SERIAL_LOG(" boundary=");
    SERIAL_LOG_HEX("", boundary);
    SERIAL_LOG("\n");
    
    return NULL;
}

uint64_t dma_allocator_get_phys(void* virt_addr) {
    if (!virt_addr) return 0;
    
    uintptr_t virt = (uintptr_t)virt_addr;
    
    spin_lock(&g_dma_lock);
    
    for (size_t i = 0; i < g_dma_allocator.pool_count; i++) {
        dma_pool_t* pool = &g_dma_allocator.pools[i];
        
        uintptr_t pool_start = pool->virt_base;
        uintptr_t pool_end = pool->virt_base + (pool->block_size * pool->max_blocks);
        
        if (virt >= pool_start && virt < pool_end) {
            // Calculate offset within pool
            uintptr_t offset = virt - pool->virt_base;
            uint64_t phys = pool->phys_base + offset;
            
            spin_unlock(&g_dma_lock);
            return phys;
        }
    }
    
    spin_unlock(&g_dma_lock);
    return 0;
}

void dma_allocator_free(void* ptr) {
    if (!ptr) return;
    
    uintptr_t virt_addr = (uintptr_t)ptr;
    
    spin_lock(&g_dma_lock);
    
    for (size_t i = 0; i < g_dma_allocator.pool_count; i++) {
        dma_pool_t* pool = &g_dma_allocator.pools[i];
        
        uintptr_t pool_start = pool->virt_base;
        uintptr_t pool_end = pool->virt_base + (pool->block_size * pool->max_blocks);
        
        if (virt_addr >= pool_start && virt_addr < pool_end) {
            size_t block_index = (virt_addr - pool->virt_base) / pool->block_size;
            size_t bitmap_index = block_index / 64;
            size_t bit_index = block_index % 64;
            
            // Check if block is actually allocated
            if (pool->used_blocks[bitmap_index] & (1ULL << bit_index)) {
                // Mark as free
                pool->used_blocks[bitmap_index] &= ~(1ULL << bit_index);
                pool->used_block_count--;
                
                spin_unlock(&g_dma_lock);
                return;
            } else {
                SERIAL_LOG("DMA free failed: Block already free\n");
                spin_unlock(&g_dma_lock);
                return;
            }
        }
    }
    
    spin_unlock(&g_dma_lock);
    SERIAL_LOG("DMA free failed: Invalid address ");
    SERIAL_LOG_HEX("", virt_addr);
    SERIAL_LOG("\n");
}

void dma_allocator_debug(void) {
    spin_lock(&g_dma_lock);
    
    SERIAL_LOG("=== DMA Allocator Debug ===\n");
    SERIAL_LOG("Total pools: ");
    SERIAL_LOG_DEC("", g_dma_allocator.pool_count);
    SERIAL_LOG("\n");
    
    for (size_t i = 0; i < g_dma_allocator.pool_count; i++) {
        dma_pool_t* pool = &g_dma_allocator.pools[i];
        
        SERIAL_LOG("Pool #");
        SERIAL_LOG_DEC("", i);
        SERIAL_LOG(": block_size=");
        SERIAL_LOG_HEX("", pool->block_size);
        SERIAL_LOG(" alignment=");
        SERIAL_LOG_HEX("", pool->alignment);
        SERIAL_LOG(" max=");
        SERIAL_LOG_DEC("", pool->max_blocks);
        SERIAL_LOG(" used=");
        SERIAL_LOG_DEC("", pool->used_block_count);
        SERIAL_LOG(" phys=");
        SERIAL_LOG_HEX("", pool->phys_base);
        SERIAL_LOG(" virt=");
        SERIAL_LOG_HEX("", pool->virt_base);
        SERIAL_LOG("\n");
    }
    
    spin_unlock(&g_dma_lock);
}
