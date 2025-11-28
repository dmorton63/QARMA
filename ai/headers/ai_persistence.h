/**
 * @file ai_persistence.h
 * @brief AI Learning Data Persistence Layer
 * 
 * Saves and loads AI learning data to/from FAT16 disk for persistence
 * across reboots.
 */

#ifndef AI_PERSISTENCE_H
#define AI_PERSISTENCE_H

#include "kernel_types.h"

// Magic number for AI persistence files: "QAIA" (QARMA AI)
#define AI_PERSISTENCE_MAGIC 0x51414941

// Data type identifiers
#define AI_DATA_QUANTUM_LEARNING  1
#define AI_DATA_COMMAND_CACHE     2
#define AI_DATA_NEURAL_WEIGHTS    3
#define AI_DATA_CROSS_LEARNING    4

// Persistence file header
typedef struct {
    uint32_t magic;           // Magic number (0x51414941)
    uint32_t version;         // Format version
    uint32_t data_type;       // Type of data
    uint32_t data_size;       // Size of following data
    uint32_t checksum;        // CRC32 of data
} __attribute__((packed)) ai_persistence_header_t;

// Initialize persistence system
void ai_persistence_init(void);

// Save all AI state to disk
int ai_save_state(void);

// Load AI state from disk
int ai_load_state(void);

// Save individual components
int ai_save_quantum_learning(const char* path);
int ai_load_quantum_learning(const char* path);
int ai_save_command_cache(const char* path);
int ai_load_command_cache(const char* path);

// Clear all saved data
int ai_clear_saved_state(void);

// Get statistics
void ai_persistence_print_stats(void);

#endif // AI_PERSISTENCE_H
