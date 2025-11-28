#include "iso9660.h"
#include "core/string.h"
#include "core/blockdev.h"
#include "vfs.h"

extern void gfx_print(const char*);

static iso_primary_volume_descriptor_t primary_vd_storage;
static iso_primary_volume_descriptor_t* primary_vd = NULL;
static blockdev_t* cdrom_device = NULL;

// Read a sector from the CD-ROM (2048 bytes per sector for ISO9660)
static int read_sector(uint32_t lba, void* buffer) {
    if (!cdrom_device) {
        gfx_print("[ISO9660] No CD-ROM device available\n");
        return -1;
    }
    
    int result = cdrom_device->read(cdrom_device, lba, buffer, 1);
    return (result == 1) ? 0 : -1;
}

// Find a file in a directory
static iso_directory_entry_t* find_file_in_directory(uint32_t dir_lba, uint32_t dir_size, const char* filename) {
    static uint8_t sector_buffer[2048];
    uint32_t sectors = (dir_size + 2047) / 2048;
    
    for (uint32_t i = 0; i < sectors; i++) {
        if (read_sector(dir_lba + i, sector_buffer) < 0) {
            return NULL;
        }
        
        uint32_t offset = 0;
        while (offset < 2048 && offset < dir_size - (i * 2048)) {
            iso_directory_entry_t* entry = (iso_directory_entry_t*)(sector_buffer + offset);
            
            if (entry->length == 0) break;
            
            // Skip '.' and '..' entries
            if (entry->name_length > 0 && entry->name[0] != 0 && entry->name[0] != 1) {
                // Compare filename (ISO9660 names are uppercase and may have version suffix)
                char name_buf[256];
                uint32_t name_len = entry->name_length;
                
                // Copy name and remove version suffix (;1)
                for (uint32_t j = 0; j < name_len && j < 255; j++) {
                    if (entry->name[j] == ';') {
                        name_len = j;
                        break;
                    }
                    name_buf[j] = entry->name[j];
                }
                name_buf[name_len] = '\0';
                
                // Simple case-insensitive compare
                if (strlen(filename) == name_len) {
                    bool match = true;
                    for (uint32_t j = 0; j < name_len; j++) {
                        char c1 = filename[j];
                        char c2 = name_buf[j];
                        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
                        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
                        if (c1 != c2) {
                            match = false;
                            break;
                        }
                    }
                    
                    if (match) {
                        // Found it! Return a copy
                        static iso_directory_entry_t found_entry;
                        memcpy(&found_entry, entry, entry->length);
                        return &found_entry;
                    }
                }
            }
            
            offset += entry->length;
        }
    }
    
    return NULL;
}

void iso9660_init(void) {
    gfx_print("[ISO9660] Initializing ISO9660 filesystem driver\n");
}

int iso9660_mount(void* blockdev, const char* mountpoint) {
    extern void serial_debug(const char*);
    (void)mountpoint;
    
    serial_debug("[ISO9660] Mount function called\n");
    cdrom_device = (blockdev_t*)blockdev;
    serial_debug("[ISO9660] Block device set\n");
    
    gfx_print("[ISO9660] Attempting to mount ISO9660 filesystem\n");
    
    // Read the primary volume descriptor (sector 16)
    uint8_t vd_buffer[2048];
    
    serial_debug("[ISO9660] Reading volume descriptor from sector 16\n");
    if (read_sector(16, vd_buffer) < 0) {
        serial_debug("[ISO9660] FAILED to read volume descriptor\n");
        gfx_print("[ISO9660] Failed to read volume descriptor\n");
        return -1;
    }
    serial_debug("[ISO9660] Volume descriptor read successfully\n");
    
    // Copy to persistent storage
    memcpy(&primary_vd_storage, vd_buffer, sizeof(iso_primary_volume_descriptor_t));
    primary_vd = &primary_vd_storage;
    
    // Verify it's a valid ISO9660 volume
    serial_debug("[ISO9660] Validating volume descriptor\n");
    if (primary_vd->type != ISO_VD_PRIMARY || 
        memcmp(primary_vd->id, "CD001", 5) != 0) {
        serial_debug("[ISO9660] Invalid volume descriptor!\n");
        gfx_print("[ISO9660] Invalid ISO9660 volume descriptor\n");
        primary_vd = NULL;
        return -1;
    }
    
    serial_debug("[ISO9660] Valid ISO9660 volume!\n");
    gfx_print("[ISO9660] Valid ISO9660 filesystem found\n");
    gfx_print("[ISO9660] Volume ID: ");
    gfx_print(primary_vd->volume_id);
    gfx_print("\n");
    serial_debug("[ISO9660] Mount completed successfully\n");
    
    return 0;
}

int iso9660_read_file(const char* path, void* buffer, size_t size, size_t offset) {
    extern void serial_debug(const char*);
    
    serial_debug("[ISO9660] read_file called\n");
    if (!primary_vd) {
        serial_debug("[ISO9660] ERROR: Filesystem not mounted\n");
        gfx_print("[ISO9660] Filesystem not mounted\n");
        return -1;
    }
    serial_debug("[ISO9660] Filesystem is mounted\n");
    
    // For simplicity, only support root directory files for now
    if (path[0] != '/') {
        serial_debug("[ISO9660] ERROR: Path must start with /\n");
        return -1;
    }
    
    const char* filename = path + 1; // Skip leading /
    
    serial_debug("[ISO9660] Looking for file: ");
    serial_debug(filename);
    serial_debug("\n");
    gfx_print("[ISO9660] Looking for file: ");
    gfx_print(filename);
    gfx_print("\n");
    
    // Get root directory location
    uint32_t root_lba = primary_vd->root_directory_entry.extent_lba_le;
    uint32_t root_size = primary_vd->root_directory_entry.data_length_le;
    serial_debug("[ISO9660] Root directory at LBA: ");
    // Simple number print
    char lba_str[16];
    uint32_t temp = root_lba;
    int idx = 0;
    if (temp == 0) lba_str[idx++] = '0';
    else {
        char digits[16];
        int d = 0;
        while (temp > 0) { digits[d++] = '0' + (temp % 10); temp /= 10; }
        for (int i = d-1; i >= 0; i--) lba_str[idx++] = digits[i];
    }
    lba_str[idx] = '\0';
    serial_debug(lba_str);
    serial_debug("\n");
    
    // Find the file
    serial_debug("[ISO9660] Searching directory...\n");
    iso_directory_entry_t* file_entry = find_file_in_directory(root_lba, root_size, filename);
    
    if (!file_entry) {
        serial_debug("[ISO9660] File not found!\n");
        gfx_print("[ISO9660] File not found\n");
        return -1;
    }
    
    serial_debug("[ISO9660] File found!\n");
    gfx_print("[ISO9660] File found!\n");
    
    // Read the file data
    uint32_t file_lba = file_entry->extent_lba_le;
    uint32_t file_size = file_entry->data_length_le;
    
    serial_debug("[ISO9660] File LBA: ");
    char lba_str2[16];
    uint32_t temp2 = file_lba;
    int idx2 = 0;
    if (temp2 == 0) lba_str2[idx2++] = '0';
    else {
        char digits2[16];
        int d2 = 0;
        while (temp2 > 0) { digits2[d2++] = '0' + (temp2 % 10); temp2 /= 10; }
        for (int i = d2-1; i >= 0; i--) lba_str2[idx2++] = digits2[i];
    }
    lba_str2[idx2] = '\0';
    serial_debug(lba_str2);
    serial_debug(" Size: ");
    char size_str[16];
    temp2 = file_size;
    idx2 = 0;
    if (temp2 == 0) size_str[idx2++] = '0';
    else {
        char digits3[16];
        int d3 = 0;
        while (temp2 > 0) { digits3[d3++] = '0' + (temp2 % 10); temp2 /= 10; }
        for (int i = d3-1; i >= 0; i--) size_str[idx2++] = digits3[i];
    }
    size_str[idx2] = '\0';
    serial_debug(size_str);
    serial_debug("\n");
    
    // Limit read to actual file size
    if (offset >= file_size) {
        serial_debug("[ISO9660] Offset >= file size\n");
        return 0;
    }
    
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    serial_debug("[ISO9660] Starting file read\n");
    
    // Calculate starting sector and offset within that sector
    uint32_t start_sector = file_lba + (offset / 2048);
    uint32_t sector_offset = offset % 2048;
    uint32_t bytes_read = 0;
    
    static uint8_t sector_buffer[2048];
    
    while (bytes_read < size) {
        if (read_sector(start_sector, sector_buffer) < 0) {
            serial_debug("[ISO9660] ERROR reading sector\n");
            return bytes_read;
        }
        
        uint32_t to_copy = 2048 - sector_offset;
        if (to_copy > size - bytes_read) {
            to_copy = size - bytes_read;
        }
        
        memcpy((uint8_t*)buffer + bytes_read, sector_buffer + sector_offset, to_copy);
        
        bytes_read += to_copy;
        start_sector++;
        sector_offset = 0;
    }
    
    serial_debug("[ISO9660] File read complete: ");
    char read_str[16];
    temp2 = bytes_read;
    idx2 = 0;
    if (temp2 == 0) read_str[idx2++] = '0';
    else {
        char digits4[16];
        int d4 = 0;
        while (temp2 > 0) { digits4[d4++] = '0' + (temp2 % 10); temp2 /= 10; }
        for (int i = d4-1; i >= 0; i--) read_str[idx2++] = digits4[i];
    }
    read_str[idx2] = '\0';
    serial_debug(read_str);
    serial_debug(" bytes\n");
    
    return bytes_read;
}
