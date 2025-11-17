#ifndef ISO9660_H
#define ISO9660_H

#include "core/stdtools.h"

// ISO9660 Volume Descriptor types
#define ISO_VD_BOOT_RECORD      0
#define ISO_VD_PRIMARY          1
#define ISO_VD_SUPPLEMENTARY    2
#define ISO_VD_VOLUME_PARTITION 3
#define ISO_VD_TERMINATOR       255

// Directory entry flags
#define ISO_FLAG_HIDDEN     0x01
#define ISO_FLAG_DIRECTORY  0x02
#define ISO_FLAG_ASSOCIATED 0x04
#define ISO_FLAG_RECORD     0x08
#define ISO_FLAG_PROTECTION 0x10
#define ISO_FLAG_MULTIEXTENT 0x80

// ISO9660 Date/Time structure (7 bytes)
typedef struct {
    uint8_t year;        // Years since 1900
    uint8_t month;       // Month (1-12)
    uint8_t day;         // Day (1-31)
    uint8_t hour;        // Hour (0-23)
    uint8_t minute;      // Minute (0-59)
    uint8_t second;      // Second (0-59)
    int8_t  gmt_offset;  // GMT offset in 15-minute intervals
} __attribute__((packed)) iso_datetime_t;

// ISO9660 Directory Entry
typedef struct {
    uint8_t length;                 // Length of directory record
    uint8_t ext_attr_length;        // Extended attribute record length
    uint32_t extent_lba_le;         // Location of extent (LBA) - little endian
    uint32_t extent_lba_be;         // Location of extent (LBA) - big endian
    uint32_t data_length_le;        // Data length - little endian
    uint32_t data_length_be;        // Data length - big endian
    iso_datetime_t recording_date;  // Recording date and time
    uint8_t file_flags;             // File flags
    uint8_t file_unit_size;         // File unit size (for interleaved files)
    uint8_t interleave_gap;         // Interleave gap size
    uint16_t volume_seq_le;         // Volume sequence number - little endian
    uint16_t volume_seq_be;         // Volume sequence number - big endian
    uint8_t name_length;            // Length of file identifier
    char name[1];                   // File identifier (variable length)
} __attribute__((packed)) iso_directory_entry_t;

// ISO9660 Primary Volume Descriptor
typedef struct {
    uint8_t type;                   // Volume descriptor type (1 for primary)
    char id[5];                     // Standard identifier "CD001"
    uint8_t version;                // Volume descriptor version (1)
    uint8_t unused1;                // Unused
    char system_id[32];             // System identifier
    char volume_id[32];             // Volume identifier
    uint8_t unused2[8];             // Unused
    uint32_t volume_space_size_le;  // Volume space size - little endian
    uint32_t volume_space_size_be;  // Volume space size - big endian
    uint8_t unused3[32];            // Unused
    uint16_t volume_set_size_le;    // Volume set size - little endian
    uint16_t volume_set_size_be;    // Volume set size - big endian
    uint16_t volume_seq_number_le;  // Volume sequence number - little endian
    uint16_t volume_seq_number_be;  // Volume sequence number - big endian
    uint16_t logical_block_size_le; // Logical block size - little endian
    uint16_t logical_block_size_be; // Logical block size - big endian
    uint32_t path_table_size_le;    // Path table size - little endian
    uint32_t path_table_size_be;    // Path table size - big endian
    uint32_t type_l_path_table;     // Location of type L path table
    uint32_t opt_type_l_path_table; // Location of optional type L path table
    uint32_t type_m_path_table;     // Location of type M path table
    uint32_t opt_type_m_path_table; // Location of optional type M path table
    iso_directory_entry_t root_directory_entry; // Directory entry for root directory
    // ... more fields we don't need yet
} __attribute__((packed)) iso_primary_volume_descriptor_t;

// Function declarations
void iso9660_init(void);
int iso9660_mount(void* blockdev, const char* mountpoint);
int iso9660_read_file(const char* path, void* buffer, size_t size, size_t offset);

#endif // ISO9660_H
