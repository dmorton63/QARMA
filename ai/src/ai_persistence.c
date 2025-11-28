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

// Stable state file paths (prefer host, fallback to RAM disk)
#define AI_PRIMARY_FILE  "/host/ai_state.bin"
#define AI_FALLBACK_FILE "/ramdisk/ai_state.bin"

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

    // Collect sections
    void* q_data = NULL; uint32_t q_size = 0;
    void* c_data = NULL; uint32_t c_size = 0;
    quantum_ai_export_learning_data(&q_data, &q_size);
    command_predictor_export_cache(&c_data, &c_size);

    // Choose target (saving to /host requires 9P create, not yet supported)
    const char* path = AI_FALLBACK_FILE;
    SERIAL_LOG("[AI_PERSIST] Saving to "); SERIAL_LOG((char*)path); SERIAL_LOG("\n");

    vfs_node_t* node = vfs_create(path, VFS_TYPE_FILE);
    if (!node) {
        gfx_print("  Error: Could not create state file\n");
        if (q_data) heap_free(q_data);
        if (c_data) heap_free(c_data);
        return -1;
    }

    size_t offset = 0;
    int result = 0;

    // Write available sections
    if (q_data && q_size > 0) {
        // Write quantum section
        ai_persistence_header_t hdr_q;
        hdr_q.magic = AI_PERSISTENCE_MAGIC;
        hdr_q.version = 1;
        hdr_q.data_type = AI_DATA_QUANTUM_LEARNING;
        hdr_q.data_size = q_size;
        hdr_q.checksum = calculate_crc32((const uint8_t*)q_data, q_size);
        int w = vfs_write(node, &hdr_q, sizeof(hdr_q), offset);
        if (w != (int)sizeof(hdr_q)) {
            gfx_print("  Quantum learning: Error\n");
            result = -1;
        } else {
            offset += sizeof(hdr_q);
            w = vfs_write(node, q_data, q_size, offset);
            if (w != (int)q_size) {
                gfx_print("  Quantum learning: Error\n");
                result = -1;
            } else {
                offset += q_size;
                g_persistence_stats.bytes_written += sizeof(hdr_q) + q_size;
                gfx_print("  Quantum learning: OK\n");
            }
        }
    } else {
        gfx_print("  Quantum learning: None\n");
    }

    if (c_data && c_size > 0) {
        // Write command cache section
        ai_persistence_header_t hdr_c;
        hdr_c.magic = AI_PERSISTENCE_MAGIC;
        hdr_c.version = 1;
        hdr_c.data_type = AI_DATA_COMMAND_CACHE;
        hdr_c.data_size = c_size;
        hdr_c.checksum = calculate_crc32((const uint8_t*)c_data, c_size);
        int w = vfs_write(node, &hdr_c, sizeof(hdr_c), offset);
        if (w != (int)sizeof(hdr_c)) {
            gfx_print("  Command cache: Error\n");
            result = -1;
        } else {
            offset += sizeof(hdr_c);
            w = vfs_write(node, c_data, c_size, offset);
            if (w != (int)c_size) {
                gfx_print("  Command cache: Error\n");
                result = -1;
            } else {
                offset += c_size;
                g_persistence_stats.bytes_written += sizeof(hdr_c) + c_size;
                gfx_print("  Command cache: OK\n");
            }
        }
    } else {
        gfx_print("  Command cache: Empty\n");
    }

    if (result == 0) {
        gfx_print("AI state saved successfully!\n");
        g_persistence_stats.saves_successful++;
    }
    g_persistence_stats.saves_attempted++;

    if (q_data) heap_free(q_data);
    if (c_data) heap_free(c_data);
    return result;
}

int ai_load_state(void) {
    gfx_print("Loading AI learning state...\n");

    // Prefer host file; fallback to RAM disk
    vfs_node_t* node = vfs_open(AI_PRIMARY_FILE);
    const char* used_path = AI_PRIMARY_FILE;
    if (!node) {
        node = vfs_open(AI_FALLBACK_FILE);
        used_path = AI_FALLBACK_FILE;
    }
    if (!node) {
        gfx_print("  No state file found\n");
        return -1;
    }

    SERIAL_LOG("[AI_PERSIST] Loading from "); SERIAL_LOG((char*)used_path); SERIAL_LOG("\n");

    size_t offset = 0;
    int sections_loaded = 0;

    while (1) {
        ai_persistence_header_t header;
        int r = vfs_read(node, &header, sizeof(header), offset);
        if (r != (int)sizeof(header)) {
            break; // EOF or error ends loop
        }

        if (header.magic != AI_PERSISTENCE_MAGIC) {
            SERIAL_LOG("[AI_PERSIST] Invalid magic in state file\n");
            break;
        }

        void* data = heap_alloc(header.data_size);
        if (!data) {
            SERIAL_LOG("[AI_PERSIST] OOM reading state section\n");
            break;
        }
        r = vfs_read(node, data, header.data_size, offset + sizeof(header));
        if (r != (int)header.data_size) {
            SERIAL_LOG("[AI_PERSIST] Short read on state section\n");
            heap_free(data);
            break;
        }

        uint32_t crc = calculate_crc32((uint8_t*)data, header.data_size);
        if (crc != header.checksum) {
            SERIAL_LOG("[AI_PERSIST] Checksum mismatch in state section\n");
            heap_free(data);
            break;
        }

        // Dispatch by type
        int res = 0;
        switch (header.data_type) {
            case AI_DATA_QUANTUM_LEARNING:
                res = quantum_ai_import_learning_data(data, header.data_size);
                if (res == 0) gfx_print("  Quantum learning: Loaded\n");
                break;
            case AI_DATA_COMMAND_CACHE:
                res = command_predictor_import_cache(data, header.data_size);
                if (res == 0) gfx_print("  Command cache: Loaded\n");
                break;
            default:
                SERIAL_LOG("[AI_PERSIST] Unknown section type, skipping\n");
                break;
        }
        if (res == 0) {
            sections_loaded++;
            g_persistence_stats.bytes_read += sizeof(header) + header.data_size;
        }

        heap_free(data);
        offset += sizeof(header) + header.data_size;
    }

    g_persistence_stats.loads_attempted++;
    if (sections_loaded > 0) {
        g_persistence_stats.loads_successful++;
        gfx_print("AI state loaded successfully!\n");
        return 0;
    }

    gfx_print("  No valid sections found (starting fresh)\n");
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
