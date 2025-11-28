/*
 * QARMA - ATA/IDE Hard Disk Driver
 * 
 * Simple PIO mode driver for reading/writing hard disks
 */

#ifndef ATA_H
#define ATA_H

#include "kernel_types.h"

// ATA I/O ports (primary bus)
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_PRIMARY_IRQ     14

// ATA registers
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

// ATA status bits
#define ATA_SR_BSY          0x80    // Busy
#define ATA_SR_DRDY         0x40    // Drive ready
#define ATA_SR_DF           0x20    // Drive write fault
#define ATA_SR_DSC          0x10    // Drive seek complete
#define ATA_SR_DRQ          0x08    // Data request ready
#define ATA_SR_CORR         0x04    // Corrected data
#define ATA_SR_IDX          0x02    // Index
#define ATA_SR_ERR          0x01    // Error

// ATA commands
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

/**
 * Initialize the ATA driver
 */
bool ata_init(void);

/**
 * Read sectors from the disk
 * @param lba Logical block address (sector number)
 * @param count Number of sectors to read
 * @param buffer Buffer to read into (must be at least count * 512 bytes)
 * @return true on success
 */
bool ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);

/**
 * Write sectors to the disk
 * @param lba Logical block address (sector number)
 * @param count Number of sectors to write
 * @param buffer Buffer to write from (must be at least count * 512 bytes)
 * @return true on success
 */
bool ata_write_sectors(uint32_t lba, uint8_t count, const void* buffer);

/**
 * Check if a disk is present
 */
bool ata_disk_present(void);

/**
 * Flush write cache to disk
 */
void ata_flush_cache(void);

#endif // ATA_H
