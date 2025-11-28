#include "overlay.h"
#include "vmm.h"
#include "config.h"

#define MAX_OVERLAYS 32
#define PAGE_SIZE 0x1000

static OverlayRegion overlays[MAX_OVERLAYS];
static int overlay_count = 0;

void overlay_init(void) {
    overlay_count = 0;
}

void* overlay_alloc(const char* name, size_t size, int flags) {
    if (overlay_count >= MAX_OVERLAYS) return NULL;

    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void* base = vmm_alloc_pages(num_pages);
    if (!base) return NULL;

    overlays[overlay_count++] = (OverlayRegion){
        .base = base,
        .size = size,
        .flags = flags,
        .name = name
    };

    return base;
}

void overlay_free(void* addr) {
    for (int i = 0; i < overlay_count; ++i) {
        if (overlays[i].base == addr) {
            size_t num_pages = (overlays[i].size + PAGE_SIZE - 1) / PAGE_SIZE;
            vmm_free_pages(addr, num_pages);

            // Shift overlays down
            for (int j = i; j < overlay_count - 1; ++j) {
                overlays[j] = overlays[j + 1];
            }
            overlay_count--;
            return;
        }
    }
}

bool overlay_protect(void* addr, int flags) {
    for (int i = 0; i < overlay_count; ++i) {
        if (overlays[i].base == addr) {
            overlays[i].flags = flags;
            // TODO: update page table flags if needed
            return true;
        }
    }
    return false;
}

bool overlay_is_valid(void* addr) {
    for (int i = 0; i < overlay_count; ++i) {
        if (overlays[i].base == addr) return true;
    }
    return false;
}

void overlay_debug_dump(void) {
    for (int i = 0; i < overlay_count; ++i) {
        SERIAL_LOG_HEX("Overlay[", i);
        SERIAL_LOG("]: ");
        SERIAL_LOG("Name: ");
        SERIAL_LOG( overlays[i].name ? overlays[i].name : "(unnamed)");
        SERIAL_LOG_HEX(" Base: ", (uint32_t)overlays[i].base);
        SERIAL_LOG_HEX("Size: %x",overlays[i].size);
        SERIAL_LOG_HEX(" Flags: %x",overlays[i].flags);
        SERIAL_LOG("\n");
    }
}

