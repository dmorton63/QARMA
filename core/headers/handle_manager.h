/**
 * QARMA - Handle Manager
 * 
 * Universal handle allocation and management system.
 * Every entity in QARMA (window, control, dialog, task, message) gets a unique handle.
 * Handles are 64-bit values with embedded type information.
 */

#ifndef HANDLE_MANAGER_H
#define HANDLE_MANAGER_H

// #include <stdint.h>
// #include <stdbool.h>
#include "stdtools.h"

// ============================================================================
// Handle Type Definitions
// ============================================================================

/**
 * Handle Format (64-bit):
 * Bits 63-56: Type identifier (8 bits)
 * Bits 55-32: Generation counter (24 bits) - for handle recycling safety
 * Bits 31-0:  Index into type-specific table (32 bits)
 */
typedef uint64_t qarma_handle_t;

#define QARMA_INVALID_HANDLE ((qarma_handle_t)0)

// Handle type identifiers (top 8 bits)
typedef enum {
    HANDLE_TYPE_INVALID     = 0x00,
    HANDLE_TYPE_WINDOW      = 0x01,
    HANDLE_TYPE_CONTROL     = 0x02,
    HANDLE_TYPE_DIALOG      = 0x03,
    HANDLE_TYPE_FRAME       = 0x04,
    HANDLE_TYPE_TASK        = 0x05,
    HANDLE_TYPE_MESSAGE     = 0x06,
    HANDLE_TYPE_SUBSYSTEM   = 0x07,
    HANDLE_TYPE_RESOURCE    = 0x08,
    HANDLE_TYPE_TIMER       = 0x09,
    HANDLE_TYPE_MENU        = 0x0A,
    HANDLE_TYPE_FONT        = 0x0B,
    HANDLE_TYPE_BITMAP      = 0x0C,
    HANDLE_TYPE_DEVICE      = 0x0D,
    HANDLE_TYPE_FILE        = 0x0E,
    HANDLE_TYPE_CUSTOM      = 0xFF
} handle_type_t;

// Handle table configuration
#define HANDLE_TABLE_SIZE 4096      // Max handles per type
#define HANDLE_GENERATION_MASK 0x00FFFFFF000000ULL
#define HANDLE_INDEX_MASK 0x0000000FFFFFFFFULL
#define HANDLE_TYPE_SHIFT 56

// ============================================================================
// Handle Entry Structure
// ============================================================================

typedef struct {
    qarma_handle_t handle;          // The actual handle value
    handle_type_t type;             // Handle type
    void* object;                   // Pointer to the object
    uint32_t generation;            // Generation counter
    uint32_t ref_count;             // Reference count
    bool in_use;                    // Is this slot in use?
    char debug_name[32];            // For debugging
} handle_entry_t;

// ============================================================================
// Handle Manager Statistics
// ============================================================================

typedef struct {
    uint32_t total_allocated;       // Total handles ever allocated
    uint32_t total_freed;           // Total handles freed
    uint32_t current_active;        // Currently active handles
    uint32_t active_by_type[256];   // Active handles per type
    uint32_t recycled_count;        // Number of handle recyclings
    uint32_t validation_failures;   // Failed validations
} handle_stats_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * Initialize the handle manager system.
 * Must be called before any other handle operations.
 */
void handle_manager_init(void);

/**
 * Shutdown the handle manager and cleanup resources.
 */
void handle_manager_shutdown(void);

/**
 * Allocate a new handle for an object.
 * 
 * @param type Handle type
 * @param object Pointer to the object being handled
 * @param debug_name Optional debug name (can be NULL)
 * @return New handle, or QARMA_INVALID_HANDLE on failure
 */
qarma_handle_t handle_allocate(handle_type_t type, void* object, const char* debug_name);

/**
 * Release a handle and mark it for recycling.
 * 
 * @param handle Handle to release
 * @return true if successful, false if handle invalid
 */
bool handle_release(qarma_handle_t handle);

/**
 * Get the object pointer associated with a handle.
 * 
 * @param handle Handle to look up
 * @return Object pointer, or NULL if handle invalid
 */
void* handle_get_object(qarma_handle_t handle);

/**
 * Validate a handle and optionally check its type.
 * 
 * @param handle Handle to validate
 * @param expected_type Expected type (HANDLE_TYPE_INVALID to skip type check)
 * @return true if handle is valid (and type matches if specified)
 */
bool handle_validate(qarma_handle_t handle, handle_type_t expected_type);

/**
 * Get the type of a handle.
 * 
 * @param handle Handle to query
 * @return Handle type, or HANDLE_TYPE_INVALID if handle invalid
 */
handle_type_t handle_get_type(qarma_handle_t handle);

/**
 * Increment reference count for a handle.
 * Useful for shared ownership scenarios.
 * 
 * @param handle Handle to increment
 * @return New reference count, or 0 if handle invalid
 */
uint32_t handle_add_ref(qarma_handle_t handle);

/**
 * Decrement reference count for a handle.
 * Handle is automatically released when ref count reaches 0.
 * 
 * @param handle Handle to decrement
 * @return New reference count, or 0 if handle invalid
 */
uint32_t handle_release_ref(qarma_handle_t handle);

/**
 * Get current reference count for a handle.
 * 
 * @param handle Handle to query
 * @return Reference count, or 0 if handle invalid
 */
uint32_t handle_get_ref_count(qarma_handle_t handle);

/**
 * Set debug name for a handle (useful for debugging).
 * 
 * @param handle Handle to name
 * @param name Debug name string
 * @return true if successful
 */
bool handle_set_debug_name(qarma_handle_t handle, const char* name);

/**
 * Get debug name for a handle.
 * 
 * @param handle Handle to query
 * @return Debug name string, or NULL if handle invalid
 */
const char* handle_get_debug_name(qarma_handle_t handle);

/**
 * Get handle manager statistics.
 * 
 * @param stats Output structure for statistics
 */
void handle_get_stats(handle_stats_t* stats);

/**
 * Print all active handles (debugging).
 */
void handle_dump_all(void);

/**
 * Print handles of a specific type (debugging).
 * 
 * @param type Type to filter by
 */
void handle_dump_by_type(handle_type_t type);

// ============================================================================
// Inline Helper Functions
// ============================================================================

/**
 * Extract type from a handle value.
 */
static inline handle_type_t handle_extract_type(qarma_handle_t handle) {
    return (handle_type_t)((handle >> HANDLE_TYPE_SHIFT) & 0xFF);
}

/**
 * Extract index from a handle value.
 */
static inline uint32_t handle_extract_index(qarma_handle_t handle) {
    return (uint32_t)(handle & HANDLE_INDEX_MASK);
}

/**
 * Extract generation from a handle value.
 */
static inline uint32_t handle_extract_generation(qarma_handle_t handle) {
    return (uint32_t)((handle & HANDLE_GENERATION_MASK) >> 32);
}

/**
 * Check if a handle is valid (non-zero and type is valid).
 */
static inline bool handle_is_valid(qarma_handle_t handle) {
    return handle != QARMA_INVALID_HANDLE && 
           handle_extract_type(handle) != HANDLE_TYPE_INVALID;
}

/**
 * Create a handle value from components (internal use).
 */
static inline qarma_handle_t handle_create(handle_type_t type, uint32_t generation, uint32_t index) {
    return ((uint64_t)type << HANDLE_TYPE_SHIFT) |
           ((uint64_t)generation << 32) |
           (uint64_t)index;
}

#endif // HANDLE_MANAGER_H
