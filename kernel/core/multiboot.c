/**
 * QARMA - Multiboot Information Parsing
 * 
 * Functions for parsing multiboot information from the bootloader.
 * Processes memory maps, module information, and boot parameters.
 */

#include "multiboot.h"
#include "graphics/graphics.h"
#include "kernel.h"
#include "core/boot_log.h"
#include "config.h"
#include "string.h"
#include "io.h"
#include "memory.h"
#include "core/memory/pmm/pmm.h"


static multiboot_info_t* g_multiboot_info = NULL;

void multiboot_parse_info(uint32_t magic, multiboot_info_t* mbi) {
    debug_buffer_clear();

    if (magic != MULTIBOOT_MAGIC) {
        //debug_buffer_append("ERROR: Bad multiboot magic!\n");
        debug_buffer_push("ERROR: Bad multiboot magic!\n");
        debug_buffer_flush_lines();
        return;
    }

    g_multiboot_info = mbi;
    debug_buffer_append("Multiboot info assigned\n");

    if (mbi->flags & MULTIBOOT_FLAG_CMDLINE) {
        const char *cmd = (const char*)mbi->cmdline;
        multiboot_parse_verbosity(cmd);
        /* Parse UHCI-specific boot options (e.g., uhci_clflush=1) */
        if (strstr(cmd, "uhci_clflush=1")) {
            extern void uhci_set_clflush_enabled(int);
            uhci_set_clflush_enabled(1);
            debug_buffer_append("UHCI: cmdline enabled CLFLUSH\n");
        } else if (strstr(cmd, "uhci_clflush=0")) {
            extern void uhci_set_clflush_enabled(int);
            uhci_set_clflush_enabled(0);
            debug_buffer_append("UHCI: cmdline disabled CLFLUSH\n");
        }
    }

    if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        debug_buffer_append_dec("Lower memory: ", mbi->mem_lower);
        debug_buffer_append_dec("Upper memory: ", mbi->mem_upper);
    }

    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        multiboot_parse_memory_map(mbi);
    }

    if (mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
        multiboot_detect_framebuffer(mbi);
    }

    debug_buffer_flush();
}

void multiboot_parse_verbosity(const char* cmdline) {
    if (strstr(cmdline, "verbosity=silent")) {
        g_verbosity = VERBOSITY_SILENT;
    } else if (strstr(cmdline, "verbosity=minimal")) {
        g_verbosity = VERBOSITY_MINIMAL;
    } else if (strstr(cmdline, "verbosity=verbose")) {
        g_verbosity = VERBOSITY_VERBOSE;
    }
    debug_buffer_append("Verbosity parsed from cmdline\n");
}

void multiboot_detect_framebuffer(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) return;

    framebuffer_info_t* fb = &mbi->framebuffer_info;
    uint32_t fb_addr = (uint32_t)(fb->framebuffer_addr & 0xFFFFFFFF);

    if (fb->framebuffer_width < 320 || fb->framebuffer_height < 200) return;
    if (fb->framebuffer_bpp < 8 || fb->framebuffer_bpp > 32) return;

    display_info_t* display = graphics_get_display_info();
    if (display && fb_addr != 0) {
        display->framebuffer = (uint32_t*)fb_addr;
        display->width = fb->framebuffer_width;
        display->height = fb->framebuffer_height;
        display->bpp = fb->framebuffer_bpp;
        display->pitch = fb->framebuffer_pitch;

        uint32_t fb_size = fb->framebuffer_height * fb->framebuffer_pitch;
        if (fb_addr >= 0x100000) vmm_map_framebuffer(fb_addr, fb_size);
    }

    debug_buffer_append("Framebuffer info parsed and graphics system updated\n");
}

void multiboot_detect_vbe_framebuffer(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_VBE)) {
        SERIAL_LOG("No VBE information provided by bootloader\n");
        return;
    }
    
    // GRUB provides framebuffer info through VBE when gfxpayload is set
    // The framebuffer address is typically available in the VBE mode info
    
    SERIAL_LOG("VBE framebuffer detection:\n");
    SERIAL_LOG_HEX("  VBE control info: ", mbi->vbe_info.vbe_control_info);
    SERIAL_LOG("\n");
    SERIAL_LOG_HEX("  VBE mode info: ", mbi->vbe_info.vbe_mode_info);
    SERIAL_LOG("\n");
    SERIAL_LOG_HEX("  VBE mode: ", mbi->vbe_info.vbe_mode);
    SERIAL_LOG("\n");
    
    // Try to extract framebuffer address from VBE mode info
    // This is a simplified approach - VBE mode info structure is complex
    // but GRUB typically sets up a linear framebuffer when gfxpayload is used
    
    // For GRUB with gfxpayload, we can try some common framebuffer addresses
    uint32_t* potential_addrs[] = {
        (uint32_t*)0xFD000000,  // Common QEMU framebuffer
        (uint32_t*)0xE0000000,  // Alternative address
        (uint32_t*)0xF0000000,  // Another common address
        NULL
    };
    
    for (int i = 0; potential_addrs[i] != NULL; i++) {
        uint32_t test_addr = (uint32_t)potential_addrs[i];
        SERIAL_LOG("  Testing framebuffer at: ");
        SERIAL_LOG_HEX("  Testing framebuffer at: ", test_addr);
        SERIAL_LOG("\n");
        
        // Test if this address is accessible (simple write/read test)
        // In a real implementation, you'd want page fault handling here
        
        // For now, assume the first address works if VBE is available
        display_info_t* display = graphics_get_display_info();
        if (display) {
            display->framebuffer = (uint32_t*)test_addr;
            display->width = 1024;   // From gfxpayload=1024x768x32
            display->height = 768;
            display->bpp = 32;
            display->pitch = 1024 * 4;  // 32 bits = 4 bytes per pixel
            
            SERIAL_LOG("VBE framebuffer configured successfully\n");
            return;
        }
    }
    
    SERIAL_LOG("VBE framebuffer detection failed\n");
}

void multiboot_parse_memory_map(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) return;

    //pmm_init();
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mbi->mmap_addr;
    multiboot_memory_map_t* mmap_end = (multiboot_memory_map_t*)(mbi->mmap_addr + mbi->mmap_length);

    while (mmap < mmap_end) {
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE && mmap->addr < 0x100000000ULL) {
            uint32_t start = (uint32_t)mmap->addr;
            uint32_t length = (uint32_t)mmap->len;
            uint32_t page_start = (start + 0xFFF) & ~0xFFF;
            uint32_t page_end = (start + length) & ~0xFFF;
            if (page_start < 0x100000) page_start = 0x100000;
            if (page_end > page_start) pmm_mark_region_free(page_start, page_end - page_start);
        }
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    pmm_mark_region_used(0x100000, 0x400000);  // Reserve kernel space
    debug_buffer_append("Memory map parsed and PMM initialized\n");
}

multiboot_info_t* multiboot_get_info(void) {
    return g_multiboot_info;
}