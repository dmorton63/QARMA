/*
 * QARMA - ATA Block Device Wrapper
 * 
 * Integrates ATA driver with the block device subsystem
 */

#include "drivers/ata.h"
#include "core/blockdev.h"
#include "fs/vfs.h"
#include "fs/fat16.h"
#include "config.h"

#define ATA_BLOCK_SIZE 512

static int ata_blockdev_read(blockdev_t* dev, uint64_t lba, void* buf, size_t count) {
    if (!dev || !buf || count > 255) return -1;
    
    if (ata_read_sectors((uint32_t)lba, (uint8_t)count, buf)) {
        return 0;
    }
    return -1;
}

static int ata_blockdev_write(blockdev_t* dev, uint64_t lba, const void* buf, size_t count) {
    if (!dev || !buf || count > 255) return -1;
    
    if (ata_write_sectors((uint32_t)lba, (uint8_t)count, buf)) {
        return 0;
    }
    return -1;
}

static blockdev_t ata_blockdev = {
    .type = BLOCKDEV_TYPE_ATA,
    .name = "hda",
    .num_blocks = 20480,  // 10MB / 512 bytes
    .block_size = ATA_BLOCK_SIZE,
    .driver_data = 0,
    .read = ata_blockdev_read,
    .write = ata_blockdev_write,
    .next = 0
};

void ata_blockdev_init(void) {
    extern void gfx_print(const char*);
    
    SERIAL_LOG("[ATA] Initializing ATA driver...\n");
    gfx_print("[ATA] Initializing ATA driver...\n");
    
    if (!ata_init()) {
        SERIAL_LOG("[ATA] No disk detected\n");
        gfx_print("[ATA] No disk detected\n");
        return;
    }
    
    SERIAL_LOG("[ATA] Disk detected\n");
    gfx_print("[ATA] Disk detected\n");
    
    // Register block device
    blockdev_register(&ata_blockdev);
    SERIAL_LOG("[ATA] Block device registered as hda\n");
    gfx_print("[ATA] Block device registered as hda\n");
    
    // Mount as SimpleFS (same format as ramdisk)
    if (vfs_mount("hda", "simplefs", "/disk") == 0) {
        SERIAL_LOG("[ATA] Mounted SimpleFS filesystem at /disk\n");
        gfx_print("[ATA] Mounted SimpleFS filesystem at /disk\n");
    } else {
        SERIAL_LOG("[ATA] Warning: Could not mount filesystem\n");
        gfx_print("[ATA] Warning: Could not mount filesystem\n");
    }
}
