/**
 * QARMA - Handle Manager Implementation
 * 
 * Provides universal handle allocation with type safety and generation tracking.
 */

#include "handle_manager.h"
#include "memory.h"
#include "string.h"
#include "config.h"

// ============================================================================
// Handle Table Storage
// ============================================================================

static struct {
    handle_entry_t entries[HANDLE_TABLE_SIZE];
    uint32_t next_free;                     // Next free slot hint
    uint32_t generation_counter;            // Global generation counter
    handle_stats_t stats;
    bool initialized;
} g_handle_manager = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * Find a free slot in the handle table.
 * @return Index of free slot, or UINT32_MAX if table full
 */
static uint32_t find_free_slot(void) {
    // Start from hint and wrap around
    uint32_t start = g_handle_manager.next_free;
    uint32_t index = start;
    
    do {
        if (!g_handle_manager.entries[index].in_use) {
            g_handle_manager.next_free = (index + 1) % HANDLE_TABLE_SIZE;
            return index;
        }
        index = (index + 1) % HANDLE_TABLE_SIZE;
    } while (index != start);
    
    return UINT32_MAX;  // Table full
}

/**
 * Get handle entry from handle value.
 * @return Pointer to entry, or NULL if invalid
 */
static handle_entry_t* get_entry(qarma_handle_t handle) {
    if (handle == QARMA_INVALID_HANDLE) {
        return NULL;
    }
    
    uint32_t index = handle_extract_index(handle);
    if (index >= HANDLE_TABLE_SIZE) {
        return NULL;
    }
    
    handle_entry_t* entry = &g_handle_manager.entries[index];
    
    // Verify entry is in use and generation matches
    if (!entry->in_use || entry->handle != handle) {
        return NULL;
    }
    
    return entry;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void handle_manager_init(void) {
    if (g_handle_manager.initialized) {
        SERIAL_LOG("[HANDLE_MGR] Already initialized\n");
        return;
    }
    
    // Clear all entries
    memset(g_handle_manager.entries, 0, sizeof(g_handle_manager.entries));
    
    g_handle_manager.next_free = 0;
    g_handle_manager.generation_counter = 1;  // Start at 1 (0 reserved for invalid)
    memset(&g_handle_manager.stats, 0, sizeof(handle_stats_t));
    g_handle_manager.initialized = true;
    
    SERIAL_LOG("[HANDLE_MGR] Initialized. Table size: ");
    SERIAL_LOG_DEC("", HANDLE_TABLE_SIZE);
    SERIAL_LOG(" entries\n");
}

void handle_manager_shutdown(void) {
    if (!g_handle_manager.initialized) {
        return;
    }
    
    // Log warning for any still-active handles
    uint32_t leaked = 0;
    for (uint32_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        if (g_handle_manager.entries[i].in_use) {
            leaked++;
            SERIAL_LOG("[HANDLE_MGR] WARNING: Handle still active at shutdown: ");
            SERIAL_LOG(g_handle_manager.entries[i].debug_name);
            SERIAL_LOG("\n");
        }
    }
    
    if (leaked > 0) {
        SERIAL_LOG("[HANDLE_MGR] WARNING: ");
        SERIAL_LOG_DEC("", leaked);
        SERIAL_LOG(" handles leaked\n");
    }
    
    g_handle_manager.initialized = false;
    SERIAL_LOG("[HANDLE_MGR] Shutdown complete\n");
}

qarma_handle_t handle_allocate(handle_type_t type, void* object, const char* debug_name) {
    if (!g_handle_manager.initialized) {
        SERIAL_LOG("[HANDLE_MGR] ERROR: Not initialized\n");
        return QARMA_INVALID_HANDLE;
    }
    
    if (type == HANDLE_TYPE_INVALID || object == NULL) {
        SERIAL_LOG("[HANDLE_MGR] ERROR: Invalid type or NULL object\n");
        return QARMA_INVALID_HANDLE;
    }
    
    // Find free slot
    uint32_t index = find_free_slot();
    if (index == UINT32_MAX) {
        SERIAL_LOG("[HANDLE_MGR] ERROR: Handle table full\n");
        return QARMA_INVALID_HANDLE;
    }
    
    // Increment generation counter
    uint32_t generation = g_handle_manager.generation_counter++;
    if (generation > 0xFFFFFF) {  // 24-bit max
        generation = 1;
        g_handle_manager.generation_counter = 2;
    }
    
    // Create handle
    qarma_handle_t handle = handle_create(type, generation, index);
    
    // Initialize entry
    handle_entry_t* entry = &g_handle_manager.entries[index];
    entry->handle = handle;
    entry->type = type;
    entry->object = object;
    entry->generation = generation;
    entry->ref_count = 1;
    entry->in_use = true;
    
    if (debug_name) {
        strncpy(entry->debug_name, debug_name, sizeof(entry->debug_name) - 1);
        entry->debug_name[sizeof(entry->debug_name) - 1] = '\0';
    } else {
        entry->debug_name[0] = '\0';
    }
    
    // Update statistics
    g_handle_manager.stats.total_allocated++;
    g_handle_manager.stats.current_active++;
    g_handle_manager.stats.active_by_type[type]++;
    
    return handle;
}

bool handle_release(qarma_handle_t handle) {
    if (!g_handle_manager.initialized) {
        return false;
    }
    
    handle_entry_t* entry = get_entry(handle);
    if (!entry) {
        g_handle_manager.stats.validation_failures++;
        return false;
    }
    
    // Clear entry
    handle_type_t type = entry->type;
    entry->in_use = false;
    entry->handle = QARMA_INVALID_HANDLE;
    entry->object = NULL;
    entry->ref_count = 0;
    
    // Update statistics
    g_handle_manager.stats.total_freed++;
    g_handle_manager.stats.current_active--;
    if (g_handle_manager.stats.active_by_type[type] > 0) {
        g_handle_manager.stats.active_by_type[type]--;
    }
    g_handle_manager.stats.recycled_count++;
    
    return true;
}

void* handle_get_object(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    return entry ? entry->object : NULL;
}

bool handle_validate(qarma_handle_t handle, handle_type_t expected_type) {
    if (!g_handle_manager.initialized) {
        return false;
    }
    
    handle_entry_t* entry = get_entry(handle);
    if (!entry) {
        g_handle_manager.stats.validation_failures++;
        return false;
    }
    
    // If type check requested, verify type matches
    if (expected_type != HANDLE_TYPE_INVALID && entry->type != expected_type) {
        g_handle_manager.stats.validation_failures++;
        return false;
    }
    
    return true;
}

handle_type_t handle_get_type(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    return entry ? entry->type : HANDLE_TYPE_INVALID;
}

uint32_t handle_add_ref(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    if (!entry) {
        return 0;
    }
    
    entry->ref_count++;
    return entry->ref_count;
}

uint32_t handle_release_ref(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    if (!entry) {
        return 0;
    }
    
    if (entry->ref_count > 0) {
        entry->ref_count--;
    }
    
    // Auto-release if ref count reaches 0
    if (entry->ref_count == 0) {
        handle_release(handle);
        return 0;
    }
    
    return entry->ref_count;
}

uint32_t handle_get_ref_count(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    return entry ? entry->ref_count : 0;
}

bool handle_set_debug_name(qarma_handle_t handle, const char* name) {
    handle_entry_t* entry = get_entry(handle);
    if (!entry || !name) {
        return false;
    }
    
    strncpy(entry->debug_name, name, sizeof(entry->debug_name) - 1);
    entry->debug_name[sizeof(entry->debug_name) - 1] = '\0';
    return true;
}

const char* handle_get_debug_name(qarma_handle_t handle) {
    handle_entry_t* entry = get_entry(handle);
    return entry ? entry->debug_name : NULL;
}

void handle_get_stats(handle_stats_t* stats) {
    if (!stats || !g_handle_manager.initialized) {
        return;
    }
    
    memcpy(stats, &g_handle_manager.stats, sizeof(handle_stats_t));
}

void handle_dump_all(void) {
    if (!g_handle_manager.initialized) {
        SERIAL_LOG("[HANDLE_MGR] Not initialized\n");
        return;
    }
    
    SERIAL_LOG("[HANDLE_MGR] Active handles (");
    SERIAL_LOG_DEC("", g_handle_manager.stats.current_active);
    SERIAL_LOG("):\n");
    
    for (uint32_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        handle_entry_t* entry = &g_handle_manager.entries[i];
        if (entry->in_use) {
            SERIAL_LOG("  [");
            SERIAL_LOG_DEC("", i);
            SERIAL_LOG("] Type: ");
            SERIAL_LOG_HEX("", entry->type);
            SERIAL_LOG(" RefCount: ");
            SERIAL_LOG_DEC("", entry->ref_count);
            SERIAL_LOG(" Name: ");
            SERIAL_LOG(entry->debug_name[0] ? entry->debug_name : "(unnamed)");
            SERIAL_LOG("\n");
        }
    }
}

void handle_dump_by_type(handle_type_t type) {
    if (!g_handle_manager.initialized) {
        SERIAL_LOG("[HANDLE_MGR] Not initialized\n");
        return;
    }
    
    SERIAL_LOG("[HANDLE_MGR] Handles of type ");
    SERIAL_LOG_HEX("", type);
    SERIAL_LOG(" (");
    SERIAL_LOG_DEC("", g_handle_manager.stats.active_by_type[type]);
    SERIAL_LOG("):\n");
    
    for (uint32_t i = 0; i < HANDLE_TABLE_SIZE; i++) {
        handle_entry_t* entry = &g_handle_manager.entries[i];
        if (entry->in_use && entry->type == type) {
            SERIAL_LOG("  [");
            SERIAL_LOG_DEC("", i);
            SERIAL_LOG("] RefCount: ");
            SERIAL_LOG_DEC("", entry->ref_count);
            SERIAL_LOG(" Name: ");
            SERIAL_LOG(entry->debug_name[0] ? entry->debug_name : "(unnamed)");
            SERIAL_LOG("\n");
        }
    }
}
