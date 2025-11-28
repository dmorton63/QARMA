/**
 * @file heap.c
 * @brief Simple bump allocator heap for QARMA OS
 * 
 * Provides a static 20MB heap using a bump allocator strategy.
 * Memory is never freed (no-op free), suitable for long-lived allocations.
 * Used for USB structures, graphics buffers, and other permanent allocations.
 */

#include "memory/heap.h"
#include "memory/vmm/vmm.h"
#include "memory.h"
#include "config.h"
#include "spinlock.h"

// ============================================================================
// Configuration
// ============================================================================

#define STATIC_HEAP_SIZE    (20 * 1024 * 1024)  // 20MB static heap
#define MIN_ALIGNMENT       8                    // Minimum 8-byte alignment
#define MAX_ALIGNMENT       (64 * 1024)          // Maximum alignment (64KB)
#define LARGE_ALLOC_THRESHOLD (1024 * 1024)      // 1MB threshold for logging
#define HUGE_ALLOC_THRESHOLD (4 * 1024 * 1024)   // 4MB threshold for VMM redirect

// ============================================================================
// Static Data
// ============================================================================

// Page-aligned static heap (20MB in .bss section)
static uint8_t static_heap[STATIC_HEAP_SIZE] __attribute__((aligned(4096)));

// Heap management state
static uint8_t* heap_base = NULL;
static uint8_t* heap_top  = NULL;
static bool heap_initialized = false;

// Concurrency protection
static spinlock_t heap_lock = SPINLOCK_INIT;

// DMA safety: track if heap is identity-mapped
static bool heap_dma_safe = false;

// Statistics for debugging
static size_t total_allocated = 0;
static size_t total_freed = 0;  // Always 0 since free is no-op
static size_t allocation_count = 0;
static size_t largest_allocation = 0;
static size_t dma_allocation_count = 0;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if value is power of 2
 */
static inline bool is_power_of_2(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Get available heap space
 */
static inline size_t heap_available(void) {
    if (!heap_initialized) {
        return STATIC_HEAP_SIZE;
    }
    return (heap_base + STATIC_HEAP_SIZE) - heap_top;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize the heap allocator
 * 
 * Must be called before any allocations. Safe to call multiple times.
 */
void heap_init(void) {
    if (heap_initialized) {
        return;
    }
    
    heap_base = static_heap;
    heap_top  = heap_base;
    heap_initialized = true;
    
    // Initialize statistics
    total_allocated = 0;
    total_freed = 0;
    allocation_count = 0;
    largest_allocation = 0;
    dma_allocation_count = 0;
    
    // Verify heap is DMA-safe (identity-mapped)
    uint64_t phys = virt_to_phys(heap_base);
    if (phys != 0 && phys == (uint64_t)(uint32_t)heap_base) {
        heap_dma_safe = true;
        SERIAL_LOG("Heap: DMA-safe (identity-mapped)\n");
    } else {
        heap_dma_safe = false;
        SERIAL_LOG("Heap: WARNING - Not identity-mapped, DMA allocations may fail\n");
    }
    
    SERIAL_LOG("Heap: Initialized 20MB static heap at virt=");
    SERIAL_LOG_HEX("", (uint32_t)heap_base);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", (uint32_t)phys);
    SERIAL_LOG("\n");
}

/**
 * @brief Allocate memory from heap (8-byte aligned)
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * Allocates zero-initialized memory. Returns NULL if:
 * - size is 0
 * - Heap is exhausted
 */
void* heap_alloc(size_t size) {
    // Validate input
    if (size == 0) {
        SERIAL_LOG("Heap: WARNING - Attempted to allocate 0 bytes\n");
        return NULL;
    }
    
    // Auto-initialize if needed (safe for early boot)
    if (!heap_initialized) {
        heap_init();
    }
    
    // Align size to minimum alignment (8 bytes)
    size_t aligned_size = (size + (MIN_ALIGNMENT - 1)) & ~(MIN_ALIGNMENT - 1);
    
    // Check for overflow
    if (aligned_size < size) {
        SERIAL_LOG("Heap: ERROR - Size overflow in allocation\n");
        return NULL;
    }
    
    // Acquire lock for thread-safe allocation
    spin_lock(&heap_lock);
    
    // Check available space
    if (heap_top + aligned_size > heap_base + STATIC_HEAP_SIZE) {
        spin_unlock(&heap_lock);
        SERIAL_LOG("Heap: ERROR - Out of heap space! Requested: ");
        SERIAL_LOG_HEX("", aligned_size);
        SERIAL_LOG(" Available: ");
        SERIAL_LOG_HEX("", heap_available());
        SERIAL_LOG("\n");
        return NULL;
    }
    
    // Allocate and update pointer
    void* result = heap_top;
    heap_top += aligned_size;
    
    // Release lock early (before zeroing)
    spin_unlock(&heap_lock);
    
    // Zero the memory (use efficient memset)
    memset(result, 0, aligned_size);
    
    // Update statistics
    total_allocated += aligned_size;
    allocation_count++;
    if (aligned_size > largest_allocation) {
        largest_allocation = aligned_size;
    }
    
    // Log large allocations for debugging
    if (size >= LARGE_ALLOC_THRESHOLD) {
        SERIAL_LOG("Heap: Large allocation - Size: ");
        SERIAL_LOG_HEX("", size);
        SERIAL_LOG(" Addr: ");
        SERIAL_LOG_HEX("", (uint32_t)result);
        SERIAL_LOG(" Remaining: ");
        SERIAL_LOG_HEX("", heap_available());
        SERIAL_LOG("\n");
    }
    
    return result;
}

/**
 * @brief Free heap memory (NO-OP)
 * @param ptr Pointer to memory to free (ignored)
 * 
 * This is a bump allocator - memory is never reclaimed.
 * Suitable for long-lived allocations only.
 */
void heap_free(void* ptr) {
    // No-op: bump allocator doesn't support freeing
    // Memory remains allocated for the lifetime of the system
    (void)ptr;
}

/**
 * @brief Allocate aligned memory from heap
 * @param size Number of bytes to allocate
 * @param alignment Required alignment (must be power of 2)
 * @return Pointer to aligned memory, or NULL on failure
 * 
 * Allocates zero-initialized memory aligned to specified boundary.
 * Returns NULL if:
 * - size is 0
 * - alignment is 0 or not power of 2
 * - alignment > 4096 (page size)
 * - Heap is exhausted
 */
void* heap_alloc_aligned(size_t size, size_t alignment) {
    // Validate inputs
    if (size == 0) {
        SERIAL_LOG("Heap: WARNING - Attempted to allocate 0 bytes (aligned)\n");
        return NULL;
    }
    
    if (alignment == 0 || !is_power_of_2(alignment)) {
        SERIAL_LOG("Heap: ERROR - Invalid alignment: ");
        SERIAL_LOG_HEX("", alignment);
        SERIAL_LOG(" (must be power of 2)\n");
        return NULL;
    }
    
    if (alignment > MAX_ALIGNMENT) {
        SERIAL_LOG("Heap: ERROR - Alignment too large: ");
        SERIAL_LOG_HEX("", alignment);
        SERIAL_LOG(" (max 64KB)\n");
        return NULL;
    }
    
    // Auto-initialize if needed
    if (!heap_initialized) {
        heap_init();
    }
    
    // Acquire lock for thread-safe allocation
    spin_lock(&heap_lock);
    
    // Calculate aligned address
    uintptr_t current = (uintptr_t)heap_top;
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    
    // Check for address overflow
    if (aligned < current) {
        spin_unlock(&heap_lock);
        SERIAL_LOG("Heap: ERROR - Address overflow in alignment\n");
        return NULL;
    }
    
    // Calculate total space needed (padding + size)
    size_t padding = aligned - current;
    size_t total_size = padding + size;
    
    // Check for size overflow
    if (total_size < size) {
        spin_unlock(&heap_lock);
        SERIAL_LOG("Heap: ERROR - Total size overflow\n");
        return NULL;
    }
    
    // Check available space
    if (heap_top + total_size > heap_base + STATIC_HEAP_SIZE) {
        spin_unlock(&heap_lock);
        SERIAL_LOG("Heap: ERROR - Out of heap space (aligned)! Requested: ");
        SERIAL_LOG_HEX("", total_size);
        SERIAL_LOG(" Available: ");
        SERIAL_LOG_HEX("", heap_available());
        SERIAL_LOG("\n");
        return NULL;
    }
    
    // Update heap_top
    heap_top += total_size;
    
    // Release lock early (before zeroing)
    spin_unlock(&heap_lock);
    
    // Zero the allocated memory (use efficient memset)
    memset((void*)aligned, 0, size);
    
    // Update statistics
    total_allocated += total_size;
    allocation_count++;
    if (total_size > largest_allocation) {
        largest_allocation = total_size;
    }
    
    // Log aligned allocations for debugging (important for DMA)
    if (size >= 1024 || alignment >= 64) {
        SERIAL_LOG("Heap: Aligned allocation - Size: ");
        SERIAL_LOG_HEX("", size);
        SERIAL_LOG(" Align: ");
        SERIAL_LOG_HEX("", alignment);
        SERIAL_LOG(" Addr: ");
        SERIAL_LOG_HEX("", aligned);
        SERIAL_LOG("\n");
    }
    
    return (void*)aligned;
}

/**
 * @brief Get heap statistics
 * @param total_alloc Total bytes allocated
 * @param avail Available bytes remaining
 * @param alloc_cnt Number of allocations made
 */
void heap_get_stats(size_t* total_alloc, size_t* avail, size_t* alloc_cnt) {
    if (total_alloc) {
        *total_alloc = total_allocated;
    }
    if (avail) {
        *avail = heap_available();
    }
    if (alloc_cnt) {
        *alloc_cnt = allocation_count;
    }
}

/**
 * @brief Allocate DMA-safe buffer with physical address
 * @param size Size in bytes
 * @param alignment Alignment requirement (power of 2)
 * @return DMA buffer structure with virtual/physical addresses
 * 
 * Returns buffer suitable for DMA operations. Validates physical address.
 * Returns zeroed structure on failure.
 */
dma_buf_t heap_alloc_dma(size_t size, size_t alignment) {
    dma_buf_t result = {0};
    
    // Validate heap is DMA-safe
    if (!heap_initialized) {
        heap_init();
    }
    
    if (!heap_dma_safe) {
        SERIAL_LOG("Heap: ERROR - DMA allocation on non-identity-mapped heap\n");
        return result;
    }
    
    // Allocate aligned buffer
    void* virt = heap_alloc_aligned(size, alignment);
    if (!virt) {
        SERIAL_LOG("Heap: ERROR - DMA allocation failed (heap exhausted)\n");
        return result;
    }
    
    // Get physical address and validate
    uint64_t phys = virt_to_phys(virt);
    if (phys == 0) {
        SERIAL_LOG("Heap: ERROR - DMA allocation: virt_to_phys returned 0 for virt=");
        SERIAL_LOG_HEX("", (uint32_t)virt);
        SERIAL_LOG("\n");
        return (dma_buf_t){0};
    }
    
    // Success - populate result
    result.virt = virt;
    result.phys = phys;
    result.size = size;
    
    dma_allocation_count++;
    
    SERIAL_LOG("Heap: DMA alloc size=");
    SERIAL_LOG_DEC("", size);
    SERIAL_LOG(" align=");
    SERIAL_LOG_DEC("", alignment);
    SERIAL_LOG(" virt=");
    SERIAL_LOG_HEX("", (uint32_t)virt);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", (uint32_t)phys);
    SERIAL_LOG("\n");
    
    return result;
}

/**
 * @brief Allocate DMA-safe pages from VMM
 * @param num_pages Number of 4KB pages
 * @return DMA buffer structure with virtual/physical addresses
 * 
 * For large allocations (>4MB), routes to VMM instead of static heap.
 * Validates physical address translation.
 */
dma_buf_t vmm_alloc_dma_pages(size_t num_pages) {
    dma_buf_t result = {0};
    
    if (num_pages == 0) {
        SERIAL_LOG("VMM: ERROR - Attempted to allocate 0 pages\n");
        return result;
    }
    
    // Allocate from VMM
    void* virt = vmm_alloc_pages(num_pages);
    if (!virt) {
        SERIAL_LOG("VMM: ERROR - DMA page allocation failed\n");
        return result;
    }
    
    // Get physical address and validate
    uint64_t phys = virt_to_phys(virt);
    if (phys == 0) {
        SERIAL_LOG("VMM: ERROR - DMA page allocation: virt_to_phys returned 0 for virt=");
        SERIAL_LOG_HEX("", (uint32_t)virt);
        SERIAL_LOG("\n");
        vmm_free_pages(virt, num_pages);
        return (dma_buf_t){0};
    }
    
    // Success - populate result
    result.virt = virt;
    result.phys = phys;
    result.size = num_pages * 4096;
    
    SERIAL_LOG("VMM: DMA alloc pages=");
    SERIAL_LOG_DEC("", num_pages);
    SERIAL_LOG(" virt=");
    SERIAL_LOG_HEX("", (uint32_t)virt);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", (uint32_t)phys);
    SERIAL_LOG("\n");
    
    return result;
}

/**
 * @brief Print heap statistics to serial
 */
void heap_print_stats(void) {
    if (!heap_initialized) {
        SERIAL_LOG("Heap: Not initialized\n");
        return;
    }
    
    SERIAL_LOG("Heap Statistics:\n");
    SERIAL_LOG("  Total Size: ");
    SERIAL_LOG_HEX("", STATIC_HEAP_SIZE);
    SERIAL_LOG(" (");
    SERIAL_LOG_DEC("", STATIC_HEAP_SIZE / 1024 / 1024);
    SERIAL_LOG("MB)\n");
    
    SERIAL_LOG("  Allocated:  ");
    SERIAL_LOG_HEX("", total_allocated);
    SERIAL_LOG(" (");
    SERIAL_LOG_DEC("", total_allocated / 1024);
    SERIAL_LOG("KB)\n");
    
    SERIAL_LOG("  Available:  ");
    SERIAL_LOG_HEX("", heap_available());
    SERIAL_LOG(" (");
    SERIAL_LOG_DEC("", heap_available() / 1024);
    SERIAL_LOG("KB)\n");
    
    SERIAL_LOG("  Allocations: ");
    SERIAL_LOG_DEC("", allocation_count);
    SERIAL_LOG("\n");
    
    SERIAL_LOG("  DMA Allocs:  ");
    SERIAL_LOG_DEC("", dma_allocation_count);
    SERIAL_LOG("\n");
    
    SERIAL_LOG("  Largest:    ");
    SERIAL_LOG_HEX("", largest_allocation);
    SERIAL_LOG("\n");
    
    uint32_t usage_percent = (total_allocated * 100) / STATIC_HEAP_SIZE;
    SERIAL_LOG("  Usage:      ");
    SERIAL_LOG_DEC("", usage_percent);
    SERIAL_LOG("%\n");
}



