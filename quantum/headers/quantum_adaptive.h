/**
 * QARMA - Quantum Adaptive Execution
 * 
 * Real-time strategy adaptation based on execution performance
 */

#ifndef QUANTUM_ADAPTIVE_H
#define QUANTUM_ADAPTIVE_H

#include "kernel_types.h"
#include "quantum/quantum_register.h"

// Adaptive execution policies
typedef enum {
    ADAPTIVE_POLICY_NONE,           // No adaptation
    ADAPTIVE_POLICY_TIMEOUT,        // Switch if execution takes too long
    ADAPTIVE_POLICY_FAILURE_RATE,   // Switch if too many qubits fail
    ADAPTIVE_POLICY_QUALITY,        // Switch if results are poor quality
    ADAPTIVE_POLICY_AGGRESSIVE      // Switch quickly based on any issue
} quantum_adaptive_policy_t;

// Performance thresholds for adaptation
typedef struct {
    uint32_t timeout_ms;            // Max time before considering switch
    float failure_threshold;        // Max failure rate (0-1) before switch
    float quality_threshold;        // Min quality (0-1) required
    uint32_t check_interval_ms;     // How often to check (100ms default)
} quantum_adaptive_thresholds_t;

// Adaptive execution state
typedef struct {
    quantum_adaptive_policy_t policy;
    quantum_adaptive_thresholds_t thresholds;
    
    // Runtime state
    uint32_t execution_start_time;
    uint32_t last_check_time;
    uint32_t switch_count;
    QARMA_COLLAPSE_STRATEGY original_strategy;
    QARMA_COLLAPSE_STRATEGY current_strategy;
    bool has_switched;
    
    // Performance tracking
    uint32_t completed_at_last_check;
    uint32_t failed_at_last_check;
    float current_quality;
} quantum_adaptive_state_t;

// Initialize adaptive execution for a register
void quantum_adaptive_init(QARMA_QUANTUM_REGISTER* reg, quantum_adaptive_policy_t policy);

// Set custom thresholds
void quantum_adaptive_set_thresholds(QARMA_QUANTUM_REGISTER* reg, 
                                      quantum_adaptive_thresholds_t* thresholds);

// Check if strategy should be adapted (call periodically during execution)
bool quantum_adaptive_check(QARMA_QUANTUM_REGISTER* reg);

// Get adaptive state for inspection
quantum_adaptive_state_t* quantum_adaptive_get_state(QARMA_QUANTUM_REGISTER* reg);

// Reset adaptive state
void quantum_adaptive_reset(QARMA_QUANTUM_REGISTER* reg);

#endif // QUANTUM_ADAPTIVE_H
