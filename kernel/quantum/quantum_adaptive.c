/**
 * QARMA - Quantum Adaptive Execution Implementation
 */

#include "quantum/quantum_adaptive.h"
#include "quantum/quantum_ai_observer.h"
#include "core/memory/heap.h"
#include "config.h"

// Default thresholds
static const quantum_adaptive_thresholds_t DEFAULT_THRESHOLDS = {
    .timeout_ms = 5000,           // 5 second timeout
    .failure_threshold = 0.3f,    // 30% failure rate
    .quality_threshold = 0.5f,    // 50% quality minimum
    .check_interval_ms = 100      // Check every 100ms
};

// Initialize adaptive execution
void quantum_adaptive_init(QARMA_QUANTUM_REGISTER* reg, quantum_adaptive_policy_t policy) {
    if (!reg) return;
    
    if (!reg->adaptive_state) {
        reg->adaptive_state = (quantum_adaptive_state_t*)heap_alloc(sizeof(quantum_adaptive_state_t));
        if (!reg->adaptive_state) {
            SERIAL_LOG("Warning: Failed to allocate adaptive state\n");
            return;
        }
    }
    
    quantum_adaptive_state_t* state = (quantum_adaptive_state_t*)reg->adaptive_state;
    
    state->policy = policy;
    state->thresholds = DEFAULT_THRESHOLDS;
    state->execution_start_time = 0;
    state->last_check_time = 0;
    state->switch_count = 0;
    state->original_strategy = reg->strategy;
    state->current_strategy = reg->strategy;
    state->has_switched = false;
    state->completed_at_last_check = 0;
    state->failed_at_last_check = 0;
    state->current_quality = 1.0f;
    
    SERIAL_LOG("Adaptive execution initialized with policy ");
    SERIAL_LOG_HEX("", policy);
    SERIAL_LOG("\n");
}

// Set custom thresholds
void quantum_adaptive_set_thresholds(QARMA_QUANTUM_REGISTER* reg, 
                                      quantum_adaptive_thresholds_t* thresholds) {
    if (!reg || !reg->adaptive_state || !thresholds) return;
    
    quantum_adaptive_state_t* state = (quantum_adaptive_state_t*)reg->adaptive_state;
    state->thresholds = *thresholds;
}

// Helper: Choose alternative strategy based on current performance
static QARMA_COLLAPSE_STRATEGY choose_alternative_strategy(QARMA_QUANTUM_REGISTER* reg,
                                                           quantum_adaptive_state_t* state) {
    // Profile current workload
    quantum_workload_profile_t profile = quantum_ai_profile_register(reg);
    
    // Ask AI for recommendation (different from current)
    QARMA_COLLAPSE_STRATEGY recommended = quantum_ai_recommend_strategy(&profile);
    
    // If AI recommends same as current, try next strategy
    if (recommended == state->current_strategy) {
        recommended = (QARMA_COLLAPSE_STRATEGY)((state->current_strategy + 1) % (COLLAPSE_STRATEGY_COUNT - 1));
    }
    
    SERIAL_LOG("Adaptive: Switching from strategy ");
    SERIAL_LOG_HEX("", state->current_strategy);
    SERIAL_LOG(" to ");
    SERIAL_LOG_HEX("", recommended);
    SERIAL_LOG("\n");
    
    return recommended;
}

// Check if strategy should be adapted
bool quantum_adaptive_check(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg || !reg->adaptive_state || !reg->executing) {
        return false;
    }
    
    quantum_adaptive_state_t* state = (quantum_adaptive_state_t*)reg->adaptive_state;
    
    // Skip if policy is NONE
    if (state->policy == ADAPTIVE_POLICY_NONE) {
        return false;
    }
    
    // TODO: Get actual current time
    uint32_t current_time = state->execution_start_time + 100; // Simulated
    
    // Check if enough time has passed since last check
    if (current_time - state->last_check_time < state->thresholds.check_interval_ms) {
        return false;
    }
    
    state->last_check_time = current_time;
    
    bool should_switch = false;
    
    // Calculate current metrics
    uint32_t total_completed = reg->completed_count;
    uint32_t total_failed = reg->failed_count;
    uint32_t total_qubits = reg->count;
    uint32_t elapsed = current_time - state->execution_start_time;
    
    float completion_rate = (float)total_completed / (float)total_qubits;
    float failure_rate = (total_completed + total_failed > 0) ? 
                        ((float)total_failed / (float)(total_completed + total_failed)) : 0.0f;
    
    // Check policy conditions
    switch (state->policy) {
        case ADAPTIVE_POLICY_TIMEOUT:
            if (elapsed > state->thresholds.timeout_ms && completion_rate < 0.5f) {
                SERIAL_LOG("Adaptive: Timeout triggered (elapsed=");
                SERIAL_LOG_HEX("", elapsed);
                SERIAL_LOG("ms, completion=");
                SERIAL_LOG_HEX("", (uint32_t)(completion_rate * 100));
                SERIAL_LOG("%)\n");
                should_switch = true;
            }
            break;
            
        case ADAPTIVE_POLICY_FAILURE_RATE:
            if (failure_rate > state->thresholds.failure_threshold) {
                SERIAL_LOG("Adaptive: High failure rate (");
                SERIAL_LOG_HEX("", (uint32_t)(failure_rate * 100));
                SERIAL_LOG("%)\n");
                should_switch = true;
            }
            break;
            
        case ADAPTIVE_POLICY_QUALITY:
            if (state->current_quality < state->thresholds.quality_threshold) {
                SERIAL_LOG("Adaptive: Low quality (");
                SERIAL_LOG_HEX("", (uint32_t)(state->current_quality * 100));
                SERIAL_LOG("%)\n");
                should_switch = true;
            }
            break;
            
        case ADAPTIVE_POLICY_AGGRESSIVE:
            // Switch on any concerning metric
            if (elapsed > state->thresholds.timeout_ms * 0.5f && completion_rate < 0.3f) {
                SERIAL_LOG("Adaptive: Aggressive - slow progress\n");
                should_switch = true;
            } else if (failure_rate > state->thresholds.failure_threshold * 0.7f) {
                SERIAL_LOG("Adaptive: Aggressive - elevated failures\n");
                should_switch = true;
            }
            break;
            
        default:
            break;
    }
    
    // Perform switch if needed
    if (should_switch && !state->has_switched) {
        QARMA_COLLAPSE_STRATEGY new_strategy = choose_alternative_strategy(reg, state);
        
        // Update strategy
        reg->strategy = new_strategy;
        state->current_strategy = new_strategy;
        state->has_switched = true;
        state->switch_count++;
        
        SERIAL_LOG("Adaptive: Strategy switched! Count=");
        SERIAL_LOG_HEX("", state->switch_count);
        SERIAL_LOG("\n");
        
        return true;
    }
    
    state->completed_at_last_check = total_completed;
    state->failed_at_last_check = total_failed;
    
    return false;
}

// Get adaptive state
quantum_adaptive_state_t* quantum_adaptive_get_state(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return NULL;
    return (quantum_adaptive_state_t*)reg->adaptive_state;
}

// Reset adaptive state
void quantum_adaptive_reset(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg || !reg->adaptive_state) return;
    
    quantum_adaptive_state_t* state = (quantum_adaptive_state_t*)reg->adaptive_state;
    state->execution_start_time = 0;
    state->last_check_time = 0;
    state->switch_count = 0;
    state->current_strategy = state->original_strategy;
    state->has_switched = false;
    state->completed_at_last_check = 0;
    state->failed_at_last_check = 0;
    state->current_quality = 1.0f;
    
    reg->strategy = state->original_strategy;
}
