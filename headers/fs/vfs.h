#pragma once
#include "core/stdtools.h"
#include "core/blockdev.h"
#include "core/memory.h"

// VFS node types
#define VFS_TYPE_FILE  1
#define VFS_TYPE_DIR   2

// Forward declaration
struct vfs_node;

// Filesystem driver interface
struct fs_driver {
    const char* name;
    int (*mount)(blockdev_t* dev, struct vfs_node* mountpoint);
    int (*probe)(blockdev_t* dev);
    // ...
};

// VFS node (file or directory)
typedef struct vfs_node {
    char name[64];
    uint32_t type;
    struct vfs_node* parent;
    struct vfs_node* children; // first child (for directories)
    struct vfs_node* next;     // next sibling
    size_t size;
    void* fs_data;
    struct fs_driver* fs;
    blockdev_t* blockdev;      // device backing this mount (if any)
    // ...
} vfs_node_t;

// Register a filesystem driver
void vfs_register_fs(struct fs_driver* fs);
// Initialize VFS system
void vfs_init(void);
// Mount a filesystem
int vfs_mount(const char* devname, const char* fstype, const char* mountpoint);
// Open a file
vfs_node_t* vfs_open(const char* path);
// Read from a file
int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
// ...
