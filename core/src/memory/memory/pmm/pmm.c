#include "pmm.h"
#include "config.h"
#include "../memory.h"
#include "core/spinlock.h"
#include "../core/E820.h"   

#define MAX_PHYSICAL_PAGES 32768  // Example: 128MB / 4KB

static uint8_t page_bitmap[MAX_PHYSICAL_PAGES / 8];
static uint32_t pmm_next = 1;            // skip page 0
static spinlock_t pmm_lock = SPINLOCK_INIT;

static inline int bit_is_set(uint32_t bit) { return (page_bitmap[bit / 8] >> (bit % 8)) & 1; }
static inline void bit_set(uint32_t bit)   { page_bitmap[bit / 8] |=  (1u << (bit % 8)); }
static inline void bit_clear(uint32_t bit) { page_bitmap[bit / 8] &= ~(1u << (bit % 8)); }

void pmm_init_from_e820(const e820_entry_t* map, size_t count) {
    memset(page_bitmap, 0xFF, sizeof(page_bitmap)); // default used
    // Mark usable ranges free
    for (size_t i = 0; i < count; i++) {
        if (map[i].type == E820_USABLE) {
            uint64_t addr = map[i].addr;
            uint64_t len  = map[i].len;
            // clamp to allocator range (32-bit phys and MAX_PHYSICAL_PAGES)
            uint64_t max_bytes = (uint64_t)MAX_PHYSICAL_PAGES * 0x1000ull;
            if (addr >= max_bytes) continue;
            if (addr + len > max_bytes) len = max_bytes - addr;
            pmm_mark_region_free((uint32_t)addr, (uint32_t)len);
        }
    }
    // Reserve critical ranges
    pmm_mark_region_used(0, 0x100000); // low 1MB
    // TODO: reserve kernel image, page tables, MMIO BARs, framebuffer
    bit_set(0);         // ensure page 0 reserved
    pmm_next = 1;
}

void pmm_init(void) {
    memset(page_bitmap, 0xFF, sizeof(page_bitmap)); // Mark all as used
    // Later: mark usable regions from BIOS/multiboot
}

uint64_t pmm_alloc_page(void) {
    spin_lock(&pmm_lock);
    if (pmm_next == 0) pmm_next = 1; // never allocate page 0

    for (uint32_t i = pmm_next; i < MAX_PHYSICAL_PAGES; i++) {
        if (!bit_is_set(i)) {
            bit_set(i);
            pmm_next = i + 1;
            uint64_t addr = (uint64_t)i * 0x1000;
            spin_unlock(&pmm_lock);
            return addr;
        }
    }
    // wrap once
    for (uint32_t i = 1; i < pmm_next; i++) {
        if (!bit_is_set(i)) {
            bit_set(i);
            pmm_next = i + 1;
            uint64_t addr = (uint64_t)i * 0x1000;
            spin_unlock(&pmm_lock);
            return addr;
        }
    }
    spin_unlock(&pmm_lock);
    return 0; // OOM
}

void pmm_free_page(uint64_t addr) {
    if ((addr & 0xFFF) != 0) return; // must be page-aligned
    uint64_t page = addr / 0x1000;
    if (page == 0 || page >= MAX_PHYSICAL_PAGES) return;

    spin_lock(&pmm_lock);
    // Optional: debug check to catch double free
    // if (!bit_is_set(page)) { /* log or assert */ }
    bit_clear((uint32_t)page);
    if (page < pmm_next) pmm_next = (uint32_t)page; // improve locality
    spin_unlock(&pmm_lock);
}

uint32_t pmm_alloc_contiguous(uint32_t num_pages, uint32_t align_pages) {
    if (num_pages == 0) return 0;
    if (align_pages == 0) align_pages = 1;

    spin_lock(&pmm_lock);
    uint32_t hint = (pmm_next == 0) ? 1 : pmm_next;
    uint32_t start = ((hint + align_pages - 1) / align_pages) * align_pages;
    if (start == 0) start = align_pages; // avoid 0

    for (uint32_t i = start; i + num_pages <= MAX_PHYSICAL_PAGES; i += align_pages) {
        bool free = true;
        for (uint32_t j = 0; j < num_pages; j++) {
            if (bit_is_set(i + j)) { free = false; break; }
        }
        if (free) {
            for (uint32_t j = 0; j < num_pages; j++) bit_set(i + j);
            pmm_next = i + num_pages;
            uint32_t addr = i * 0x1000;
            spin_unlock(&pmm_lock);
            return addr;
        }
    }
    // Optional: one wrap attempt from 1
    for (uint32_t i = align_pages; i + num_pages <= hint; i += align_pages) {
        bool free = true;
        for (uint32_t j = 0; j < num_pages; j++) {
            if (bit_is_set(i + j)) { free = false; break; }
        }
        if (free) {
            for (uint32_t j = 0; j < num_pages; j++) bit_set(i + j);
            pmm_next = i + num_pages;
            uint32_t addr = i * 0x1000;
            spin_unlock(&pmm_lock);
            return addr;
        }
    }
    spin_unlock(&pmm_lock);
    return 0;
}

void pmm_mark_region_free(uint64_t start_addr, uint64_t length) {
    if (length == 0) return;
    uint64_t start = start_addr;
    uint64_t end   = start_addr + length;
    uint32_t start_page = (uint32_t)(start / 0x1000);
    uint32_t end_page   = (uint32_t)((end + 0xFFF) / 0x1000);

    if (end_page > MAX_PHYSICAL_PAGES) end_page = MAX_PHYSICAL_PAGES;

    for (uint32_t i = start_page; i < end_page; i++) {
        if (i == 0) continue;
        bit_clear(i);
    }
}

void pmm_mark_region_used(uint64_t start_addr, uint64_t length) {
    if (length == 0) return;
    uint64_t start = start_addr;
    uint64_t end   = start_addr + length;
    uint32_t start_page = (uint32_t)(start / 0x1000);
    uint32_t end_page   = (uint32_t)((end + 0xFFF) / 0x1000);

    if (end_page > MAX_PHYSICAL_PAGES) end_page = MAX_PHYSICAL_PAGES;

    for (uint32_t i = start_page; i < end_page; i++) {
        bit_set(i);
    }
}

void pmm_print_stats(void) {
    uint32_t used = 0;
    for (uint32_t i = 0; i < MAX_PHYSICAL_PAGES; ++i) {
        if (bit_is_set(i)) used++;
    }
    SERIAL_LOG("PMM: used=");
    SERIAL_LOG_DEC("", used);
    SERIAL_LOG(" free=");
    SERIAL_LOG_DEC("", MAX_PHYSICAL_PAGES - used);
    SERIAL_LOG(" total=");
    SERIAL_LOG_DEC("", MAX_PHYSICAL_PAGES);
    SERIAL_LOG("\n");
}