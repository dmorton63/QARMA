/**
 * @file command_predictor.c
 * @brief AI-powered command prediction and caching implementation
 */

#include "command_predictor.h"
#include "core/string.h"  // For string operations
#include "core/timer.h"   // For timestamps

// Command cache storage
static command_cache_entry_t command_cache[MAX_CACHED_COMMANDS];
static predictor_stats_t stats;
static bool initialized = false;

/**
 * Simple hash function (djb2 algorithm)
 */
uint32_t command_hash(const char* command) {
    uint32_t hash = 5381;
    int c;
    
    while ((c = *command++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

/**
 * Find a cache entry by hash
 */
static command_cache_entry_t* find_cache_entry(uint32_t hash) {
    for (int i = 0; i < MAX_CACHED_COMMANDS; i++) {
        if (command_cache[i].valid && command_cache[i].hash == hash) {
            return &command_cache[i];
        }
    }
    return NULL;
}

/**
 * Find an empty cache slot or evict least recently used
 */
static command_cache_entry_t* get_cache_slot(void) {
    // First, try to find an empty slot
    for (int i = 0; i < MAX_CACHED_COMMANDS; i++) {
        if (!command_cache[i].valid) {
            return &command_cache[i];
        }
    }
    
    // No empty slots, find LRU entry
    command_cache_entry_t* lru = &command_cache[0];
    for (int i = 1; i < MAX_CACHED_COMMANDS; i++) {
        if (command_cache[i].timestamp < lru->timestamp) {
            lru = &command_cache[i];
        }
    }
    
    return lru;
}

bool command_predictor_init(void) {
    if (initialized) {
        return true;
    }
    
    // Clear cache
    for (int i = 0; i < MAX_CACHED_COMMANDS; i++) {
        command_cache[i].valid = false;
        command_cache[i].hit_count = 0;
        command_cache[i].timestamp = 0;
        command_cache[i].hash = 0;
    }
    
    // Clear stats
    stats.total_predictions = 0;
    stats.cache_hits = 0;
    stats.cache_misses = 0;
    stats.cache_size = 0;
    stats.hit_rate = 0.0f;
    
    initialized = true;
    
    extern void gfx_print(const char*);
    gfx_print("[AI] Command predictor initialized\n");
    
    return true;
}

bool command_check_cache(const char* command, char* result, uint32_t result_size) {
    if (!initialized || !command || !result) {
        return false;
    }
    
    stats.total_predictions++;
    
    uint32_t hash = command_hash(command);
    command_cache_entry_t* entry = find_cache_entry(hash);
    
    if (entry) {
        // Cache hit!
        stats.cache_hits++;
        entry->hit_count++;
        entry->timestamp = system_ticks;
        
        // Copy result to output buffer
        int len = 0;
        while (entry->result[len] && len < result_size - 1) {
            result[len] = entry->result[len];
            len++;
        }
        result[len] = '\0';
        
        // Update hit rate
        stats.hit_rate = (float)stats.cache_hits / (float)stats.total_predictions * 100.0f;
        
        return true;
    }
    
    // Cache miss
    stats.cache_misses++;
    stats.hit_rate = (float)stats.cache_hits / (float)stats.total_predictions * 100.0f;
    
    return false;
}

bool command_cache_result(const char* command, const char* result) {
    if (!initialized || !command || !result) {
        return false;
    }
    
    uint32_t hash = command_hash(command);
    
    // Check if already cached
    command_cache_entry_t* entry = find_cache_entry(hash);
    
    if (!entry) {
        // Get a new slot
        entry = get_cache_slot();
        
        // Copy command
        int i = 0;
        while (command[i] && i < MAX_COMMAND_LENGTH - 1) {
            entry->command[i] = command[i];
            i++;
        }
        entry->command[i] = '\0';
        
        entry->hash = hash;
        entry->hit_count = 0;
        entry->valid = true;
        stats.cache_size++;
    }
    
    // Copy result
    int i = 0;
    while (result[i] && i < MAX_RESULT_SIZE - 1) {
        entry->result[i] = result[i];
        i++;
    }
    entry->result[i] = '\0';
    
    entry->timestamp = system_ticks;
    
    return true;
}

void command_predictor_get_stats(predictor_stats_t* stats_out) {
    if (stats_out) {
        stats_out->total_predictions = stats.total_predictions;
        stats_out->cache_hits = stats.cache_hits;
        stats_out->cache_misses = stats.cache_misses;
        stats_out->cache_size = stats.cache_size;
        stats_out->hit_rate = stats.hit_rate;
    }
}

void command_cache_clear(void) {
    for (int i = 0; i < MAX_CACHED_COMMANDS; i++) {
        command_cache[i].valid = false;
        command_cache[i].hit_count = 0;
    }
    stats.cache_size = 0;
}

void command_cache_print_stats(void) {
    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    
    gfx_print("\n=== Command Predictor Statistics ===\n");
    gfx_print("Total predictions: ");
    gfx_print_hex(stats.total_predictions);
    gfx_print("\nCache hits: ");
    gfx_print_hex(stats.cache_hits);
    gfx_print("\nCache misses: ");
    gfx_print_hex(stats.cache_misses);
    gfx_print("\nCache size: ");
    gfx_print_hex(stats.cache_size);
    gfx_print("\nHit rate: ");
    gfx_print_hex((uint32_t)stats.hit_rate);
    gfx_print("%\n");
    gfx_print("====================================\n\n");
}
