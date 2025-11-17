#include "simplefs.h"
#include "core/string.h"
#include "core/memory.h"

#define MAX_FILES 16

// Simple filesystem structure for demo
typedef struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
    uint32_t used;
} simple_file_entry_t;

typedef struct {
    uint32_t magic;  // 0x51554144 "QUAD" 
    uint32_t file_count;
    simple_file_entry_t files[MAX_FILES];
} simple_fs_header_t;

// Global reference to RAM disk
static blockdev_t* ram_device = NULL;
static simple_fs_header_t fs_header;

// Forward declarations
static int simplefs_mount(blockdev_t* dev, vfs_node_t* mountpoint);
static int simplefs_probe(blockdev_t* dev);
static vfs_node_t* simplefs_create_node(const char* name, uint32_t size, uint32_t offset);

// Filesystem driver interface
static struct fs_driver simplefs_driver = {
    .name = "simplefs",
    .mount = simplefs_mount,
    .probe = simplefs_probe
};

/**
 * Initialize the simple filesystem driver
 */
void simplefs_init(void) {
    vfs_register_fs(&simplefs_driver);
}

/**
 * Probe if device contains our simple filesystem
 */
static int simplefs_probe(blockdev_t* dev) {
    if (!dev || !dev->read) {
        return 0;
    }
    
    uint8_t sector[512];
    if (dev->read(dev, 0, sector, 1) != 0) {
        return 0;
    }
    
    // Check magic number
    simple_fs_header_t* header = (simple_fs_header_t*)sector;
    return (header->magic == 0x51554144); // "QUAD"
}

/**
 * Mount the filesystem
 */
static int simplefs_mount(blockdev_t* dev, vfs_node_t* mountpoint) {
    if (!dev || !mountpoint) {
        return -1;
    }
    
    ram_device = dev;
    
    // Read filesystem header
    if (dev->read(dev, 0, &fs_header, 1) != 0) {
        return -1;
    }
    
    // Validate header
    if (fs_header.magic != 0x51554144) {
        return -1;
    }
    
    // Create VFS nodes for each file
    for (uint32_t i = 0; i < fs_header.file_count && i < MAX_FILES; i++) {
        if (fs_header.files[i].used) {
            vfs_node_t* file_node = simplefs_create_node(
                fs_header.files[i].name,
                fs_header.files[i].size,
                fs_header.files[i].offset
            );
            
            if (file_node) {
                // Add to mount point's children
                file_node->parent = mountpoint;
                file_node->next = mountpoint->children;
                mountpoint->children = file_node;
            }
        }
    }
    
    return 0;
}

/**
 * Create a VFS node for a file
 */
static vfs_node_t* simplefs_create_node(const char* name, uint32_t size, uint32_t offset) {
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (!node) {
        return NULL;
    }
    
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 63);
    node->type = VFS_TYPE_FILE;
    node->size = size;
    node->fs_data = (void*)(uintptr_t)offset; // Store file offset
    node->fs = &simplefs_driver;
    node->blockdev = ram_device;
    
    return node;
}

/**
 * Find a file by name in the filesystem
 */
vfs_node_t* simplefs_find_file(const char* filename) {
    if (!filename || !ram_device) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < fs_header.file_count && i < MAX_FILES; i++) {
        if (fs_header.files[i].used && strcmp(fs_header.files[i].name, filename) == 0) {
            return simplefs_create_node(
                fs_header.files[i].name,
                fs_header.files[i].size,
                fs_header.files[i].offset
            );
        }
    }
    
    return NULL;
}

/**
 * Read file content
 */
int simplefs_read_file(const char* filename, void* buffer, size_t size, size_t offset) {
    if (!filename || !buffer || !ram_device) {
        return -1;
    }
    
    // Find file entry
    simple_file_entry_t* file_entry = NULL;
    for (uint32_t i = 0; i < fs_header.file_count && i < MAX_FILES; i++) {
        if (fs_header.files[i].used && strcmp(fs_header.files[i].name, filename) == 0) {
            file_entry = &fs_header.files[i];
            break;
        }
    }
    
    if (!file_entry) {
        return -1;
    }
    
    // Check bounds
    if (offset >= file_entry->size) {
        return 0; // EOF
    }
    
    size_t read_size = size;
    if (offset + read_size > file_entry->size) {
        read_size = file_entry->size - offset;
    }
    
    // Calculate block and byte offset
    uint32_t file_offset = file_entry->offset + offset;
    uint32_t block_num = file_offset / 512;
    uint32_t block_offset = file_offset % 512;
    
    // For simplicity, read one block and copy the relevant portion
    uint8_t block_buffer[512];
    if (ram_device->read(ram_device, block_num, block_buffer, 1) != 0) {
        return -1;
    }
    
    // Copy data to user buffer
    size_t copy_size = read_size;
    if (block_offset + copy_size > 512) {
        copy_size = 512 - block_offset;
    }
    
    memcpy(buffer, &block_buffer[block_offset], copy_size);
    return copy_size;
}