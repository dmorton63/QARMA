/*
 * QARMA - ATA/IDE Hard Disk Driver Implementation
 */

#include "drivers/ata.h"
#include "core/io.h"
#include "core/string.h"
#include "config.h"


static uint16_t g_ata_base = ATA_PRIMARY_IO;
static bool g_disk_present = false;
static uint16_t g_ata_ctrl = ATA_PRIMARY_CONTROL;   // 0x3F6

static inline void ata_400ns_delay(void) {
    (void)inb(g_ata_ctrl);
    (void)inb(g_ata_ctrl);
    (void)inb(g_ata_ctrl);
    (void)inb(g_ata_ctrl);
}

static void ata_wait_bsy(void);
static bool ata_wait_ready(uint32_t spins, uint8_t *last);
static bool ata_wait_drq(uint32_t spins, uint8_t *last_status);
static uint8_t ata_read_status(void);

static inline uint8_t ata_status(void) { return inb(g_ata_base + ATA_REG_STATUS); }
static inline uint8_t ata_error(void)  { return inb(g_ata_base + ATA_REG_ERROR);  }

bool ata_init(void) {
    SERIAL_LOG("[ATA] DKM Initializing ATA driver...\n");
    // Enable IRQs on the device; use polling if you don’t have ISRs yet
    outb(g_ata_ctrl, 0x00);

    // Select master (CHS/LBA nibble stays 0 for now); then delay
    outb(g_ata_base + ATA_REG_DRIVE, 0xA0);
    ata_400ns_delay();

    // Wait for ready (BSY=0, DRDY=1)
    uint8_t st;
    if (!ata_wait_ready(100000, &st)) {
        // If totally floating bus, st often 0x00 or 0xFF
        g_disk_present = false;
        return false;
    }

    // Optional: IDENTIFY to confirm presence
    outb(g_ata_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    if (!ata_wait_drq(100000, &st)) {
        if (st & ATA_SR_ERR) {
            uint8_t er = ata_error();
            SERIAL_LOG("[ATA] IDENTIFY failed with error: ");
            SERIAL_LOG_HEX("", er);
            SERIAL_LOG(" status: ");
            SERIAL_LOG_HEX("", st);
            SERIAL_LOG("\n");
            
            // Try IDENTIFY PACKET DEVICE for ATAPI (CD-ROM)
            SERIAL_LOG("[ATA] Trying IDENTIFY PACKET DEVICE...\n");
            outb(g_ata_base + ATA_REG_COMMAND, 0xA1);  // IDENTIFY PACKET DEVICE
            if (!ata_wait_drq(100000, &st)) {
                SERIAL_LOG("[ATA] IDENTIFY PACKET DEVICE also failed\n");
                g_disk_present = false;
                return false;
            }
            SERIAL_LOG("[ATA] ATAPI device detected\n");
        } else {
            g_disk_present = false;
            return false;
        }
    }
    // Drain IDENTIFY data (256 words) into a temp buffer
    uint16_t tmp[256];
    for (int i = 0; i < 256; i++) tmp[i] = inw(g_ata_base + ATA_REG_DATA);

    g_disk_present = true;
    return true;
}

bool ata_disk_present(void) {
    return g_disk_present;
}

static void ata_wait_bsy(void) {
    // Wait until BSY bit is clear (with timeout)
    int timeout = 100000;
    while ((inb(g_ata_base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout > 0) {
        timeout--;
    }
}

static bool ata_wait_drq(uint32_t spins, uint8_t *last_status) {
    for (uint32_t i = 0; i < spins; i++) {
        uint8_t s = inb(g_ata_base + ATA_REG_STATUS);
        if (last_status) *last_status = s;

        if ((s & ATA_SR_ERR) || (s & ATA_SR_DF)) return false;   // error
        if ((s & ATA_SR_DRQ) && !(s & ATA_SR_BSY)) return true;  // data ready

        // 400ns delay
        inb(ATA_PRIMARY_CONTROL);
        inb(ATA_PRIMARY_CONTROL);
        inb(ATA_PRIMARY_CONTROL);
        inb(ATA_PRIMARY_CONTROL);
    }
    return false; // timed out
}

static uint8_t ata_read_status(void) {
    return inb(g_ata_base + ATA_REG_STATUS);
}

bool ata_read_sectors(uint32_t lba, uint8_t count, void *buffer) {
    if (!g_disk_present || !buffer || count == 0) return false;

    outb(g_ata_ctrl, 0x00); // enable device IRQs (even if you poll)

    // Select master LBA and settle
    outb(g_ata_base + ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_400ns_delay();

    uint8_t st;
    if (!ata_wait_ready(100000, &st)) {
        // log status
        return false;
    }

    // Program count & LBA28
    outb(g_ata_base + ATA_REG_SECCOUNT, count);
    outb(g_ata_base + ATA_REG_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(g_ata_base + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(g_ata_base + ATA_REG_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    // Issue READ SECTORS
    outb(g_ata_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    uint16_t *w = (uint16_t *)buffer;
    for (int s = 0; s < count; s++) {
        if (!ata_wait_drq(100000, &st)) {
            // log st and error reg
            if (st & ATA_SR_ERR) { uint8_t er = ata_error(); /* log er */ }
            return false;
        }
        for (int i = 0; i < 256; i++) w[s * 256 + i] = inw(g_ata_base + ATA_REG_DATA);
    }
    return true;
}


static bool ata_wait_ready(uint32_t spins, uint8_t *last) {
    for (uint32_t i = 0; i < spins; i++) {
        uint8_t s = ata_status();
        if (last) *last = s;
        if ((s & ATA_SR_ERR) || (s & ATA_SR_DF)) return false;       // error
        if (!(s & ATA_SR_BSY) && (s & ATA_SR_DRDY)) return true;     // ready
        ata_400ns_delay();
    }
    return false;
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
        ata_wait_drq(100000, NULL);
        
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


