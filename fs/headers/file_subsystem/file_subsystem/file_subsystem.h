#pragma once
#include "core/stdtools.h"
#include "core/scheduler/subsystem_registry.h"
#include "../vfs.h"
#include "core/blockdev.h"

// File types for subsystem registration
typedef enum {
    FILE_TYPE_TEXT,
    FILE_TYPE_BINARY,
    FILE_TYPE_EXECUTABLE,
    FILE_TYPE_CONFIG,
    FILE_TYPE_FONT,
    FILE_TYPE_IMAGE,
    FILE_TYPE_DATA
} file_type_t;

// Filesystem subsystem statistics
typedef struct {
    uint32_t total_files_registered;
    uint32_t total_files_loaded;
    uint32_t total_memory_used_kb;
    uint32_t total_read_operations;
    uint32_t total_write_operations;
    uint32_t total_cache_hits;
    uint32_t total_cache_misses;
    uint32_t avg_read_time_us;
    uint32_t avg_write_time_us;
} filesystem_subsystem_stats_t;

// Filesystem subsystem configuration
typedef struct {
    uint32_t max_cached_files;
    uint32_t max_cache_size_mb;
    uint32_t read_buffer_size_kb;
    uint32_t write_buffer_size_kb;
    bool enable_compression;
    bool enable_encryption;
    bool enable_write_through;
} filesystem_subsystem_config_t;

// Filesystem operation modes
typedef enum {
    FS_MODE_READ_ONLY,
    FS_MODE_READ_WRITE,
    FS_MODE_APPEND_ONLY,
    FS_MODE_CREATE_NEW
} filesystem_mode_t;

// File cache entry
typedef struct {
    const char* name;
    const char* path;
    file_type_t type;
    void* data;
    size_t size;
    bool loaded;
    bool dirty;
    uint32_t access_count;
    uint32_t last_access_time;
} registered_file_t;

// Virtual file handle for subsystem operations
typedef struct {
    uint32_t handle_id;
    vfs_node_t* vfs_node;
    filesystem_mode_t mode;
    size_t position;
    size_t size;
    bool valid;
} filesystem_handle_t;

#define FILESYSTEM_SUBSYSTEM_ID 0x03

/* Filesystem Subsystem Core Functions */
void filesystem_subsystem_init(subsystem_t* registry);
void filesystem_subsystem_shutdown(void);
void filesystem_subsystem_get_stats(filesystem_subsystem_stats_t* stats);
void filesystem_subsystem_configure(const filesystem_subsystem_config_t* config);

/* File Registration and Management */
bool filesystem_register_file(const char* name, const char* path, file_type_t type);
registered_file_t* filesystem_lookup_file(const char* name);
bool filesystem_load_file(const char* name);
void* filesystem_get_file_data(const char* name, size_t* size);
bool filesystem_set_file_data(const char* name, void* data, size_t size); // Manual data assignment
void filesystem_unload_file(const char* name);
void filesystem_unregister_file(const char* name);

/* VFS Integration Functions */
filesystem_handle_t* filesystem_open(const char* path, filesystem_mode_t mode);
size_t filesystem_read(filesystem_handle_t* handle, void* buffer, size_t size);
size_t filesystem_write(filesystem_handle_t* handle, const void* buffer, size_t size);
bool filesystem_seek(filesystem_handle_t* handle, size_t position);
void filesystem_close(filesystem_handle_t* handle);

/* Directory Operations */
bool filesystem_create_directory(const char* path);
bool filesystem_remove_directory(const char* path);
vfs_node_t* filesystem_list_directory(const char* path);

/* File Operations */
bool filesystem_create_file(const char* path, file_type_t type);
bool filesystem_delete_file(const char* path);
bool filesystem_copy_file(const char* src_path, const char* dest_path);
bool filesystem_move_file(const char* src_path, const char* dest_path);
bool filesystem_file_exists(const char* path);
size_t filesystem_get_file_size(const char* path);

/* Mount Point Management */
bool filesystem_mount(const char* device, const char* filesystem_type, const char* mount_point);
bool filesystem_unmount(const char* mount_point);
vfs_node_t* filesystem_get_mount_points(void);

/* Cache Management */
void filesystem_flush_cache(void);
void filesystem_clear_cache(void);
bool filesystem_cache_file(const char* path);
void filesystem_uncache_file(const char* path);

/* Utility Functions */
const char* filesystem_get_file_extension(const char* filename);
file_type_t filesystem_detect_file_type(const char* filename);
bool filesystem_is_valid_filename(const char* filename);
char* filesystem_resolve_path(const char* path);

/* Legacy compatibility functions for existing code */
#define file_subsystem_init() filesystem_subsystem_init(NULL)
#define file_register(name, path, type) filesystem_register_file(name, path, type)
#define file_lookup(name) filesystem_lookup_file(name)
#define file_load(name) filesystem_load_file(name)
#define file_get_data(name, size) filesystem_get_file_data(name, size)
#define file_unload(name) filesystem_unload_file(name)