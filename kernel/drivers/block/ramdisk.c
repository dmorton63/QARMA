#include "core/blockdev.h"
#include "core/string.h"
#include "core/stdtools.h"
#include "fs/vfs.h"

#define RAMDISK_SIZE (128 * 1024) // 128 KiB
#define RAMDISK_BLOCK_SIZE 512
#define MAX_FILES 16

static uint8_t ramdisk_data[RAMDISK_SIZE];

// Simple filesystem structure for demo
typedef struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
    uint32_t used;
} simple_file_entry_t;

typedef struct {
    uint32_t magic;  // 0x51554144 "QUAD" 
    uint32_t file_count;
    simple_file_entry_t files[MAX_FILES];
} simple_fs_header_t;

static simple_fs_header_t* fs_header = (simple_fs_header_t*)ramdisk_data;

static int ramdisk_read(blockdev_t* dev, uint64_t lba, void* buf, size_t count) {
    if (!dev || !buf) return -1;
    if ((lba + count) * RAMDISK_BLOCK_SIZE > RAMDISK_SIZE) return -1;
    memcpy(buf, &ramdisk_data[lba * RAMDISK_BLOCK_SIZE], count * RAMDISK_BLOCK_SIZE);
    return 0;
}

static int ramdisk_write(blockdev_t* dev, uint64_t lba, const void* buf, size_t count) {
    if (!dev || !buf) return -1;
    if ((lba + count) * RAMDISK_BLOCK_SIZE > RAMDISK_SIZE) return -1;
    memcpy(&ramdisk_data[lba * RAMDISK_BLOCK_SIZE], buf, count * RAMDISK_BLOCK_SIZE);
    return 0;
}

static blockdev_t ramdisk_dev = {
    .type = BLOCKDEV_TYPE_RAMDISK,
    .name = "ram0",
    .num_blocks = RAMDISK_SIZE / RAMDISK_BLOCK_SIZE,
    .block_size = RAMDISK_BLOCK_SIZE,
    .driver_data = 0,
    .read = ramdisk_read,
    .write = ramdisk_write,
    .next = 0
};

void ramdisk_init(void) {
    memset(ramdisk_data, 0, sizeof(ramdisk_data));
    
    // Initialize simple filesystem
    fs_header->magic = 0x51554144; // "QUAD"
    fs_header->file_count = 0;
    
    // Create test files
    uint32_t data_offset = sizeof(simple_fs_header_t);
    
    // File 1: boot config
    const char* boot_config = "# QARMA Boot Configuration\nverbose=1\ndebug=1\nboot_delay=3\n";
    simple_file_entry_t* file1 = &fs_header->files[fs_header->file_count++];
    strcpy(file1->name, "config.txt");
    file1->offset = data_offset;
    file1->size = strlen(boot_config);
    file1->used = 1;
    memcpy(&ramdisk_data[data_offset], boot_config, file1->size);
    data_offset += file1->size;
    
    // File 2: kernel log
    const char* kernel_log = "[BOOT] QARMA Kernel Starting\n[INIT] Memory manager initialized\n[INIT] VFS mounted\n";
    simple_file_entry_t* file2 = &fs_header->files[fs_header->file_count++];
    strcpy(file2->name, "kernel.log");
    file2->offset = data_offset;
    file2->size = strlen(kernel_log);
    file2->used = 1;
    memcpy(&ramdisk_data[data_offset], kernel_log, file2->size);
    data_offset += file2->size;
    
    // File 3: system info
    const char* sys_info = "QARMA v1.0\nArchitecture: x86\nMemory: Available\nSubsystems: Video, Filesystem\n";
    simple_file_entry_t* file3 = &fs_header->files[fs_header->file_count++];
    strcpy(file3->name, "sysinfo.txt");
    file3->offset = data_offset;
    file3->size = strlen(sys_info);
    file3->used = 1;
    memcpy(&ramdisk_data[data_offset], sys_info, file3->size);
    
    blockdev_register(&ramdisk_dev);
}
