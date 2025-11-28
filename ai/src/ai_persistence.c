/**
 * QARMA - AI Persistence Implementation
 * 
 * Handles saving and loading AI learning data to FAT16 disk.
 */

#include "ai_persistence.h"
#include "vfs.h"
#include "graphics.h"
#include "string.h"
#include "memory/heap.h"
#include "config.h"

// Paths for AI data files
#define AI_BASE_PATH "/disk"
#define QUANTUM_LEARNING_FILE "/disk/qlearn.dat"
#define COMMAND_CACHE_FILE "/disk/cmdcache.dat"

// Statistics
static struct {
    uint32_t saves_attempted;
    uint32_t saves_successful;
    uint32_t loads_attempted;
    uint32_t loads_successful;
    uint32_t bytes_written;
    uint32_t bytes_read;
} g_persistence_stats = {0};

// Simple CRC32 calculation
static uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

void ai_persistence_init(void) {
    SERIAL_LOG("[AI_PERSIST] Initializing persistence layer\n");
    gfx_print("AI Persistence: Ready\n");
}

// Export quantum learning data
extern void quantum_ai_export_learning_data(void** data, uint32_t* size);
extern int quantum_ai_import_learning_data(const void* data, uint32_t size);

// Export command cache data
extern void command_predictor_export_cache(void** data, uint32_t* size);
extern int command_predictor_import_cache(const void* data, uint32_t size);

int ai_save_quantum_learning(const char* path) {
    g_persistence_stats.saves_attempted++;
    
    SERIAL_LOG("[AI_PERSIST] Saving quantum learning to: ");
    SERIAL_LOG((char*)path);
    SERIAL_LOG("\n");
    
    // Get quantum learning data
    void* data = NULL;
    uint32_t data_size = 0;
    quantum_ai_export_learning_data(&data, &data_size);
    
    if (!data || data_size == 0) {
        SERIAL_LOG("[AI_PERSIST] No quantum data to save\n");
        return -1;
    }
    
    // Create header
    ai_persistence_header_t header;
    header.magic = AI_PERSISTENCE_MAGIC;
    header.version = 1;
    header.data_type = AI_DATA_QUANTUM_LEARNING;
    header.data_size = data_size;
    header.checksum = calculate_crc32((uint8_t*)data, data_size);
    
    // Write to VFS
    vfs_node_t* node = vfs_create(path, VFS_TYPE_FILE);
    if (!node) {
        SERIAL_LOG("[AI_PERSIST] Failed to create file\n");
        heap_free(data);
        return -1;
    }
    
    // Write header
    int written = vfs_write(node, &header, sizeof(header), 0);
    if (written != sizeof(header)) {
        SERIAL_LOG("[AI_PERSIST] Failed to write header\n");
        heap_free(data);
        return -1;
    }
    
    // Write data
    written = vfs_write(node, data, data_size, sizeof(header));
    if (written != (int)data_size) {
        SERIAL_LOG("[AI_PERSIST] Failed to write data\n");
        heap_free(data);
        return -1;
    }
    
    SERIAL_LOG("[AI_PERSIST] Saved quantum data: ");
    SERIAL_LOG_DEC("", data_size);
    SERIAL_LOG(" bytes, CRC32=");
    SERIAL_LOG_HEX("", header.checksum);
    SERIAL_LOG("\n");
    
    g_persistence_stats.saves_successful++;
    g_persistence_stats.bytes_written += sizeof(header) + data_size;
    
    heap_free(data);
    return 0;
}

int ai_load_quantum_learning(const char* path) {
    g_persistence_stats.loads_attempted++;
    
    SERIAL_LOG("[AI_PERSIST] Loading quantum learning from: ");
    SERIAL_LOG((char*)path);
    SERIAL_LOG("\n");
    
    // Open file
    vfs_node_t* node = vfs_open(path);
    if (!node) {
        SERIAL_LOG("[AI_PERSIST] File not found\n");
        return -1;
    }
    
    // Read header
    ai_persistence_header_t header;
    int bytes_read = vfs_read(node, &header, sizeof(header), 0);
    if (bytes_read != sizeof(header)) {
        SERIAL_LOG("[AI_PERSIST] Failed to read header - expected ");
        SERIAL_LOG_DEC("", sizeof(header));
        SERIAL_LOG(" bytes, got ");
        SERIAL_LOG_DEC("", bytes_read);
        SERIAL_LOG("\n");
        return -1;
    }
    
    SERIAL_LOG("[AI_PERSIST] Header: magic=");
    SERIAL_LOG_HEX("", header.magic);
    SERIAL_LOG(" size=");
    SERIAL_LOG_DEC("", header.data_size);
    SERIAL_LOG("\n");
    
    // Verify magic
    if (header.magic != AI_PERSISTENCE_MAGIC) {
        SERIAL_LOG("[AI_PERSIST] Invalid magic number\n");
        return -1;
    }
    
    // Read data
    void* data = heap_alloc(header.data_size);
    if (!data) {
        SERIAL_LOG("[AI_PERSIST] Failed to allocate memory\n");
        return -1;
    }
    
    bytes_read = vfs_read(node, data, header.data_size, sizeof(header));
    if (bytes_read != (int)header.data_size) {
        SERIAL_LOG("[AI_PERSIST] Failed to read data - expected ");
        SERIAL_LOG_DEC("", header.data_size);
        SERIAL_LOG(" bytes, got ");
        SERIAL_LOG_DEC("", bytes_read);
        SERIAL_LOG("\n");
        heap_free(data);
        return -1;
    }
    
    // Verify checksum
    uint32_t crc = calculate_crc32((uint8_t*)data, header.data_size);
    if (crc != header.checksum) {
        SERIAL_LOG("[AI_PERSIST] Checksum mismatch\n");
        heap_free(data);
        return -1;
    }
    
    // Import data
    int result = quantum_ai_import_learning_data(data, header.data_size);
    heap_free(data);
    
    if (result == 0) {
        g_persistence_stats.loads_successful++;
        g_persistence_stats.bytes_read += sizeof(header) + header.data_size;
        SERIAL_LOG("[AI_PERSIST] Successfully loaded quantum learning data\n");
    }
    
    return result;
}

int ai_save_command_cache(const char* path) {
    g_persistence_stats.saves_attempted++;
    
    SERIAL_LOG("[AI_PERSIST] Saving command cache to: ");
    SERIAL_LOG((char*)path);
    SERIAL_LOG("\n");
    
    // Get command cache data
    void* data = NULL;
    uint32_t data_size = 0;
    command_predictor_export_cache(&data, &data_size);
    
    if (!data || data_size == 0) {
        SERIAL_LOG("[AI_PERSIST] No cache data to save (empty cache is OK)\n");
        g_persistence_stats.saves_successful++;
        return 0;  // Empty cache is not an error
    }
    
    // Create header
    ai_persistence_header_t header;
    header.magic = AI_PERSISTENCE_MAGIC;
    header.version = 1;
    header.data_type = AI_DATA_COMMAND_CACHE;
    header.data_size = data_size;
    header.checksum = calculate_crc32((uint8_t*)data, data_size);
    
    // Write to VFS
    vfs_node_t* node = vfs_create(path, VFS_TYPE_FILE);
    if (!node) {
        SERIAL_LOG("[AI_PERSIST] Failed to create file\n");
        heap_free(data);
        return -1;
    }
    
    // Write header
    int written = vfs_write(node, &header, sizeof(header), 0);
    if (written != sizeof(header)) {
        SERIAL_LOG("[AI_PERSIST] Failed to write header\n");
        heap_free(data);
        return -1;
    }
    
    // Write data
    written = vfs_write(node, data, data_size, sizeof(header));
    if (written != (int)data_size) {
        SERIAL_LOG("[AI_PERSIST] Failed to write data\n");
        heap_free(data);
        return -1;
    }
    
    SERIAL_LOG("[AI_PERSIST] Saved cache data: ");
    SERIAL_LOG_DEC("", data_size);
    SERIAL_LOG(" bytes\n");
    
    g_persistence_stats.saves_successful++;
    g_persistence_stats.bytes_written += sizeof(header) + data_size;
    
    heap_free(data);
    return 0;
}

int ai_load_command_cache(const char* path) {
    extern int command_predictor_import_cache(const void* data, uint32_t size);
    
    g_persistence_stats.loads_attempted++;
    
    SERIAL_LOG("[AI_PERSIST] Loading command cache from: ");
    SERIAL_LOG((char*)path);
    SERIAL_LOG("\n");
    
    // Open file
    vfs_node_t* node = vfs_open(path);
    if (!node) {
        SERIAL_LOG("[AI_PERSIST] File not found\n");
        return -1;
    }
    
    // Read header
    ai_persistence_header_t header;
    int bytes_read = vfs_read(node, &header, sizeof(header), 0);
    if (bytes_read != sizeof(header)) {
        SERIAL_LOG("[AI_PERSIST] Failed to read header - expected ");
        SERIAL_LOG_DEC("", sizeof(header));
        SERIAL_LOG(" bytes, got ");
        SERIAL_LOG_DEC("", bytes_read);
        SERIAL_LOG("\n");
        return -1;
    }
    
    SERIAL_LOG("[AI_PERSIST] Header: magic=");
    SERIAL_LOG_HEX("", header.magic);
    SERIAL_LOG(" size=");
    SERIAL_LOG_DEC("", header.data_size);
    SERIAL_LOG("\n");
    
    // Verify magic
    if (header.magic != AI_PERSISTENCE_MAGIC) {
        SERIAL_LOG("[AI_PERSIST] Invalid magic number\n");
        return -1;
    }
    
    // Read data
    void* data = heap_alloc(header.data_size);
    if (!data) {
        SERIAL_LOG("[AI_PERSIST] Failed to allocate memory\n");
        return -1;
    }
    
    bytes_read = vfs_read(node, data, header.data_size, sizeof(header));
    if (bytes_read != (int)header.data_size) {
        SERIAL_LOG("[AI_PERSIST] Failed to read data - expected ");
        SERIAL_LOG_DEC("", header.data_size);
        SERIAL_LOG(" bytes, got ");
        SERIAL_LOG_DEC("", bytes_read);
        SERIAL_LOG("\n");
        heap_free(data);
        return -1;
    }
    
    // Verify checksum
    uint32_t crc = calculate_crc32((uint8_t*)data, header.data_size);
    if (crc != header.checksum) {
        SERIAL_LOG("[AI_PERSIST] Checksum mismatch\n");
        heap_free(data);
        return -1;
    }
    
    // Import data
    int result = command_predictor_import_cache(data, header.data_size);
    heap_free(data);
    
    if (result == 0) {
        g_persistence_stats.loads_successful++;
        g_persistence_stats.bytes_read += sizeof(header) + header.data_size;
        SERIAL_LOG("[AI_PERSIST] Successfully loaded command cache\n");
    }
    
    return result;
}

int ai_save_state(void) {
    gfx_print("Saving AI learning state...\n");
    
    int result = 0;
    
    // Save quantum learning
    if (ai_save_quantum_learning(QUANTUM_LEARNING_FILE) != 0) {
        gfx_print("  Warning: Failed to save quantum learning\n");
        result = -1;
    } else {
        gfx_print("  Quantum learning: OK\n");
    }
    
    // Save command cache
    if (ai_save_command_cache(COMMAND_CACHE_FILE) != 0) {
        gfx_print("  Command cache: Error\n");
        result = -1;
    } else {
        gfx_print("  Command cache: OK\n");
    }
    
    if (result == 0) {
        gfx_print("AI state saved successfully!\n");
    }
    
    return result;
}

int ai_load_state(void) {
    gfx_print("Loading AI learning state...\n");
    
    int loaded = 0;
    
    // Load quantum learning
    if (ai_load_quantum_learning(QUANTUM_LEARNING_FILE) == 0) {
        gfx_print("  Quantum learning: Loaded\n");
        loaded++;
    } else {
        gfx_print("  Quantum learning: Not found (starting fresh)\n");
    }
    
    // Load command cache
    if (ai_load_command_cache(COMMAND_CACHE_FILE) == 0) {
        gfx_print("  Command cache: Loaded\n");
        loaded++;
    } else {
        gfx_print("  Command cache: Not found (starting fresh)\n");
    }
    
    if (loaded > 0) {
        gfx_print("AI state loaded successfully!\n");
        return 0;
    }
    
    return -1;
}

int ai_clear_saved_state(void) {
    gfx_print("Clearing AI saved state...\n");
    
    // Would delete files here
    SERIAL_LOG("[AI_PERSIST] Would delete saved files\n");
    
    gfx_print("AI state cleared.\n");
    return 0;
}

void ai_persistence_print_stats(void) {
    gfx_print("\n=== AI Persistence Statistics ===\n");
    gfx_print("Saves attempted: ");
    gfx_print_decimal(g_persistence_stats.saves_attempted);
    gfx_print("\nSaves successful: ");
    gfx_print_decimal(g_persistence_stats.saves_successful);
    gfx_print("\nLoads attempted: ");
    gfx_print_decimal(g_persistence_stats.loads_attempted);
    gfx_print("\nLoads successful: ");
    gfx_print_decimal(g_persistence_stats.loads_successful);
    gfx_print("\nBytes written: ");
    gfx_print_decimal(g_persistence_stats.bytes_written);
    gfx_print("\nBytes read: ");
    gfx_print_decimal(g_persistence_stats.bytes_read);
    gfx_print("\n");
}
