/**
 * @file heap.h
 * @brief Simple bump allocator heap for QARMA OS
 * 
 * Static 20MB heap using bump allocation strategy.
 * Memory is never freed - suitable for long-lived allocations only.
 */

#pragma once

#include "../stdtools.h"

/**
 * @brief DMA buffer with virtual and physical addresses
 */
typedef struct {
    void*    virt;  // Virtual address
    uint64_t phys;  // Physical address (for DMA)
    size_t   size;  // Buffer size in bytes
} dma_buf_t;

/**
 * @brief Initialize the heap allocator
 * 
 * Must be called before first allocation. Safe to call multiple times.
 */
void heap_init(void);

/**
 * @brief Allocate memory from heap (8-byte aligned)
 * @param size Number of bytes to allocate
 * @return Pointer to zero-initialized memory, or NULL on failure
 */
void* heap_alloc(size_t size);

/**
 * @brief Allocate aligned memory from heap
 * @param size Number of bytes to allocate
 * @param alignment Required alignment (must be power of 2, max 4096)
 * @return Pointer to zero-initialized aligned memory, or NULL on failure
 */
void* heap_alloc_aligned(size_t size, size_t alignment);

/**
 * @brief Free heap memory (NO-OP)
 * @param ptr Pointer to memory (ignored)
 * 
 * Note: This is a bump allocator - memory is never reclaimed.
 */
void heap_free(void* ptr);

/**
 * @brief Get heap statistics
 * @param total_alloc Total bytes allocated (can be NULL)
 * @param avail Available bytes remaining (can be NULL)
 * @param alloc_cnt Number of allocations made (can be NULL)
 */
void heap_get_stats(size_t* total_alloc, size_t* avail, size_t* alloc_cnt);

/**
 * @brief Print heap statistics to serial console
 */
void heap_print_stats(void);

/**
 * @brief Allocate DMA-safe buffer with physical address
 * @param size Size in bytes
 * @param alignment Alignment requirement (power of 2, max 64KB)
 * @return DMA buffer with virtual/physical addresses, or zeroed on failure
 * 
 * Validates physical address translation. Suitable for device DMA rings.
 * Example: heap_alloc_dma(4096, 4096) for XHCI command ring.
 */
dma_buf_t heap_alloc_dma(size_t size, size_t alignment);

/**
 * @brief Allocate DMA-safe pages from VMM
 * @param num_pages Number of 4KB pages
 * @return DMA buffer with virtual/physical addresses, or zeroed on failure
 * 
 * For large allocations (scratchpad buffers, large rings).
 * Validates physical address translation.
 */
dma_buf_t vmm_alloc_dma_pages(size_t num_pages);