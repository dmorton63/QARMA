#include "vfs.h"
#include "fat16.h"
#include "block/block/ramdisk.h"

// Forward declaration for ATA block device
extern void ata_blockdev_init(void);

void fs_init(void) {
    extern void gfx_print(const char*);
    
    ramdisk_init();
    fat16_init();
    
    // Initialize ATA hard disk
    ata_blockdev_init();
    
    gfx_print("[FS_INIT] ATA init returned, about to vfs_mount\n");
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'@', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Add more fs/block drivers here
    vfs_mount("ram0", "fat16", "/ram0");
    
    gfx_print("[FS_INIT] vfs_mount completed\n");
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'#', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
}
