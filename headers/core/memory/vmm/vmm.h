#pragma once

#include "../../stdtools.h"


// Page flags (bitmask values)
#define PAGE_PRESENT  0x001  // Page is present
#define PAGE_WRITE    0x002  // Writable
#define PAGE_USER     0x004  // User-mode accessible
#define PAGE_EXEC     0x008  // Executable (optional, if supported)
#define PAGE_GLOBAL   0x010  // Global page (not flushed from TLB)
#define PAGE_CACHE    0x020  // Enable caching
#define PAGE_NO_CACHE 0x040  // Disable caching


void vmm_init(void);
void* vmm_alloc_pages(size_t num_pages);   // page-aligned allocation
 void  vmm_free_pages(void* addr, size_t num_pages);

void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virtual_addr);
uint32_t vmm_get_physical_address(uint32_t virtual_addr);
void vmm_free_region(uint32_t virtual_addr, uint32_t size);
uint32_t vmm_alloc_region(uint32_t size);
bool vmm_map_framebuffer(uint32_t fb_physical_addr, uint32_t fb_size);
bool vmm_is_initialized(void);
void vmm_ensure_initialized(void);

void enable_paging(uint32_t page_directory_phys_addr);