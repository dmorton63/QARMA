#include <drivers/usb/xhci/xhci_mem.h>
#include <memory/vmm.h>
#include <memory/allocators/dma_allocator.h>
#include <serial/serial.h>
#include "xhci_mem.h"

uintptr_t xhci_map_mmio(uint64_t pci_bar_address, uint32_t bar_size)
{
    size_t page_count = bar_size / PAGESIZE;
    void* vbase = vmm::map_contigous_physical_pages(pci_bar_address, page_count,DEFAULT_PRIV_PAGE_FLAGS | PTE_PCD);
    return uintptr_t();
}

void *alloc_xhci_memory(size_t size, size_t alignment, size_t boundary)
{
    if(size == 0) {
        serial::printf("Attempted xhci DMA allocatiion with size 0\n");
        while(true);
    }

    if(alignmment == 0) {
        serial::printf("Attempted xhci DMA allocation with alignment 0\n");
        while(true);
    }

    if(boundry == 0) {
        serial::printf("Attempted xhci DMA allocation with boundary 0\n");
        while(true);
    }

    auto& dma_allocator = memory::dma_allocator::get();
    void* memblock = dma.allocator.allocate(size, alignment, boundary);
    
}

void free_xhci_memory(void *ptr)
{
}

uintptr_t xhci_get_physical_addr(void *vaddr)
{
    return uintptr_t();
}
