#include "vmm.h"
#include "config.h"
#include "../memory.h"
#include "../pmm/pmm.h"
//#include "overlay.h"

#define PAGE_SIZE 0x1000
#define IDENTITY_PDE_COUNT 8
#define EARLY_PAGETABLE_POOL 16

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t identity_page_tables[IDENTITY_PDE_COUNT][1024] __attribute__((aligned(4096)));
static uint32_t early_page_tables[EARLY_PAGETABLE_POOL][1024] __attribute__((aligned(4096)));
static uint32_t framebuffer_page_table[1024] __attribute__((aligned(4096)));

static bool paging_enabled = false;
static bool vmm_initialized = false;
static uint32_t vmm_next_virtual_addr = 0xC0000000; // Start virtual allocations at 3GB

static uint32_t early_pt_count = 0;
static bool early_pt_exhausted = false;

uint32_t vmm_get_physical_address(uint32_t vaddr) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;

    uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    if (!(page_directory[pd_index] & PAGE_PRESENT)) return 0;
    if (!(page_table[pt_index] & PAGE_PRESENT)) return 0;

    return page_table[pt_index] & ~0xFFF;
}

void* vmm_alloc_pages(size_t num_pages) {
    if (!vmm_initialized || num_pages == 0) return NULL;

    void* base = (void*)vmm_next_virtual_addr;

    for (size_t i = 0; i < num_pages; ++i) {
        uint32_t phys = pmm_alloc_page();
        if (!phys) return NULL;

        vmm_map_page(vmm_next_virtual_addr, phys, PAGE_PRESENT | PAGE_WRITE);
        vmm_next_virtual_addr += PAGE_SIZE;
    }
    SERIAL_LOG("VMM: Allocated ");
    SERIAL_LOG_HEX("VMM: ALLOCATED Pages: %x",(uint32_t)num_pages);
    SERIAL_LOG_HEX(" Base: %x", base);

    return base;
}

void vmm_free_pages(void* addr, size_t num_pages) {
    if (!vmm_initialized || !addr || num_pages == 0) return;

    uint32_t base = (uint32_t)addr;
    uint32_t size = num_pages * PAGE_SIZE;

    vmm_free_region(base, size);
}  


void vmm_unmap_page(uint32_t virtual_addr)
{
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    // Get page table from page directory
    uint32_t* page_table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        serial_debug("vmm_unmap_page: page directory entry not present");
        return;
    }

    // Clear the page table entry
    page_table[pt_index] = 0;

    // Invalidate the TLB entry for this virtual address
    __asm__ volatile("invlpg (%0)" ::"r"(virtual_addr) : "memory");
}

void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!vmm_initialized) {
        GFX_LOG_MIN("VMM: Cannot map page - VMM not initialized");
        return;
    }

    uint32_t page_dir_idx = virtual_addr >> 22;
    uint32_t page_table_idx = (virtual_addr >> 12) & 0x3FF;
    
    GFX_LOG_HEX("VMM: Mapping virtual ", virtual_addr);
    GFX_LOG_HEX("VMM: to physical ", physical_addr);
    
    uint32_t* page_table;
    if (page_directory[page_dir_idx] & PAGE_PRESENT) {
        page_table = (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
    } else if (!early_pt_exhausted && early_pt_count < EARLY_PAGETABLE_POOL) {
        page_table = early_page_tables[early_pt_count++];
        memset(page_table, 0, PAGE_SIZE);
        page_directory[page_dir_idx] = ((uintptr_t)page_table) | PAGE_PRESENT | PAGE_WRITE | flags;
    } else {
        uint32_t page_table_phys = pmm_alloc_page();
        if (page_table_phys == 0) {
            gfx_print("VMM: Failed to allocate page table\n");
            return;
        }
        page_table = (uint32_t*)page_table_phys;
        memset(page_table, 0, PAGE_SIZE);
        page_directory[page_dir_idx] = page_table_phys | PAGE_PRESENT | PAGE_WRITE | flags;
        early_pt_exhausted = true;
    }

    page_table[page_table_idx] = physical_addr | PAGE_PRESENT | flags;
    
    // Invalidate TLB for this page
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

// Allocate virtual memory region
uint32_t vmm_alloc_region(uint32_t size) {
    if (!vmm_initialized) {
        SERIAL_LOG("VMM: Cannot allocate region - VMM not initialized\n");
        return 0;
    }
    
    if (!paging_enabled) {
        SERIAL_LOG("VMM: Cannot allocate region - paging not enabled\n");
        return 0;
    }
    
    if (size == 0) {
        SERIAL_LOG("VMM: Cannot allocate 0-sized region\n");
        return 0;
    }
    
    // Align size to page boundary
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t num_pages = size / PAGE_SIZE;
    
    SERIAL_LOG("VMM: Allocating region of ");
    SERIAL_LOG_HEX("", num_pages);
    SERIAL_LOG(" pages\n");
    
    // Allocate virtual pages
    void* region = vmm_alloc_pages(num_pages);
    if (!region) {
        SERIAL_LOG("VMM: Failed to allocate pages for region\n");
        return 0;
    }
    
    SERIAL_LOG("VMM: Region allocated at ");
    SERIAL_LOG_HEX("", (uint32_t)region);
    SERIAL_LOG("\n");
    
    return (uint32_t)region;
}

// Free virtual memory region
void vmm_free_region(uint32_t virtual_addr, uint32_t size) {
    if (!vmm_initialized) {
        return;
    }
    
    // Align to page boundaries
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t num_pages = size / PAGE_SIZE;
    
    gfx_print("VMM: Freeing virtual region ");
    gfx_print_hex(virtual_addr);
    gfx_print("-");
    gfx_print_hex(virtual_addr + size);
    gfx_print("\n");
    
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virtual_page = virtual_addr + (i * PAGE_SIZE);
        uint32_t page_dir_idx = virtual_page >> 22;
        uint32_t page_table_idx = (virtual_page >> 12) & 0x3FF;
        
        if (page_directory[page_dir_idx] & PAGE_PRESENT) {
            uint32_t* page_table = (uint32_t*)(page_directory[page_dir_idx] & ~0xFFF);
            if (page_table[page_table_idx] & PAGE_PRESENT) {
                uint32_t phys_addr = page_table[page_table_idx] & ~0xFFF;
                pmm_free_page(phys_addr);
                page_table[page_table_idx] = 0;
                
                // Invalidate TLB
                __asm__ volatile("invlpg (%0)" :: "r"(virtual_page) : "memory");
            }
        }
    }
}


void vmm_init(void) {
    if (vmm_initialized) {
        SERIAL_LOG("[vmm_init] already initialized\n");
        return;
    }

    gfx_print("Initializing Virtual Memory Manager...\n");
    
    // Clear page directory
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // Identity-map the first 32MB (loader + kernel + early tables)
    for (int pde = 0; pde < IDENTITY_PDE_COUNT; pde++) {
        for (int entry = 0; entry < 1024; entry++) {
            uint32_t phys_page = (pde * 1024u + entry) * PAGE_SIZE;
            identity_page_tables[pde][entry] = phys_page | PAGE_PRESENT | PAGE_WRITE;
        }
        page_directory[pde] = (uint32_t)identity_page_tables[pde] | PAGE_PRESENT | PAGE_WRITE;
    }

    SERIAL_LOG("[vmm_init] identity mapping ready\n");
    SERIAL_LOG_HEX("PDE[0] = ", page_directory[0]);
    SERIAL_LOG_HEX("PDE[1] = ", page_directory[1]);

        early_pt_count = 0;
        early_pt_exhausted = false;
    
    vmm_initialized = true;
    gfx_print("Virtual memory manager initialized (identity mapped + framebuffer).\n");
    // Print information about virtual allocation space
    GFX_LOG_HEX("Virtual allocation space starts at: ", vmm_next_virtual_addr);
}

bool vmm_is_initialized(void) {
    return vmm_initialized;
}

void vmm_ensure_initialized(void) {
    if (!vmm_initialized) {
        vmm_init();
    }
}

void enable_paging(uint32_t page_directory_phys_addr)
{
    SERIAL_LOG_HEX("Enabling paging with PD at: ", page_directory_phys_addr);
    SERIAL_LOG("\n");
    
    __asm__ volatile (
        "mov %0, %%cr3\n"           // Set page directory
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"  // Set PG bit
        "mov %%eax, %%cr0\n"
        : : "r" (page_directory_phys_addr) : "eax"
    );
    
    paging_enabled = true;
    SERIAL_LOG("Paging enabled.\n");
}

// Map framebuffer memory region
bool vmm_map_framebuffer(uint32_t fb_physical_addr, uint32_t fb_size) {
    SERIAL_LOG_HEX("Mapping framebuffer: phys=", fb_physical_addr);
    SERIAL_LOG_HEX(" size=", fb_size);
    SERIAL_LOG("\n");
    
    // For QEMU, framebuffer is typically in high memory (0xFD000000+)
    // We need to set up page tables to access it
    
    if (fb_physical_addr < 0x100000) {
        SERIAL_LOG("FB address in low memory, already accessible\n");
        return true; // Low memory is already mapped
    }
    
    // Check if address is in QEMU framebuffer range
    if (fb_physical_addr >= 0xE0000000 && fb_physical_addr < 0xFFE00000) {
        // Check if paging is already enabled
        if (paging_enabled) {
            SERIAL_LOG("Paging already enabled, framebuffer should be accessible\n");
            return true;
        }
        
        SERIAL_LOG("Setting up page table for framebuffer...\n");
        
        // Map framebuffer region
        uint32_t fb_page_start = fb_physical_addr >> 12; // Convert to page number
        uint32_t fb_page_count = (fb_size + 0xFFF) >> 12; // Round up to pages
        uint32_t fb_pde_index = fb_physical_addr >> 22; // Page directory entry index
        
        SERIAL_LOG_HEX("FB pages: start=", fb_page_start);
        SERIAL_LOG_HEX(" count=", fb_page_count);
        SERIAL_LOG_HEX(" pde_idx=", fb_pde_index);
        SERIAL_LOG("\n");
        
        // For simplicity, create a page table for the framebuffer region
        // This is a basic implementation - normally you'd allocate page tables dynamically
        static uint32_t fb_page_table[1024] __attribute__((aligned(4096)));
        
        // Map the framebuffer pages
        uint32_t fb_base_page = (fb_physical_addr >> 22) << 10; // Base page in this 4MB region
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t page_addr = ((fb_base_page + i) << 12);
            fb_page_table[i] = page_addr | PAGE_PRESENT | PAGE_WRITE;
        }
        
        // Identity-map the framebuffer region by mapping each page into its own table
        for (uint32_t page = 0; page < fb_page_count; page++) {
            uint32_t phys_addr = fb_physical_addr + (page * PAGE_SIZE);
            vmm_map_page(phys_addr, phys_addr, PAGE_WRITE);
        }

        SERIAL_LOG_HEX("PDE[0] = ", page_directory[0]);
        SERIAL_LOG_HEX("PDE[1] = ", page_directory[1]);
        SERIAL_LOG_HEX("PDE[0x3F4] = ", page_directory[fb_pde_index]);
        SERIAL_LOG("\n");
        
        // Enable paging
        uint32_t page_dir_phys = (uint32_t)page_directory;
        enable_paging(page_dir_phys);

        // __asm__ volatile (
        //     "mov %0, %%cr3\n"           // Set page directory
        //     "mov %%cr0, %%eax\n"
        //     "or $0x80000000, %%eax\n"  // Set PG bit
        //     "mov %%eax, %%cr0\n"
        //     : : "r" (page_dir_phys) : "eax"
        // );
        
        //paging_enabled = true;
        SERIAL_LOG("Paging enabled with framebuffer mapping\n");
        return true;
    }
    
    gfx_print("FB address out of supported range\n");
    return false;
}

// vmm.c

