/**
 * @file command_predictor.h
 * @brief AI-powered command prediction and caching system
 * 
 * Implements Phase 1 of the QARMA Vision:
 * - Command result caching
 * - Pattern recognition for repeated commands
 * - Fast result injection
 */

#ifndef COMMAND_PREDICTOR_H
#define COMMAND_PREDICTOR_H

#include "kernel_types.h"

// Maximum number of cached commands
#define MAX_CACHED_COMMANDS 256

// Maximum command length
#define MAX_COMMAND_LENGTH 256

// Maximum result size (in bytes)
#define MAX_RESULT_SIZE 4096

/**
 * Command cache entry structure
 */
typedef struct {
    char command[MAX_COMMAND_LENGTH];      // The command string
    char result[MAX_RESULT_SIZE];          // The cached result
    uint32_t hash;                         // Command hash for fast lookup
    uint32_t hit_count;                    // Number of times this was used
    uint32_t timestamp;                    // Last access time
    bool valid;                            // Is this entry valid?
} command_cache_entry_t;

/**
 * Command predictor statistics
 */
typedef struct {
    uint32_t total_predictions;            // Total prediction attempts
    uint32_t cache_hits;                   // Successful cache hits
    uint32_t cache_misses;                 // Cache misses
    uint32_t cache_size;                   // Current cache size
    float hit_rate;                        // Cache hit rate (%)
} predictor_stats_t;

/**
 * Initialize the command predictor
 * @return true on success
 */
bool command_predictor_init(void);

/**
 * Check if a command result is cached
 * @param command The command to check
 * @param result Buffer to store the result (if found)
 * @param result_size Size of the result buffer
 * @return true if cached result found
 */
bool command_check_cache(const char* command, char* result, uint32_t result_size);

/**
 * Cache a command and its result
 * @param command The command executed
 * @param result The result to cache
 * @return true on success
 */
bool command_cache_result(const char* command, const char* result);

/**
 * Get predictor statistics
 * @param stats Pointer to stats structure to fill
 */
void command_predictor_get_stats(predictor_stats_t* stats);

/**
 * Clear the command cache
 */
void command_cache_clear(void);

/**
 * Print cache statistics (for debugging)
 */
void command_cache_print_stats(void);

/**
 * Compute hash for a command string
 * @param command The command to hash
 * @return 32-bit hash value
 */
uint32_t command_hash(const char* command);

#endif // COMMAND_PREDICTOR_H
