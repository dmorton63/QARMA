/**
 * QARMA - Multiboot Information Parsing
 * 
 * Functions for parsing multiboot information from the bootloader.
 * Processes memory maps, module information, and boot parameters.
 */

#include "multiboot.h"
#include "graphics.h"
#include "framebuffer.h"
#include "kernel.h"
#include "boot_log.h"
#include "config.h"
#include "string.h"
#include "io.h"
#include "memory.h"
#include "memory/pmm/pmm.h"


static multiboot_info_t* g_multiboot_info = NULL;

// Helper: align up to 8-byte boundary for Multiboot2 tags
static inline uint32_t align_up_8(uint32_t x) {
    return (x + 7) & ~7;
}

// Parse all Multiboot2 tags (framebuffer, memory map, command line)
void multiboot2_parse_all_tags(uintptr_t mbi_ptr) {
    extern FramebufferInfo* fbinfo;
    
    if (!mbi_ptr) {
        SERIAL_LOG("[MB2] Invalid mbi_ptr\n");
        return;
    }
    
    // Multiboot2 info structure starts with total_size
    const uint32_t total_size = *(const uint32_t*)mbi_ptr;
    const uint32_t reserved = *(const uint32_t*)(mbi_ptr + 4);
    
    SERIAL_LOG("[MB2] Multiboot2 info total_size=");
    SERIAL_LOG_DEC("", total_size);
    SERIAL_LOG(" reserved=");
    SERIAL_LOG_DEC("", reserved);
    SERIAL_LOG("\n");
    
    if (fbinfo) {
        fbinfo->valid = false;
    }
    
    // Tags start at offset 8
    const uint8_t* p = (const uint8_t*)(mbi_ptr + 8);
    const uint8_t* end = (const uint8_t*)(mbi_ptr + total_size);
    
    while (p < end) {
        uint32_t type = *(const uint32_t*)(p + 0);
        uint32_t size = *(const uint32_t*)(p + 4);
        
        // End tag
        if (type == 0 && size == 8) {
            SERIAL_LOG("[MB2] Reached end tag\n");
            break;
        }
        
        // Command line tag (type 1)
        if (type == 1) {
            const char* cmdline = (const char*)(p + 8);
            SERIAL_LOG("[MB2] Found command line: ");
            SERIAL_LOG(cmdline);
            SERIAL_LOG("\n");
            multiboot_parse_verbosity(cmdline);
        }
        
        // Memory map tag (type 6)
        if (type == 6) {
            SERIAL_LOG("[MB2] Found memory map tag\n");
            uint32_t entry_size = *(const uint32_t*)(p + 8);
            uint32_t entry_version = *(const uint32_t*)(p + 12);
            const uint8_t* entries = p + 16;
            const uint8_t* entries_end = p + size;
            
            SERIAL_LOG("[MB2] Entry size=");
            SERIAL_LOG_DEC("", entry_size);
            SERIAL_LOG(" version=");
            SERIAL_LOG_DEC("", entry_version);
            SERIAL_LOG("\n");
            
            uint32_t region_count = 0;
            while (entries < entries_end) {
                uint64_t base = *(const uint64_t*)(entries + 0);
                uint64_t length = *(const uint64_t*)(entries + 8);
                uint32_t mtype = *(const uint32_t*)(entries + 16);
                
                region_count++;
                SERIAL_LOG("[E820] Region ");
                SERIAL_LOG_DEC("", region_count);
                SERIAL_LOG(": 0x");
                SERIAL_LOG_HEX("", (uint32_t)(base >> 32));
                SERIAL_LOG_HEX("", (uint32_t)base);
                SERIAL_LOG(" len=0x");
                SERIAL_LOG_HEX("", (uint32_t)(length >> 32));
                SERIAL_LOG_HEX("", (uint32_t)length);
                SERIAL_LOG(" type=");
                SERIAL_LOG_DEC("", mtype);
                SERIAL_LOG("\n");
                
                // Type 1 = Available, mark as free for PMM
                if (mtype == 1 && base < 0x100000000ULL) {
                    uint32_t start = (uint32_t)base;
                    uint32_t len = (uint32_t)length;
                    uint32_t page_start = (start + 0xFFF) & ~0xFFF;
                    uint32_t page_end = (start + len) & ~0xFFF;
                    
                    if (page_start < 0x100000) page_start = 0x100000;  // Skip low memory
                    
                    if (page_end > page_start) {
                        SERIAL_LOG("[E820]   -> Marking FREE: 0x");
                        SERIAL_LOG_HEX("", page_start);
                        SERIAL_LOG(" - 0x");
                        SERIAL_LOG_HEX("", page_end);
                        SERIAL_LOG("\n");
                        pmm_mark_region_free(page_start, page_end - page_start);
                    }
                }
                
                entries += entry_size;
            }
            
            // Reserve kernel region
            SERIAL_LOG("[E820] Reserving kernel: 0x100000-0x500000\n");
            pmm_mark_region_used(0x100000, 0x400000);
        }
        
        // Framebuffer tag (type 8)
        if (type == 8 && fbinfo) {
            SERIAL_LOG("[MB2] Found framebuffer tag\n");
            
            const uint64_t addr   = *(const uint64_t*)(p + 8);
            const uint32_t pitch  = *(const uint32_t*)(p + 16);
            const uint32_t width  = *(const uint32_t*)(p + 20);
            const uint32_t height = *(const uint32_t*)(p + 24);
            const uint8_t  bpp    = *(const uint8_t*)(p + 28);
            const uint8_t  fbtype = *(const uint8_t*)(p + 29);
            
            SERIAL_LOG("[MB2] FB ");
            SERIAL_LOG_DEC("", width);
            SERIAL_LOG("x");
            SERIAL_LOG_DEC("", height);
            SERIAL_LOG(" bpp=");
            SERIAL_LOG_DEC("", bpp);
            SERIAL_LOG(" @ 0x");
            SERIAL_LOG_HEX("", (uint32_t)(addr >> 32));
            SERIAL_LOG_HEX("", (uint32_t)addr);
            SERIAL_LOG("\n");
            
            if (addr != 0 && addr != 0xB8000 && width >= 320 && height >= 200 && bpp >= 8 && bpp <= 32) {
                fbinfo->phys_addr = addr;
                fbinfo->virt_addr = addr;
                fbinfo->pitch = pitch;
                fbinfo->width = width;
                fbinfo->height = height;
                fbinfo->bpp = bpp;
                fbinfo->type = fbtype;
                fbinfo->valid = true;
                SERIAL_LOG("[MB2] Framebuffer OK\n");
            }
        }
        
        // Move to next tag (align to 8 bytes)
        p += align_up_8(size);
    }
}

void multiboot_parse_info(uint32_t magic, multiboot_info_t* mbi) {
    SERIAL_LOG("[MBOOT] Starting multiboot_parse_info\n");
    debug_buffer_clear();

    SERIAL_LOG("[MBOOT] Received magic: 0x");
    SERIAL_LOG_HEX("", magic);
    SERIAL_LOG("\n");
    SERIAL_LOG("[MBOOT] Expected magic: 0x");
    SERIAL_LOG_HEX("", MULTIBOOT_MAGIC);
    SERIAL_LOG("\n");

    if (magic != MULTIBOOT_MAGIC) {
        SERIAL_LOG("[MBOOT] ERROR: Magic mismatch!\n");
        debug_buffer_push("ERROR: Bad multiboot magic!\n");
        debug_buffer_flush_lines();
        return;
    }

    SERIAL_LOG("[MBOOT] Magic validated\n");
    g_multiboot_info = mbi;
    debug_buffer_append("Multiboot info assigned\n");

    // Log all flags for debugging
    SERIAL_LOG("[MBOOT] Flags: 0x");
    SERIAL_LOG_HEX("", mbi->flags);
    SERIAL_LOG("\n");
    SERIAL_LOG("[MBOOT] Flag checks:\n");
    SERIAL_LOG("[MBOOT]   MEM (0x1): ");
    SERIAL_LOG((mbi->flags & 0x1) ? "YES\n" : "NO\n");
    SERIAL_LOG("[MBOOT]   CMDLINE (0x4): ");
    SERIAL_LOG((mbi->flags & 0x4) ? "YES\n" : "NO\n");
    SERIAL_LOG("[MBOOT]   MMAP (0x40): ");
    SERIAL_LOG((mbi->flags & 0x40) ? "YES\n" : "NO\n");
    SERIAL_LOG("[MBOOT]   VBE (0x400): ");
    SERIAL_LOG((mbi->flags & 0x400) ? "YES\n" : "NO\n");
    SERIAL_LOG("[MBOOT]   FRAMEBUFFER (0x1000): ");
    SERIAL_LOG((mbi->flags & 0x1000) ? "YES\n" : "NO\n");

    // Multiboot2 uses tag-based structure, not flags
    // Parse all tags instead of checking flags
    SERIAL_LOG("[MBOOT] Parsing Multiboot2 tags\n");
    
    // Parse framebuffer, memory map, command line from tags
    multiboot2_parse_all_tags((uintptr_t)mbi);

    SERIAL_LOG("[MBOOT] Flushing debug buffer\n");
    debug_buffer_flush();
    SERIAL_LOG("[MBOOT] multiboot_parse_info complete\n");
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
    (void)mbi;  // Unused
    SERIAL_LOG("[MBOOT] multiboot_detect_framebuffer called (deprecated)\n");
    // This function is deprecated - use multiboot2_parse_framebuffer instead
    // Kept for compatibility but does nothing
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
        uintptr_t test_addr = (uintptr_t)potential_addrs[i];
        SERIAL_LOG("  Testing framebuffer at: ");
        SERIAL_LOG_HEX("  Testing framebuffer at: ", (uint32_t)test_addr);
        SERIAL_LOG("\n");
        
        // Test if this address is accessible (simple write/read test)
        // In a real implementation, you'd want page fault handling here
        
        // For now, assume the first address works if VBE is available
        display_info_t* display = graphics_get_display_info();
        if (display) {
            display->framebuffer = (uint32_t*)(uintptr_t)test_addr;
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

// E820 Memory Type Names (for logging)
static const char* e820_type_name(uint32_t type) {
    switch (type) {
        case MULTIBOOT_MEMORY_AVAILABLE: return "Available";
        case 2: return "Reserved";
        case 3: return "ACPI Reclaimable";
        case 4: return "ACPI NVS";
        case 5: return "Bad Memory";
        default: return "Unknown";
    }
}

void multiboot_parse_memory_map(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) {
        SERIAL_LOG("[E820] ERROR: No memory map provided by bootloader\n");
        return;
    }

    SERIAL_LOG("[E820] ========== E820 Memory Map ==========\n");
    SERIAL_LOG("[E820] Total map size: ");
    SERIAL_LOG_DEC("", mbi->mmap_length);
    SERIAL_LOG(" bytes\n");
    
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)(uintptr_t)mbi->mmap_addr;
    multiboot_memory_map_t* mmap_end = (multiboot_memory_map_t*)((uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    uint32_t region_count = 0;
    uint64_t total_available = 0;
    uint64_t total_reserved = 0;
    
    SERIAL_LOG("[E820] Parsing memory regions...\n");
    while (mmap < mmap_end) {
        region_count++;
        
        // Log every region with full details
        SERIAL_LOG("[E820] Region ");
        SERIAL_LOG_DEC("", region_count);
        SERIAL_LOG(": Base=0x");
        SERIAL_LOG_HEX("", (uint32_t)(mmap->addr >> 32));
        SERIAL_LOG_HEX("", (uint32_t)(mmap->addr & 0xFFFFFFFF));
        SERIAL_LOG(" Len=0x");
        SERIAL_LOG_HEX("", (uint32_t)(mmap->len >> 32));
        SERIAL_LOG_HEX("", (uint32_t)(mmap->len & 0xFFFFFFFF));
        SERIAL_LOG(" Type=");
        SERIAL_LOG(e820_type_name(mmap->type));
        SERIAL_LOG("\n");
        
        // Track totals
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
            total_available += mmap->len;
        } else {
            total_reserved += mmap->len;
        }
        
        // Mark available memory for PMM (32-bit only)
        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE && mmap->addr < 0x100000000ULL) {
            uint32_t start = (uint32_t)mmap->addr;
            uint32_t length = (uint32_t)mmap->len;
            uint32_t page_start = (start + 0xFFF) & ~0xFFF;
            uint32_t page_end = (start + length) & ~0xFFF;
            
            // Skip low memory (below 1MB)
            if (page_start < 0x100000) page_start = 0x100000;
            
            if (page_end > page_start) {
                uint32_t usable = page_end - page_start;
                SERIAL_LOG("[E820]   -> Marking 0x");
                SERIAL_LOG_HEX("", page_start);
                SERIAL_LOG(" - 0x");
                SERIAL_LOG_HEX("", page_end);
                SERIAL_LOG(" as FREE (");
                SERIAL_LOG_DEC("", usable / 1024);
                SERIAL_LOG(" KB)\n");
                pmm_mark_region_free(page_start, usable);
            }
        } else if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE && mmap->addr >= 0x100000000ULL) {
            SERIAL_LOG("[E820]   -> Skipping >4GB region (32-bit OS)\n");
        } else if (mmap->type == 3) {
            SERIAL_LOG("[E820]   -> ACPI Reclaimable (can be freed after parsing)\n");
        } else if (mmap->type == 4) {
            SERIAL_LOG("[E820]   -> ACPI NVS (must not touch)\n");
        }
        
        mmap = (multiboot_memory_map_t*)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }

    SERIAL_LOG("[E820] ========== Summary ==========\n");
    SERIAL_LOG("[E820] Total regions: ");
    SERIAL_LOG_DEC("", region_count);
    SERIAL_LOG("\n[E820] Available RAM: ");
    SERIAL_LOG_DEC("", (uint32_t)(total_available / (1024 * 1024)));
    SERIAL_LOG(" MB\n[E820] Reserved: ");
    SERIAL_LOG_DEC("", (uint32_t)(total_reserved / (1024 * 1024)));
    SERIAL_LOG(" MB\n");
    
    // Reserve kernel region
    SERIAL_LOG("[E820] Reserving kernel space: 0x100000 - 0x500000 (4MB)\n");
    pmm_mark_region_used(0x100000, 0x400000);
    
    SERIAL_LOG("[E820] Memory map parsing complete\n");
    SERIAL_LOG("[E820] =====================================\n");
    debug_buffer_append("Memory map parsed and PMM initialized\n");
}

multiboot_info_t* multiboot_get_info(void) {
    return g_multiboot_info;
}