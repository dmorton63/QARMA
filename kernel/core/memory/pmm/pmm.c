#include "pmm.h"
#include "config.h"
#include "../memory.h"

#define MAX_PHYSICAL_PAGES 32768  // Example: 128MB / 4KB
static uint8_t page_bitmap[MAX_PHYSICAL_PAGES / 8];

static inline void set_bit(uint32_t bit) {
    page_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void clear_bit(uint32_t bit) {
    page_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int test_bit(uint32_t bit) {
    return page_bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(void) {
    memset(page_bitmap, 0xFF, sizeof(page_bitmap)); // Mark all as used
    // Later: mark usable regions from BIOS/multiboot
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < MAX_PHYSICAL_PAGES; ++i) {
        if (!test_bit(i)) {
            set_bit(i);
            return i * 0x1000;
        }
    }
    return 0; // Out of memory
}

void pmm_free_page(uint32_t addr) {
    uint32_t page = addr / 0x1000;
    clear_bit(page);
}

void pmm_mark_region_free(uint32_t start_addr, uint32_t length) {
    uint32_t start_page = start_addr / 0x1000;
    uint32_t num_pages = length / 0x1000;
    for (uint32_t i = 0; i < num_pages; ++i) {
        clear_bit(start_page + i);
    }
}

void pmm_mark_region_used(uint32_t start_addr, uint32_t length) {
    uint32_t start_page = start_addr / 0x1000;
    uint32_t num_pages = length / 0x1000;
    for (uint32_t i = 0; i < num_pages; ++i) {
        set_bit(start_page + i);
    }
}

void pmm_print_stats(void) {
    uint32_t used = 0;
    for (uint32_t i = 0; i < MAX_PHYSICAL_PAGES; ++i) {
        if (test_bit(i)) used++;
    }
    SERIAL_LOG("PMM:");
    SERIAL_LOG_HEX(" %u pages used, ", used);
    SERIAL_LOG_HEX(" %u free",  MAX_PHYSICAL_PAGES - used);
    SERIAL_LOG_HEX(", %u total", MAX_PHYSICAL_PAGES);
}