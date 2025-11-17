#include "vfs.h"
#include "fat16.h"
#include "drivers/block/ramdisk.h"

void fs_init(void) {
    ramdisk_init();
    fat16_init();
    // Add more fs/block drivers here
    vfs_mount("ram0", "fat16", "/ram0");
}
