/*
 * QARMA - ATA Block Device Wrapper
 * 
 * Integrates ATA driver with the block device subsystem
 */

#include "ata.h"
#include "blockdev.h"
#include "vfs.h"
#include "fat16.h"
#include "config.h"

#define ATA_BLOCK_SIZE 512
static bool g_disk_present = false;


// static bool ata_wait_ready(uint32_t spins, uint8_t *last) {
//     for (uint32_t i = 0; i < spins; i++) {
//         uint8_t s = ata_status();
//         if (last) *last = s;
//         if ((s & ATA_SR_ERR) || (s & ATA_SR_DF)) return false;       // error
//         if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRDY)) return true;     // ready
//         ata_400ns_delay();
//     }
//     return false;
// }

static bool ata_wait_drq(uint32_t spins, uint8_t *last) {
    for (uint32_t i = 0; i < spins; i++) {
        uint8_t s = ata_status();
        if (last) *last = s;
        if ((s & ATA_SR_ERR) || (s & ATA_SR_DF)) return false;
        if ((s & ATA_SR_DRQ) && !(s & ATA_SR_BSY)) return true;      // data ready
        ata_400ns_delay();
    }
    return false;
}


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
    SERIAL_LOG("!!!!!!!! ATA_BLOCKDEV_INIT 2025-11-24 NOON BUILD !!!!!!!!\n");
    gfx_print("!!!!!!!! ATA_BLOCKDEV_INIT 2025-11-24 NOON BUILD !!!!!!!!\n");
    
    if (!ata_init()) {
        SERIAL_LOG("[ATA] No disk detected\n");
        gfx_print("[ATA] No disk detected\n");
        return;
    }
    
    SERIAL_LOG("[ATA] Disk detected - NEW CODE EXECUTED\n");
    gfx_print("[ATA] Disk detected - NEW CODE EXECUTED\n");
    
    blockdev_register(&ata_blockdev);
    
    SERIAL_LOG("[ATA] Block device registered - CONTINUING TO VFS_MOUNT\n");
    gfx_print("[ATA] Block device registered - CONTINUING\n");
}
