#include "fat16.h"
#include "core/stdtools.h"
#include "core/string.h"  
#include "core/memory.h"
static int fat16_mount(blockdev_t* dev, vfs_node_t* mountpoint) {
    (void)dev; (void)mountpoint;
    // TODO: Implement FAT16 mount logic
    return 0;
}


static int fat16_probe(blockdev_t* dev)     {
    // Read first sector and check for FAT16 signature
    uint8_t sector[512];
    if (!dev || !dev->read) return 0;
    if (dev->read(dev, 0, sector, 1) != 0) return 0;
    // Check 0x55AA at offset 510
    if (sector[510] != 0x55 || sector[511] != 0xAA) return 0;
    // Check for "FAT16" at offset 54 or 82 (depends on BPB)
    if (memcmp(&sector[54], "FAT16", 5) == 0 || memcmp(&sector[82], "FAT16", 5) == 0) return 1;
    return 0;
}

static struct fs_driver fat16_driver = {
    .name = "fat16",
    .mount = fat16_mount,
    .probe = fat16_probe
};

void fat16_init(void) {
    vfs_register_fs(&fat16_driver);
}
