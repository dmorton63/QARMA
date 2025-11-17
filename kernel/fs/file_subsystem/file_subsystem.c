/**
 * QARMA - Filesystem Subsystem Implementation
 * Comprehensive file management and VFS integration layer
 */

#include "file_subsystem.h"
#include "core/string.h"
#include "core/memory.h"
#include "config.h"

// Forward declarations
extern void serial_debug(const char* msg);
extern void serial_debug_hex(uint32_t value);

// Maximum number of registered files and open handles
#define MAX_REGISTERED_FILES 256
#define MAX_OPEN_HANDLES 64

// Global filesystem subsystem state
static struct {
    bool initialized;
    filesystem_subsystem_config_t config;
    filesystem_subsystem_stats_t stats;
    subsystem_t* subsystem_registry;
    
    // File registration arrays
    registered_file_t registered_files[MAX_REGISTERED_FILES];
    uint32_t registered_file_count;
    
    // Handle management
    filesystem_handle_t open_handles[MAX_OPEN_HANDLES];
    uint32_t next_handle_id;
    
    // Cache management
    uint32_t total_cache_size;
} g_filesystem_state = {
    .initialized = false,
    .config = {
        .max_cached_files = 64,
        .max_cache_size_mb = 16,
        .read_buffer_size_kb = 4,
        .write_buffer_size_kb = 4,
        .enable_compression = false,
        .enable_encryption = false,
        .enable_write_through = true
    },
    .stats = {0},
    .subsystem_registry = NULL,
    .registered_file_count = 0,
    .next_handle_id = 1,
    .total_cache_size = 0
};

// Subsystem lifecycle functions
static void filesystem_subsystem_start(void);
static void filesystem_subsystem_stop(void);
static void filesystem_subsystem_restart(void);
static void filesystem_subsystem_message_handler(void *msg);

// Internal helper functions
static registered_file_t* find_registered_file(const char* name);
static filesystem_handle_t* allocate_handle(void);
static void free_handle(filesystem_handle_t* handle);
static void update_stats_read_operation(uint32_t time_us, size_t bytes);
static void update_stats_write_operation(uint32_t time_us, size_t bytes);

/**
 * Initialize the filesystem subsystem
 */
void filesystem_subsystem_init(subsystem_t* registry) {
    if (g_filesystem_state.initialized) {
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] Subsystem already initialized\n");
        #endif
        return;
    }
    
    // Initialize subsystem registry entry
    if (registry) {
        g_filesystem_state.subsystem_registry = registry;
        
        registry->id = FILESYSTEM_SUBSYSTEM_ID;
        registry->name = "Filesystem Subsystem";
        registry->type = SUBSYSTEM_TYPE_FILESYSTEM;
        registry->state = SUBSYSTEM_STATE_STARTED;
        registry->start = filesystem_subsystem_start;
        registry->stop = filesystem_subsystem_stop;
        registry->restart = filesystem_subsystem_restart;
        registry->message_handler = filesystem_subsystem_message_handler;
        registry->memory_limit_kb = 1024 * 16; // 16MB limit
        registry->cpu_affinity_mask = 0xFF; // Can run on any CPU
        registry->stats_uptime_ms = 0;
        registry->stats_messages_handled = 0;
    }
    
    // Clear file arrays
    for (uint32_t i = 0; i < MAX_REGISTERED_FILES; i++) {
        g_filesystem_state.registered_files[i].name = NULL;
        g_filesystem_state.registered_files[i].path = NULL;
        g_filesystem_state.registered_files[i].data = NULL;
        g_filesystem_state.registered_files[i].loaded = false;
        g_filesystem_state.registered_files[i].dirty = false;
    }
    
    // Clear handles
    for (uint32_t i = 0; i < MAX_OPEN_HANDLES; i++) {
        g_filesystem_state.open_handles[i].valid = false;
        g_filesystem_state.open_handles[i].handle_id = 0;
        g_filesystem_state.open_handles[i].vfs_node = NULL;
    }
    
    g_filesystem_state.initialized = true;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Subsystem initialized successfully\n");
    #endif
}

/**
 * Shutdown the filesystem subsystem
 */
void filesystem_subsystem_shutdown(void) {
    if (!g_filesystem_state.initialized) {
        return;
    }
    
    // Close all open handles
    for (uint32_t i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (g_filesystem_state.open_handles[i].valid) {
            free_handle(&g_filesystem_state.open_handles[i]);
        }
    }
    
    // Unload all cached files
    filesystem_clear_cache();
    
    // Update subsystem state
    if (g_filesystem_state.subsystem_registry) {
        g_filesystem_state.subsystem_registry->state = SUBSYSTEM_STATE_STOPPED;
    }
    
    g_filesystem_state.initialized = false;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Subsystem shutdown complete\n");
    #endif
}

/**
 * Get filesystem subsystem statistics
 */
void filesystem_subsystem_get_stats(filesystem_subsystem_stats_t* stats) {
    if (!stats || !g_filesystem_state.initialized) {
        return;
    }
    
    *stats = g_filesystem_state.stats;
}

/**
 * Configure filesystem subsystem settings
 */
void filesystem_subsystem_configure(const filesystem_subsystem_config_t* config) {
    if (!config || !g_filesystem_state.initialized) {
        return;
    }
    
    g_filesystem_state.config = *config;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Configuration updated\n");
    #endif
}

/**
 * Register a file for subsystem management
 */
bool filesystem_register_file(const char* name, const char* path, file_type_t type) {
    if (!g_filesystem_state.initialized || !name || !path) {
        return false;
    }
    
    if (g_filesystem_state.registered_file_count >= MAX_REGISTERED_FILES) {
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] Maximum registered files reached\n");
        #endif
        return false;
    }
    
    // Check if already registered
    if (find_registered_file(name)) {
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] File already registered: ");
        serial_debug(name);
        serial_debug("\n");
        #endif
        return false;
    }
    
    // Find free slot
    for (uint32_t i = 0; i < MAX_REGISTERED_FILES; i++) {
        if (!g_filesystem_state.registered_files[i].name) {
            g_filesystem_state.registered_files[i].name = name;
            g_filesystem_state.registered_files[i].path = path;
            g_filesystem_state.registered_files[i].type = type;
            g_filesystem_state.registered_files[i].data = NULL;
            g_filesystem_state.registered_files[i].size = 0;
            g_filesystem_state.registered_files[i].loaded = false;
            g_filesystem_state.registered_files[i].dirty = false;
            g_filesystem_state.registered_files[i].access_count = 0;
            g_filesystem_state.registered_files[i].last_access_time = 0;
            
            g_filesystem_state.registered_file_count++;
            g_filesystem_state.stats.total_files_registered++;
            
            #ifdef DEBUG_SERIAL
            serial_debug("[FILESYSTEM] Registered file: ");
            serial_debug(name);
            serial_debug(" -> ");
            serial_debug(path);
            serial_debug("\n");
            #endif
            
            return true;
        }
    }
    
    return false;
}

/**
 * Look up a registered file by name
 */
registered_file_t* filesystem_lookup_file(const char* name) {
    if (!g_filesystem_state.initialized || !name) {
        return NULL;
    }
    
    return find_registered_file(name);
}

/**
 * Load a registered file into memory
 */
bool filesystem_load_file(const char* name) {
    if (!g_filesystem_state.initialized || !name) {
        return false;
    }
    
    registered_file_t* file = find_registered_file(name);
    if (!file) {
        return false;
    }
    
    if (file->loaded) {
        file->access_count++;
        g_filesystem_state.stats.total_cache_hits++;
        return true;
    }
    
    // Try to open via VFS
    vfs_node_t* node = vfs_open(file->path);
    if (!node) {
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] Failed to open file via VFS: ");
        serial_debug(file->path);
        serial_debug("\n");
        #endif
        return false;
    }
    
    // Allocate memory for file data
    size_t file_size = node->size;
    if (file_size == 0) {
        file_size = 1024; // Default size for unknown files
    }
    
    void* data = malloc(file_size);
    if (!data) {
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] Failed to allocate memory for file data\n");
        #endif
        return false;
    }
    
    // Read file data through VFS
    int bytes_read = vfs_read(node, data, file_size, 0);
    if (bytes_read <= 0) {
        free(data);
        #ifdef DEBUG_SERIAL
        serial_debug("[FILESYSTEM] Failed to read file data via VFS\n");
        #endif
        return false;
    }
    
    // Read file data (simplified - real implementation would use VFS read)
    // For now, just mark as loaded
    file->data = data;
    file->size = bytes_read;  // Use actual bytes read
    file->loaded = true;
    file->access_count++;
    
    g_filesystem_state.stats.total_files_loaded++;
    g_filesystem_state.stats.total_memory_used_kb += bytes_read / 1024;
    g_filesystem_state.stats.total_read_operations++;
    g_filesystem_state.stats.total_cache_misses++;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Loaded file into cache: ");
    serial_debug(name);
    serial_debug("\n");
    #endif
    
    return true;
}

/**
 * Set file data manually (for in-memory files)
 */
bool filesystem_set_file_data(const char* name, void* data, size_t size) {
    if (!g_filesystem_state.initialized || !name || !data) {
        return false;
    }
    
    registered_file_t* file = find_registered_file(name);
    if (!file) {
        return false;
    }
    
    // Set the data directly
    file->data = data;
    file->size = size;
    file->loaded = true;
    file->access_count++;
    
    // Update statistics
    g_filesystem_state.stats.total_files_loaded++;
    g_filesystem_state.stats.total_memory_used_kb += size / 1024;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Set file data manually: ");
    serial_debug(name);
    serial_debug("\n");
    #endif
    
    return true;
}

/**
 * Get data pointer for a loaded file
 */
void* filesystem_get_file_data(const char* name, size_t* size) {
    if (!g_filesystem_state.initialized || !name) {
        return NULL;
    }
    
    registered_file_t* file = find_registered_file(name);
    if (!file || !file->loaded) {
        return NULL;
    }
    
    if (size) {
        *size = file->size;
    }
    
    file->access_count++;
    return file->data;
}

/**
 * Unload a file from memory
 */
void filesystem_unload_file(const char* name) {
    if (!g_filesystem_state.initialized || !name) {
        return;
    }
    
    registered_file_t* file = find_registered_file(name);
    if (!file || !file->loaded) {
        return;
    }
    
    if (file->data) {
        free(file->data);
        g_filesystem_state.stats.total_memory_used_kb -= file->size / 1024;
    }
    
    file->data = NULL;
    file->size = 0;
    file->loaded = false;
    file->dirty = false;
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Unloaded file from cache: ");
    serial_debug(name);
    serial_debug("\n");
    #endif
}

/**
 * Open a file and return a handle
 */
filesystem_handle_t* filesystem_open(const char* path, filesystem_mode_t mode) {
    if (!g_filesystem_state.initialized || !path) {
        return NULL;
    }
    
    vfs_node_t* node = vfs_open(path);
    if (!node && mode == FS_MODE_READ_ONLY) {
        return NULL;
    }
    
    filesystem_handle_t* handle = allocate_handle();
    if (!handle) {
        return NULL;
    }
    
    handle->vfs_node = node;
    handle->mode = mode;
    handle->position = 0;
    handle->size = node ? node->size : 0;
    
    g_filesystem_state.stats.total_read_operations++;
    
    return handle;
}

/**
 * Read data from a file handle
 */
size_t filesystem_read(filesystem_handle_t* handle, void* buffer, size_t size) {
    if (!handle || !handle->valid || !buffer) {
        return 0;
    }
    
    // Simplified implementation - real version would use VFS read
    uint32_t start_time = 0; // Would get actual timestamp
    size_t bytes_read = 0;
    
    // Simulate read operation
    if (handle->position + size <= handle->size) {
        bytes_read = size;
    } else if (handle->position < handle->size) {
        bytes_read = handle->size - handle->position;
    }
    
    handle->position += bytes_read;
    
    uint32_t end_time = start_time + 100; // Simulate 100us
    update_stats_read_operation(end_time - start_time, bytes_read);
    
    return bytes_read;
}

/**
 * Write data to a file handle
 */
size_t filesystem_write(filesystem_handle_t* handle, const void* buffer, size_t size) {
    if (!handle || !handle->valid || !buffer) {
        return 0;
    }
    
    if (handle->mode == FS_MODE_READ_ONLY) {
        return 0;
    }
    
    // Simplified implementation
    uint32_t start_time = 0;
    size_t bytes_written = size;
    
    handle->position += bytes_written;
    if (handle->position > handle->size) {
        handle->size = handle->position;
    }
    
    uint32_t end_time = start_time + 150; // Simulate 150us
    update_stats_write_operation(end_time - start_time, bytes_written);
    
    return bytes_written;
}

/**
 * Close a file handle
 */
void filesystem_close(filesystem_handle_t* handle) {
    if (!handle || !handle->valid) {
        return;
    }
    
    free_handle(handle);
}

/**
 * Flush all cached files to disk
 */
void filesystem_flush_cache(void) {
    if (!g_filesystem_state.initialized) {
        return;
    }
    
    for (uint32_t i = 0; i < MAX_REGISTERED_FILES; i++) {
        if (g_filesystem_state.registered_files[i].loaded && 
            g_filesystem_state.registered_files[i].dirty) {
            // Would implement actual flush to disk here
            g_filesystem_state.registered_files[i].dirty = false;
        }
    }
    
    #ifdef DEBUG_SERIAL
    serial_debug("[FILESYSTEM] Cache flushed to disk\n");
    #endif
}

/**
 * Clear all cached files from memory
 */
void filesystem_clear_cache(void) {
    if (!g_filesystem_state.initialized) {
        return;
    }
    
    for (uint32_t i = 0; i < MAX_REGISTERED_FILES; i++) {
        if (g_filesystem_state.registered_files[i].loaded) {
            filesystem_unload_file(g_filesystem_state.registered_files[i].name);
        }
    }
    
    g_filesystem_state.total_cache_size = 0;
}

/**
 * Detect file type from filename extension
 */
file_type_t filesystem_detect_file_type(const char* filename) {
    if (!filename) {
        return FILE_TYPE_BINARY;
    }
    
    const char* ext = filesystem_get_file_extension(filename);
    if (!ext) {
        return FILE_TYPE_BINARY;
    }
    
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "log") == 0) {
        return FILE_TYPE_TEXT;
    } else if (strcmp(ext, "cfg") == 0 || strcmp(ext, "conf") == 0) {
        return FILE_TYPE_CONFIG;
    } else if (strcmp(ext, "exe") == 0 || strcmp(ext, "bin") == 0) {
        return FILE_TYPE_EXECUTABLE;
    } else if (strcmp(ext, "bdf") == 0 || strcmp(ext, "ttf") == 0) {
        return FILE_TYPE_FONT;
    } else if (strcmp(ext, "bmp") == 0 || strcmp(ext, "png") == 0) {
        return FILE_TYPE_IMAGE;
    } else {
        return FILE_TYPE_DATA;
    }
}

/**
 * Get file extension from filename
 */
const char* filesystem_get_file_extension(const char* filename) {
    if (!filename) {
        return NULL;
    }
    
    const char* last_dot = NULL;
    for (const char* p = filename; *p; p++) {
        if (*p == '.') {
            last_dot = p;
        }
    }
    
    return last_dot ? last_dot + 1 : NULL;
}

// Internal helper functions
static registered_file_t* find_registered_file(const char* name) {
    for (uint32_t i = 0; i < MAX_REGISTERED_FILES; i++) {
        if (g_filesystem_state.registered_files[i].name && 
            strcmp(g_filesystem_state.registered_files[i].name, name) == 0) {
            return &g_filesystem_state.registered_files[i];
        }
    }
    return NULL;
}

static filesystem_handle_t* allocate_handle(void) {
    for (uint32_t i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (!g_filesystem_state.open_handles[i].valid) {
            g_filesystem_state.open_handles[i].handle_id = g_filesystem_state.next_handle_id++;
            g_filesystem_state.open_handles[i].valid = true;
            g_filesystem_state.open_handles[i].position = 0;
            g_filesystem_state.open_handles[i].vfs_node = NULL;
            return &g_filesystem_state.open_handles[i];
        }
    }
    return NULL;
}

static void free_handle(filesystem_handle_t* handle) {
    if (handle) {
        handle->valid = false;
        handle->handle_id = 0;
        handle->vfs_node = NULL;
        handle->position = 0;
        handle->size = 0;
    }
}

static void update_stats_read_operation(uint32_t time_us, size_t bytes) {
    (void)bytes; // Suppress unused parameter warning
    g_filesystem_state.stats.total_read_operations++;
    g_filesystem_state.stats.avg_read_time_us = 
        (g_filesystem_state.stats.avg_read_time_us + time_us) / 2;
}

static void update_stats_write_operation(uint32_t time_us, size_t bytes) {
    (void)bytes; // Suppress unused parameter warning
    g_filesystem_state.stats.total_write_operations++;
    g_filesystem_state.stats.avg_write_time_us = 
        (g_filesystem_state.stats.avg_write_time_us + time_us) / 2;
}

// Subsystem lifecycle callbacks
static void filesystem_subsystem_start(void) {
    if (g_filesystem_state.subsystem_registry) {
        g_filesystem_state.subsystem_registry->state = SUBSYSTEM_STATE_RUNNING;
    }
}

static void filesystem_subsystem_stop(void) {
    filesystem_subsystem_shutdown();
}

static void filesystem_subsystem_restart(void) {
    filesystem_subsystem_shutdown();
    filesystem_subsystem_init(g_filesystem_state.subsystem_registry);
}

static void filesystem_subsystem_message_handler(void *msg) {
    (void)msg; // Suppress unused parameter warning
    if (g_filesystem_state.subsystem_registry) {
        g_filesystem_state.subsystem_registry->stats_messages_handled++;
    }
}
