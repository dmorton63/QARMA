/*
 * QARMA - ATA/IDE Hard Disk Driver Implementation
 */

#include "drivers/ata.h"
#include "core/io.h"
#include "core/string.h"

static uint16_t g_ata_base = ATA_PRIMARY_IO;
static bool g_disk_present = false;

static void ata_wait_bsy(void);
static void ata_wait_drq(void);
static uint8_t ata_read_status(void);

bool ata_init(void) {
    // Wait for drive to be ready
    ata_wait_bsy();
    
    // Select master drive
    outb(g_ata_base + ATA_REG_DRIVE, 0xA0);
    
    // Wait and check status
    ata_wait_bsy();
    uint8_t status = ata_read_status();
    
    // Check if drive exists
    if (status == 0x00 || status == 0xFF) {
        g_disk_present = false;
        return false;
    }
    
    g_disk_present = true;
    return true;
}

bool ata_disk_present(void) {
    return g_disk_present;
}

static void ata_wait_bsy(void) {
    // Wait until BSY bit is clear
    while (inb(g_ata_base + ATA_REG_STATUS) & ATA_SR_BSY) {
        // Busy wait
    }
}

static void ata_wait_drq(void) {
    // Wait until DRQ bit is set
    while (!(inb(g_ata_base + ATA_REG_STATUS) & ATA_SR_DRQ)) {
        // Busy wait
    }
}

static uint8_t ata_read_status(void) {
    return inb(g_ata_base + ATA_REG_STATUS);
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (!g_disk_present || !buffer || count == 0) {
        return false;
    }
    
    uint16_t* buf = (uint16_t*)buffer;
    
    // Wait for drive to be ready
    ata_wait_bsy();
    
    // Select master drive with LBA mode
    outb(g_ata_base + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Set sector count
    outb(g_ata_base + ATA_REG_SECCOUNT, count);
    
    // Set LBA
    outb(g_ata_base + ATA_REG_LBA_LO, (uint8_t)lba);
    outb(g_ata_base + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(g_ata_base + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send read command
    outb(g_ata_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    // Read sectors
    for (int sector = 0; sector < count; sector++) {
        // Wait for data to be ready
        ata_wait_drq();
        
        // Check for errors
        uint8_t status = ata_read_status();
        if (status & ATA_SR_ERR) {
            return false;
        }
        
        // Read 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = inw(g_ata_base + ATA_REG_DATA);
        }
    }
    
    return true;
}

bool ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer) {
    if (!g_disk_present || !buffer || count == 0) {
        return false;
    }
    
    const uint16_t* buf = (const uint16_t*)buffer;
    
    // Wait for drive to be ready
    ata_wait_bsy();
    
    // Select master drive with LBA mode
    outb(g_ata_base + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Set sector count
    outb(g_ata_base + ATA_REG_SECCOUNT, count);
    
    // Set LBA
    outb(g_ata_base + ATA_REG_LBA_LO, (uint8_t)lba);
    outb(g_ata_base + ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
    outb(g_ata_base + ATA_REG_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send write command
    outb(g_ata_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    // Write sectors
    for (int sector = 0; sector < count; sector++) {
        // Wait for drive to be ready
        ata_wait_drq();
        
        // Check for errors
        uint8_t status = ata_read_status();
        if (status & ATA_SR_ERR) {
            return false;
        }
        
        // Write 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            outw(g_ata_base + ATA_REG_DATA, buf[sector * 256 + i]);
        }
    }
    
    // Wait for write to complete
    ata_wait_bsy();
    
    // Flush cache to ensure data is written to disk
    ata_flush_cache();
    
    return true;
}

void ata_flush_cache(void) {
    if (!g_disk_present) {
        return;
    }
    
    // Wait for drive to be ready
    ata_wait_bsy();
    
    // Send cache flush command
    outb(g_ata_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    // Wait for flush to complete
    ata_wait_bsy();
}
