/**
 * QARMA - Quantum Predictive Scheduler
 * 
 * AI-powered scheduling that predicts execution times and optimizes
 * qubit execution order for maximum parallelism.
 */

#ifndef QUANTUM_SCHEDULER_H
#define QUANTUM_SCHEDULER_H

#include "kernel_types.h"
#include "quantum/quantum_register.h"

// Scheduling strategies
typedef enum {
    SCHEDULE_SEQUENTIAL,        // Execute in order (no optimization)
    SCHEDULE_RANDOM,            // Random order
    SCHEDULE_LONGEST_FIRST,     // Longest predicted tasks first
    SCHEDULE_SHORTEST_FIRST,    // Shortest predicted tasks first
    SCHEDULE_BALANCED,          // Balance load across cores
    SCHEDULE_AI_PREDICTED       // AI predicts and optimizes
} quantum_schedule_strategy_t;

// Qubit execution prediction
typedef struct {
    uint32_t qubit_index;
    uint32_t predicted_time_ms;
    float confidence;           // 0-1, how confident we are
    uint32_t complexity_score;  // Computed complexity hint
} qubit_prediction_t;

// Scheduler state
typedef struct {
    quantum_schedule_strategy_t strategy;
    
    // Predictions for current register
    qubit_prediction_t* predictions;
    uint32_t prediction_count;
    
    // Learning database
    struct {
        uint32_t data_size;
        uint32_t avg_time_ms;
        uint32_t sample_count;
    } learned_patterns[32];     // Simple pattern cache
    uint32_t pattern_count;
    
    // Statistics
    uint32_t total_scheduled;
    uint32_t predictions_accurate; // Within 20% of actual
    float avg_prediction_error;
} quantum_scheduler_t;

// Initialize scheduler
void quantum_scheduler_init(void);

// Set scheduling strategy
void quantum_scheduler_set_strategy(quantum_schedule_strategy_t strategy);

// Predict execution times for all qubits in a register
void quantum_scheduler_predict(QARMA_QUANTUM_REGISTER* reg);

// Get optimal execution order based on predictions
uint32_t* quantum_scheduler_get_order(QARMA_QUANTUM_REGISTER* reg, uint32_t* order_size);

// Learn from actual execution (called after completion)
void quantum_scheduler_learn(QARMA_QUANTUM_REGISTER* reg);

// Get prediction for a specific qubit
qubit_prediction_t* quantum_scheduler_get_prediction(QARMA_QUANTUM_REGISTER* reg, uint32_t index);

// Print scheduler statistics
void quantum_scheduler_print_stats(void);

#endif // QUANTUM_SCHEDULER_H
