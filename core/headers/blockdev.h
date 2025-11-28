#pragma once

#include "core/stdtools.h"

// Block device type
typedef enum {
    BLOCKDEV_TYPE_UNKNOWN = 0,
    BLOCKDEV_TYPE_RAMDISK,
    BLOCKDEV_TYPE_ATA,
    BLOCKDEV_TYPE_NVME,
    BLOCKDEV_TYPE_USB,
    BLOCKDEV_TYPE_OPTICAL,
    // ...
} blockdev_type_t;

// Block device descriptor
typedef struct blockdev {
    blockdev_type_t type;
    const char* name;
    uint64_t num_blocks;
    uint32_t block_size;
    void* driver_data;
    // Read/write sector interface
    int (*read)(struct blockdev* dev, uint64_t lba, void* buf, size_t count);
    int (*write)(struct blockdev* dev, uint64_t lba, const void* buf, size_t count);
    struct blockdev* next;
} blockdev_t;

// Register a block device
void blockdev_register(blockdev_t* dev);
// Find a block device by name
blockdev_t* blockdev_find(const char* name);
// List all block devices
blockdev_t* blockdev_list(void);
