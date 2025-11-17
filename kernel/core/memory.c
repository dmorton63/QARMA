/**
 * QARMA - Basic Memory Management and String Functions
 */

#include "core/kernel.h"
#include "graphics/graphics.h" 
#include "multiboot.h"
#include "memory.h"
#include "config.h"
#include "memory/pmm/pmm.h"
#include "memory/vmm/vmm.h"
#include "memory/heap.h"
Block* freeList = NULL;

// Forward declarations for serial debug functions
extern void serial_debug(const char* msg);
extern void serial_debug_hex(uint32_t value);

// Page flags
// #define PAGE_PRESENT    0x1
// #define PAGE_WRITE      0x2
// #define PAGE_USER       0x4
// #define PAGE_SIZE       0x1000  // 4KB pages

// // Physical Memory Manager
// #define PMM_MAX_PAGES   32768   // Support up to 128MB (32768 * 4KB)
// #define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8)  // Bitmap for page allocation

#define HEAP_SIZE (1024 * 1024)  // 1MB
static char heap[HEAP_SIZE];
static char* heap_end = heap;



// Simple page directory for basic mapping
// static uint32_t page_directory[1024] __attribute__((aligned(4096)));
// static uint32_t page_table_0[1024] __attribute__((aligned(4096)));

// Simple page directory and table for framebuffer mapping
// static uint32_t page_directory[1024] __attribute__((aligned(4096)));
// static uint32_t framebuffer_page_table[1024] __attribute__((aligned(4096)));
// static bool paging_enabled = false;

// Virtual memory management state
void memory_init(void) {
    pmm_init();
    vmm_init();
    heap_init();
}

void *malloc(size_t size)
{
    Block* curr = freeList;
    while (curr) {
        if (curr->free && curr->size >= size) {
            curr->free = 0;
            return (void*)(curr + 1);
        }
        curr = curr->next;
    }

    Block* newBlock = sbrk(size + BLOCK_SIZE);
    if (newBlock == (void*)-1) return NULL;

    newBlock->size = size;
    newBlock->next = freeList;
    newBlock->free = 0;
    freeList = newBlock;

    return (void*)(newBlock + 1);

}



void free(void *ptr)
{
    if (!ptr) return;
    Block* block = (Block*)ptr - 1;
    block->free = 1;

}


// Simple delay function
void kernel_delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        // Busy wait
    }
}

void* sbrk(int increment) {
    if (heap_end + increment > heap + HEAP_SIZE) return (void*)-1;
    void* prev = heap_end;
    heap_end += increment;
    return prev;
}