/**
 * QARMA - Multiboot Information Structures
 * 
 * Definitions for parsing multiboot information passed by the bootloader
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "kernel_types.h"

#define MULTIBOOT_MAGIC 0x2BADB002

// Multiboot flags
#define MULTIBOOT_FLAG_MEM     0x1
#define MULTIBOOT_FLAG_DEVICE  0x2
#define MULTIBOOT_FLAG_CMDLINE 0x4
#define MULTIBOOT_FLAG_MODS    0x8
#define MULTIBOOT_FLAG_AOUT    0x10
#define MULTIBOOT_FLAG_ELF     0x20
#define MULTIBOOT_FLAG_MMAP    0x40
#define MULTIBOOT_FLAG_CONFIG  0x80
#define MULTIBOOT_FLAG_LOADER  0x100
#define MULTIBOOT_FLAG_APM     0x200
#define MULTIBOOT_FLAG_VBE     0x400
#define MULTIBOOT_FLAG_FRAMEBUFFER 0x1000

// VBE info structure
typedef struct {
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed)) vbe_info_t;

// Framebuffer info structure
typedef struct {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    
    union {
        struct {
            uint32_t framebuffer_palette_addr;
            uint16_t framebuffer_palette_num_colors;
        };
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
} __attribute__((packed)) framebuffer_info_t;

// Main multiboot info structure
typedef struct multiboot_info {
    uint32_t flags;
    
    // Memory info (available if flags[0] is set)
    uint32_t mem_lower;
    uint32_t mem_upper;
    
    // Boot device (available if flags[1] is set)
    uint32_t boot_device;
    
    // Command line (available if flags[2] is set)
    uint32_t cmdline;
    
    // Modules (available if flags[3] is set)
    uint32_t mods_count;
    uint32_t mods_addr;
    
    // ELF section headers (available if flags[5] is set)
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
    
    // Memory map (available if flags[6] is set)
    uint32_t mmap_length;
    uint32_t mmap_addr;
    
    // Drive info (available if flags[7] is set)
    uint32_t drives_length;
    uint32_t drives_addr;
    
    // Config table (available if flags[8] is set)
    uint32_t config_table;
    
    // Bootloader name (available if flags[9] is set)
    uint32_t boot_loader_name;
    
    // APM table (available if flags[10] is set)
    uint32_t apm_table;
    
    // VBE info (available if flags[11] is set)
    vbe_info_t vbe_info;
    
    // Framebuffer info (available if flags[12] is set)
    framebuffer_info_t framebuffer_info;
    
} __attribute__((packed)) multiboot_info_t;

// Memory map entry structure
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

// Memory map entry types
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

// Function declarations
void multiboot_parse_info(uint32_t magic, multiboot_info_t* mbi);
void multiboot_parse_memory_map(multiboot_info_t* mbi);
void multiboot_parse_verbosity(const char *cmdline);
void multiboot_detect_framebuffer(multiboot_info_t *mbi);
void multiboot_detect_vbe_framebuffer(multiboot_info_t* mbi);
multiboot_info_t* multiboot_get_info(void);

#endif // MULTIBOOT_H