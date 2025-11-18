/**
 * QARMA - Quantum AI Observer
 * 
 * AI system that observes quantum register executions and learns
 * optimal collapse strategies for different workload patterns.
 */

#ifndef QUANTUM_AI_OBSERVER_H
#define QUANTUM_AI_OBSERVER_H

#include "kernel_types.h"
#include "quantum/quantum_register.h"

// Workload characteristics that affect strategy choice
typedef struct {
    uint32_t qubit_count;           // Number of qubits in register
    uint32_t avg_execution_time;    // Average time per qubit (ms)
    uint32_t variance;              // Execution time variance
    bool has_evaluation;            // Whether evaluation function exists
    bool requires_all;              // Whether all results needed
    uint32_t data_size;             // Size of data per qubit
} quantum_workload_profile_t;

// Performance metrics for a collapse strategy
typedef struct {
    uint32_t total_uses;            // How many times used
    uint32_t success_count;         // Successful collapses
    uint32_t total_time;            // Total time spent
    float avg_quality;              // Average result quality (0-1)
    uint32_t last_used;             // Timestamp of last use
} strategy_metrics_t;

// Learning entry - maps workload characteristics to strategy performance
typedef struct {
    quantum_workload_profile_t profile;
    strategy_metrics_t metrics[COLLAPSE_STRATEGY_COUNT];
    uint32_t observation_count;
    float confidence;               // 0-1, how confident we are
} quantum_learning_entry_t;

// AI Observer state
typedef struct {
    quantum_learning_entry_t* learning_db;
    uint32_t db_size;
    uint32_t db_capacity;
    bool enabled;
    uint32_t total_observations;
} quantum_ai_observer_t;

// Initialize the quantum AI observer
void quantum_ai_init(void);

// Observe a quantum register execution
void quantum_ai_observe_start(QARMA_QUANTUM_REGISTER* reg);
void quantum_ai_observe_complete(QARMA_QUANTUM_REGISTER* reg, uint32_t elapsed_ms, float quality);

// Get recommended collapse strategy for a workload
QARMA_COLLAPSE_STRATEGY quantum_ai_recommend_strategy(quantum_workload_profile_t* profile);

// Get confidence in recommendation (0-1)
float quantum_ai_get_confidence(quantum_workload_profile_t* profile, 
                                 QARMA_COLLAPSE_STRATEGY strategy);

// Profile a quantum register to extract workload characteristics
quantum_workload_profile_t quantum_ai_profile_register(QARMA_QUANTUM_REGISTER* reg);

// Statistics and debugging
void quantum_ai_print_stats(void);
void quantum_ai_reset_learning(void);

// Enable/disable learning
void quantum_ai_set_enabled(bool enabled);

#endif // QUANTUM_AI_OBSERVER_H
