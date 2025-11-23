#include "vfs.h"
#include "fat16.h"
#include "drivers/block/ramdisk.h"

// Forward declaration for ATA block device
extern void ata_blockdev_init(void);

void fs_init(void) {
    ramdisk_init();
    fat16_init();
    
    // Initialize ATA hard disk
    ata_blockdev_init();
    
    // Add more fs/block drivers here
    vfs_mount("ram0", "fat16", "/ram0");
}
