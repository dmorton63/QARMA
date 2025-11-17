#include "core/blockdev.h"
#include "core/io.h"
#include "core/string.h"
#include "config.h"

// ATA/ATAPI I/O ports for primary and secondary bus
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CONTROL 0x376

// ATA registers
#define ATA_REG_DATA       0
#define ATA_REG_ERROR      1
#define ATA_REG_FEATURES   1
#define ATA_REG_SECCOUNT0  2
#define ATA_REG_LBA0       3
#define ATA_REG_LBA1       4
#define ATA_REG_LBA2       5
#define ATA_REG_DEVSEL     6
#define ATA_REG_COMMAND    7
#define ATA_REG_STATUS     7

// ATA commands
#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_PACKET     0xA0
#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

// ATAPI commands
#define ATAPI_CMD_READ_10 0x28
#define ATAPI_CMD_READ_12 0xA8

// Status bits
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static uint16_t ata_base = ATA_PRIMARY_IO;
static bool cdrom_present = false;

static int ata_wait_bsy() {
    extern void serial_debug(const char*);
    int timeout = 1000000;
    while ((inb(ata_base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        serial_debug("[CDROM] Timeout waiting for BSY to clear\n");
        return -1;
    }
    return 0;
}

static int ata_wait_drq() {
    extern void serial_debug(const char*);
    int timeout = 1000000;
    while (!(inb(ata_base + ATA_REG_STATUS) & ATA_SR_DRQ) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        serial_debug("[CDROM] Timeout waiting for DRQ\n");
        return -1;
    }
    return 0;
}

static int cdrom_read(blockdev_t* dev, uint64_t lba, void* buf, size_t count) {
    extern void serial_debug(const char*);
    (void)dev;
    
    if (!cdrom_present || count == 0) {
        serial_debug("[CDROM] Read rejected: drive not present or count=0\n");
        return -1;
    }
    
    //serial_debug("[CDROM] Read request: LBA=");
    // Simple decimal print
    uint32_t temp = (uint32_t)lba;
    if (temp == 0) {
        //serial_debug("0");
    } else {
        char digits[12];
        int idx = 0;
        while (temp > 0) {
            digits[idx++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int k = idx - 1; k >= 0; k--) {
            char ch[2] = {digits[k], 0};
            //serial_debug(ch);
        }
    }
    //serial_debug(" count=1\n");
    
    uint16_t* buffer = (uint16_t*)buf;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t sector = lba + i;
        
        // Wait for drive to be ready
        //serial_debug("[CDROM] Waiting for drive ready...\n");
        if (ata_wait_bsy() < 0) return -1;
        
        // Select drive (master = 0xE0, slave = 0xF0)
        outb(ata_base + ATA_REG_DEVSEL, 0xE0);
        
        // Send PACKET command
        outb(ata_base + ATA_REG_FEATURES, 0x00);
        outb(ata_base + ATA_REG_LBA1, 2048 & 0xFF);        // Byte count low
        outb(ata_base + ATA_REG_LBA2, (2048 >> 8) & 0xFF); // Byte count high
        outb(ata_base + ATA_REG_COMMAND, ATA_CMD_PACKET);
        
        //serial_debug("[CDROM] PACKET command sent\n");
        
        // Small delay to let command register
        for (volatile int d = 0; d < 100; d++);
        
        // Wait for drive to be ready for packet
        if (ata_wait_bsy() < 0) return -1;
        
        // Check status before waiting for DRQ
        uint8_t status_before_drq = inb(ata_base + ATA_REG_STATUS);
        //serial_debug("[CDROM] Status before DRQ wait: 0x");
        char hex[3];
        hex[0] = "0123456789ABCDEF"[(status_before_drq >> 4) & 0xF];
        hex[1] = "0123456789ABCDEF"[status_before_drq & 0xF];
        hex[2] = '\0';
        //serial_debug(hex);
        //serial_debug("\n");
        
        // Check for error
        if (status_before_drq & ATA_SR_ERR) {
            //serial_debug("[CDROM] Error bit set after PACKET command!\n");
            uint8_t error = inb(ata_base + ATA_REG_ERROR);
            serial_debug("[CDROM] Error register: 0x");
            hex[0] = "0123456789ABCDEF"[(error >> 4) & 0xF];
            hex[1] = "0123456789ABCDEF"[error & 0xF];
            serial_debug(hex);
            serial_debug("\n");
            return -1;
        }
        
        if (ata_wait_drq() < 0) return -1;
        
        //serial_debug("[CDROM] Sending ATAPI READ(10) packet...\n");
        
        // Send ATAPI READ(10) command packet (10 bytes)
        // Format: [opcode] [flags] [LBA 31-24] [LBA 23-16] [LBA 15-8] [LBA 7-0]
        //         [reserved] [length 15-8] [length 7-0] [control]
        uint8_t atapi_packet[12] = {
            ATAPI_CMD_READ_10,        // 0: Command (READ 10)
            0x00,                     // 1: Flags  
            (sector >> 24) & 0xFF,    // 2: LBA byte 3
            (sector >> 16) & 0xFF,    // 3: LBA byte 2
            (sector >> 8) & 0xFF,     // 4: LBA byte 1
            sector & 0xFF,            // 5: LBA byte 0
            0x00,                     // 6: Reserved
            0x00,                     // 7: Transfer length MSB
            0x01,                     // 8: Transfer length LSB (1 sector)
            0x00,                     // 9: Control
            0x00,                     // 10: Padding
            0x00                      // 11: Padding
        };
        
        // Send packet as words (little endian)
        uint16_t* packet_words = (uint16_t*)atapi_packet;
        for (int j = 0; j < 6; j++) {
            outw(ata_base + ATA_REG_DATA, packet_words[j]);
        }
        
        //serial_debug("[CDROM] Waiting for data...\n");
        
        // Wait for data
        if (ata_wait_bsy() < 0) return -1;
        if (ata_wait_drq() < 0) return -1;
        
        // Read data (2048 bytes = 1024 words)
        for (int j = 0; j < 1024; j++) {
            buffer[j] = inw(ata_base + ATA_REG_DATA);
        }
        
        buffer += 1024;  // Move to next sector in buffer
    }
    
    return count;
}

static blockdev_t cdrom_dev = {
    .type = BLOCKDEV_TYPE_OPTICAL,
    .name = "cdrom0",
    .num_blocks = 0,  // Unknown size
    .block_size = 2048,  // CD-ROM sector size
    .driver_data = NULL,
    .read = cdrom_read,
    .write = NULL,  // CD-ROMs are read-only
    .next = NULL
};

void cdrom_init(void) {
    extern void serial_debug(const char*);
    serial_debug("[CDROM] Initializing CD-ROM driver\n");
    //SERIAL_LOG("[CDROM] Initializing CD-ROM driver\n");
    
    // Try both IDE controllers and both master/slave positions
    uint16_t controllers[] = {ATA_PRIMARY_IO, ATA_SECONDARY_IO};
    uint8_t drive_selects[] = {0xE0, 0xF0};  // Master, Slave
    
    bool detected = false;
    
    for (int ctrl = 0; ctrl < 2 && !detected; ctrl++) {
        uint16_t test_base = controllers[ctrl];
        //serial_debug("[CDROM] Trying controller at 0x");
        char ctrl_hex[5];
        for (int i = 3; i >= 0; i--) {
            ctrl_hex[3-i] = "0123456789ABCDEF"[(test_base >> (i*4)) & 0xF];
        }
        ctrl_hex[4] = '\0';
        //serial_debug(ctrl_hex);
        //serial_debug("\n");
        
        for (int drv = 0; drv < 2 && !detected; drv++) {
            uint8_t drive_select = drive_selects[drv];

            //serial_debug("[CDROM]   Trying drive select: 0x");
            //char hex_sel[3];
            //hex_sel[0] = "0123456789ABCDEF"[(drive_select >> 4) & 0xF];
            //hex_sel[1] = "0123456789ABCDEF"[drive_select & 0xF];
            //hex_sel[2] = '\0';
            //serial_debug(hex_sel);
            //serial_debug("\n");
            
            // Select drive
            outb(test_base + ATA_REG_DEVSEL, drive_select);
            // Delay for drive selection
            for (volatile int i = 0; i < 10000; i++);
            
            // Send IDENTIFY PACKET DEVICE command
            outb(test_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
            
            // Delay for command
            for (volatile int i = 0; i < 10000; i++);
            
            // Check if device exists
            uint8_t status = inb(test_base + ATA_REG_STATUS);
            //serial_debug("[CDROM]   Status: 0x");
            //char hex[3];
            //hex[0] = "0123456789ABCDEF"[(status >> 4) & 0xF];
            //hex[1] = "0123456789ABCDEF"[status & 0xF];
            //hex[2] = '\0';
            //serial_debug(hex);
            //serial_debug("\n");
            
            if (status != 0 && status != 0xFF) {
                detected = true;
                ata_base = test_base;
                serial_debug("[CDROM] Drive found!\n");
                break;
            }
        }
    }
    
    if (!detected) {
        serial_debug("[CDROM] No CD-ROM drive detected on any controller\n");
        SERIAL_LOG("[CDROM] No CD-ROM drive detected\n");
        return;
    }
    
    uint8_t status = inb(ata_base + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        serial_debug("[CDROM] Final status check failed\n");
        return;
    }
    
    // Wait for response
    if (ata_wait_bsy() < 0) {
        serial_debug("[CDROM] Timeout waiting for IDENTIFY response\n");
        return;
    }
    
    if (inb(ata_base + ATA_REG_STATUS) & ATA_SR_ERR) {
        serial_debug("[CDROM] CD-ROM identification failed (error bit set)\n");
        return;
    }
    
    if (ata_wait_drq() < 0) {
        serial_debug("[CDROM] Timeout waiting for DRQ after IDENTIFY\n");
        return;
    }
    
    // Read identification data (discard for now)
    for (int i = 0; i < 256; i++) {
        inw(ata_base + ATA_REG_DATA);
    }
    
    cdrom_present = true;
    serial_debug("[CDROM] CD-ROM drive detected successfully!\n");
    SERIAL_LOG("[CDROM] CD-ROM drive detected successfully\n");
    
    // Register block device
    blockdev_register(&cdrom_dev);
    serial_debug("[CDROM] Block device registered as 'cdrom0'\n");
    SERIAL_LOG("[CDROM] Block device registered as 'cdrom0'\n");
}
