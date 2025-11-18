/**
 * QARMA - Quantum AI Observer Implementation
 * 
 * AI system that learns optimal collapse strategies.
 */

#include "quantum/quantum_ai_observer.h"
#include "quantum/quantum_register.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "config.h"

// Global observer instance
static quantum_ai_observer_t g_observer = {
    .learning_db = NULL,
    .db_size = 0,
    .db_capacity = 0,
    .enabled = true,
    .total_observations = 0
};

#define INITIAL_DB_CAPACITY 32
#define PROFILE_MATCH_THRESHOLD 0.8f

// Helper: Calculate similarity between two workload profiles (0-1)
static float profile_similarity(quantum_workload_profile_t* a, quantum_workload_profile_t* b) {
    float similarity = 0.0f;
    float weight_sum = 0.0f;
    
    // Qubit count similarity (weight: 0.3)
    float qubit_diff = (float)((a->qubit_count > b->qubit_count) ? 
                               (a->qubit_count - b->qubit_count) : 
                               (b->qubit_count - a->qubit_count));
    float qubit_sim = (qubit_diff < 10) ? (1.0f - qubit_diff / 10.0f) : 0.0f;
    similarity += qubit_sim * 0.3f;
    weight_sum += 0.3f;
    
    // Execution time similarity (weight: 0.25)
    if (a->avg_execution_time > 0 && b->avg_execution_time > 0) {
        float time_ratio = (float)a->avg_execution_time / (float)b->avg_execution_time;
        if (time_ratio > 1.0f) time_ratio = 1.0f / time_ratio;
        similarity += time_ratio * 0.25f;
        weight_sum += 0.25f;
    }
    
    // Boolean flags (weight: 0.15 each)
    if (a->has_evaluation == b->has_evaluation) {
        similarity += 0.15f;
        weight_sum += 0.15f;
    }
    if (a->requires_all == b->requires_all) {
        similarity += 0.15f;
        weight_sum += 0.15f;
    }
    
    // Data size similarity (weight: 0.15)
    if (a->data_size > 0 && b->data_size > 0) {
        float size_ratio = (float)a->data_size / (float)b->data_size;
        if (size_ratio > 1.0f) size_ratio = 1.0f / size_ratio;
        similarity += size_ratio * 0.15f;
        weight_sum += 0.15f;
    }
    
    return (weight_sum > 0) ? (similarity / weight_sum) : 0.0f;
}

// Helper: Find or create learning entry for profile
static quantum_learning_entry_t* find_or_create_entry(quantum_workload_profile_t* profile) {
    // Find best matching entry
    quantum_learning_entry_t* best_match = NULL;
    float best_similarity = 0.0f;
    
    for (uint32_t i = 0; i < g_observer.db_size; i++) {
        float sim = profile_similarity(profile, &g_observer.learning_db[i].profile);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_match = &g_observer.learning_db[i];
        }
    }
    
    // If we found a good match, use it
    if (best_match && best_similarity >= PROFILE_MATCH_THRESHOLD) {
        return best_match;
    }
    
    // Need to create new entry
    if (g_observer.db_size >= g_observer.db_capacity) {
        // Expand database
        uint32_t new_capacity = (g_observer.db_capacity == 0) ? 
                                INITIAL_DB_CAPACITY : 
                                (g_observer.db_capacity * 2);
        
        quantum_learning_entry_t* new_db = (quantum_learning_entry_t*)heap_alloc(
            sizeof(quantum_learning_entry_t) * new_capacity);
        
        if (!new_db) {
            SERIAL_LOG("Warning: Failed to expand quantum AI database\n");
            return NULL;
        }
        
        // Copy existing entries
        if (g_observer.learning_db) {
            for (uint32_t i = 0; i < g_observer.db_size; i++) {
                new_db[i] = g_observer.learning_db[i];
            }
            heap_free(g_observer.learning_db);
        }
        
        g_observer.learning_db = new_db;
        g_observer.db_capacity = new_capacity;
    }
    
    // Initialize new entry
    quantum_learning_entry_t* entry = &g_observer.learning_db[g_observer.db_size++];
    entry->profile = *profile;
    entry->observation_count = 0;
    entry->confidence = 0.0f;
    
    // Initialize all strategy metrics
    for (int i = 0; i < COLLAPSE_STRATEGY_COUNT; i++) {
        entry->metrics[i].total_uses = 0;
        entry->metrics[i].success_count = 0;
        entry->metrics[i].total_time = 0;
        entry->metrics[i].avg_quality = 0.0f;
        entry->metrics[i].last_used = 0;
    }
    
    return entry;
}

// Initialize the quantum AI observer
void quantum_ai_init(void) {
    SERIAL_LOG("Quantum AI Observer: Initializing\n");
    
    g_observer.learning_db = NULL;
    g_observer.db_size = 0;
    g_observer.db_capacity = 0;
    g_observer.enabled = true;
    g_observer.total_observations = 0;
}

// Profile a quantum register to extract workload characteristics
quantum_workload_profile_t quantum_ai_profile_register(QARMA_QUANTUM_REGISTER* reg) {
    quantum_workload_profile_t profile = {0};
    
    if (!reg) return profile;
    
    profile.qubit_count = reg->count;
    profile.has_evaluation = (reg->evaluate != NULL);
    profile.requires_all = reg->wait_for_all;
    profile.data_size = 0; // TODO: Track this in register
    
    // Calculate average execution time and variance
    uint32_t total_time = 0;
    uint32_t completed = 0;
    
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->qubits[i].status == QUBIT_STATUS_COMPLETED) {
            uint32_t duration = reg->qubits[i].end_time - reg->qubits[i].start_time;
            total_time += duration;
            completed++;
        }
    }
    
    if (completed > 0) {
        profile.avg_execution_time = total_time / completed;
        
        // Calculate variance
        uint32_t variance_sum = 0;
        for (uint32_t i = 0; i < reg->count; i++) {
            if (reg->qubits[i].status == QUBIT_STATUS_COMPLETED) {
                uint32_t duration = reg->qubits[i].end_time - reg->qubits[i].start_time;
                int32_t diff = (int32_t)duration - (int32_t)profile.avg_execution_time;
                variance_sum += (diff * diff);
            }
        }
        profile.variance = variance_sum / completed;
    }
    
    return profile;
}

// Observe start of quantum register execution
void quantum_ai_observe_start(QARMA_QUANTUM_REGISTER* reg) {
    if (!g_observer.enabled || !reg) return;
    
    SERIAL_LOG("Quantum AI: Observing execution start\n");
}

// Observe completion of quantum register execution
void quantum_ai_observe_complete(QARMA_QUANTUM_REGISTER* reg, uint32_t elapsed_ms, float quality) {
    if (!g_observer.enabled || !reg) return;
    
    SERIAL_LOG("Quantum AI: Observing completion - strategy=");
    SERIAL_LOG_HEX("", reg->strategy);
    SERIAL_LOG(" time=");
    SERIAL_LOG_HEX("", elapsed_ms);
    SERIAL_LOG("ms\n");
    
    // Profile the workload
    quantum_workload_profile_t profile = quantum_ai_profile_register(reg);
    
    // Find or create learning entry
    quantum_learning_entry_t* entry = find_or_create_entry(&profile);
    if (!entry) return;
    
    // Update metrics for the strategy used
    if (reg->strategy < COLLAPSE_STRATEGY_COUNT) {
        strategy_metrics_t* metrics = &entry->metrics[reg->strategy];
        
        metrics->total_uses++;
        if (reg->collapsed) {
            metrics->success_count++;
        }
        metrics->total_time += elapsed_ms;
        
        // Update average quality (exponential moving average)
        float alpha = 0.3f; // Learning rate
        metrics->avg_quality = (metrics->avg_quality * (1.0f - alpha)) + (quality * alpha);
        
        metrics->last_used = g_observer.total_observations;
    }
    
    // Update entry statistics
    entry->observation_count++;
    entry->confidence = (entry->observation_count >= 10) ? 
                       (1.0f - 1.0f / entry->observation_count) : 
                       (entry->observation_count / 10.0f);
    
    g_observer.total_observations++;
    
    SERIAL_LOG("Quantum AI: Learning updated (observations=");
    SERIAL_LOG_HEX("", entry->observation_count);
    SERIAL_LOG(" confidence=");
    SERIAL_LOG_HEX("", (uint32_t)(entry->confidence * 100));
    SERIAL_LOG("%)\n");
}

// Get recommended collapse strategy for a workload
QARMA_COLLAPSE_STRATEGY quantum_ai_recommend_strategy(quantum_workload_profile_t* profile) {
    if (!g_observer.enabled || !profile) {
        return COLLAPSE_FIRST_WINS; // Default
    }
    
    // Find matching entry
    quantum_learning_entry_t* best_match = NULL;
    float best_similarity = 0.0f;
    
    for (uint32_t i = 0; i < g_observer.db_size; i++) {
        float sim = profile_similarity(profile, &g_observer.learning_db[i].profile);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_match = &g_observer.learning_db[i];
        }
    }
    
    // If no good match or not confident, use heuristics
    if (!best_match || best_similarity < PROFILE_MATCH_THRESHOLD || best_match->confidence < 0.5f) {
        // Heuristic recommendations based on profile
        if (profile->has_evaluation) {
            return COLLAPSE_BEST;
        } else if (profile->requires_all) {
            return COLLAPSE_COMBINE;
        } else if (profile->variance < 100) {
            return COLLAPSE_FIRST_WINS; // Low variance, any result is good
        } else {
            return COLLAPSE_VALIDATE; // High variance, validate
        }
    }
    
    // Find best performing strategy from learned data
    QARMA_COLLAPSE_STRATEGY best_strategy = COLLAPSE_FIRST_WINS;
    float best_score = 0.0f;
    
    for (int i = 0; i < COLLAPSE_STRATEGY_COUNT; i++) {
        strategy_metrics_t* metrics = &best_match->metrics[i];
        
        if (metrics->total_uses == 0) continue;
        
        // Score = quality * success_rate / avg_time
        float success_rate = (float)metrics->success_count / (float)metrics->total_uses;
        float avg_time = (float)metrics->total_time / (float)metrics->total_uses;
        float score = (metrics->avg_quality * success_rate) / (avg_time + 1.0f);
        
        if (score > best_score) {
            best_score = score;
            best_strategy = (QARMA_COLLAPSE_STRATEGY)i;
        }
    }
    
    SERIAL_LOG("Quantum AI: Recommending strategy ");
    SERIAL_LOG_HEX("", best_strategy);
    SERIAL_LOG(" (confidence=");
    SERIAL_LOG_HEX("", (uint32_t)(best_match->confidence * 100));
    SERIAL_LOG("%)\n");
    
    return best_strategy;
}

// Get confidence in recommendation
float quantum_ai_get_confidence(quantum_workload_profile_t* profile, 
                                 QARMA_COLLAPSE_STRATEGY strategy) {
    if (!g_observer.enabled || !profile) return 0.0f;
    
    // Find matching entry
    for (uint32_t i = 0; i < g_observer.db_size; i++) {
        float sim = profile_similarity(profile, &g_observer.learning_db[i].profile);
        if (sim >= PROFILE_MATCH_THRESHOLD) {
            quantum_learning_entry_t* entry = &g_observer.learning_db[i];
            
            if (strategy < COLLAPSE_STRATEGY_COUNT && 
                entry->metrics[strategy].total_uses > 0) {
                return entry->confidence;
            }
        }
    }
    
    return 0.0f;
}

// Print AI statistics
void quantum_ai_print_stats(void) {
    GFX_LOG("\n=== Quantum AI Observer Statistics ===\n");
    GFX_LOG("Total observations: ");
    GFX_LOG_HEX("", g_observer.total_observations);
    GFX_LOG("\nLearning database entries: ");
    GFX_LOG_HEX("", g_observer.db_size);
    GFX_LOG("\nEnabled: ");
    GFX_LOG(g_observer.enabled ? "Yes" : "No");
    GFX_LOG("\n");
    
    SERIAL_LOG("\nQuantum AI Statistics:\n");
    SERIAL_LOG("  Observations: ");
    SERIAL_LOG_HEX("", g_observer.total_observations);
    SERIAL_LOG("\n  DB entries: ");
    SERIAL_LOG_HEX("", g_observer.db_size);
    SERIAL_LOG("\n");
}

// Reset learning
void quantum_ai_reset_learning(void) {
    SERIAL_LOG("Quantum AI: Resetting learned data\n");
    
    if (g_observer.learning_db) {
        heap_free(g_observer.learning_db);
    }
    
    g_observer.learning_db = NULL;
    g_observer.db_size = 0;
    g_observer.db_capacity = 0;
    g_observer.total_observations = 0;
}

// Enable/disable learning
void quantum_ai_set_enabled(bool enabled) {
    g_observer.enabled = enabled;
    SERIAL_LOG("Quantum AI: ");
    SERIAL_LOG(enabled ? "Enabled" : "Disabled");
    SERIAL_LOG("\n");
}
