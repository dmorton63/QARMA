/**
 * QARMA - Quantum Predictive Scheduler Implementation
 */

#include "quantum/quantum_scheduler.h"
#include "core/memory/heap.h"
#include "config.h"

// Global scheduler state
static quantum_scheduler_t g_scheduler = {
    .strategy = SCHEDULE_AI_PREDICTED,
    .predictions = NULL,
    .prediction_count = 0,
    .pattern_count = 0,
    .total_scheduled = 0,
    .predictions_accurate = 0,
    .avg_prediction_error = 0.0f
};

// Initialize scheduler
void quantum_scheduler_init(void) {
    SERIAL_LOG("Quantum Scheduler: Initializing with AI prediction\n");
    g_scheduler.strategy = SCHEDULE_AI_PREDICTED;
    g_scheduler.pattern_count = 0;
    g_scheduler.total_scheduled = 0;
}

// Set scheduling strategy
void quantum_scheduler_set_strategy(quantum_schedule_strategy_t strategy) {
    g_scheduler.strategy = strategy;
    SERIAL_LOG("Scheduler: Strategy set to ");
    SERIAL_LOG_HEX("", strategy);
    SERIAL_LOG("\n");
}

// Helper: Estimate complexity based on data size
static uint32_t estimate_complexity(uint32_t data_size) {
    // Simple heuristic: larger data = more time
    // In reality, this would analyze the function
    return data_size * 10;
}

// Helper: Find learned pattern for data size
static uint32_t lookup_learned_time(uint32_t data_size) {
    for (uint32_t i = 0; i < g_scheduler.pattern_count; i++) {
        if (g_scheduler.learned_patterns[i].data_size == data_size) {
            return g_scheduler.learned_patterns[i].avg_time_ms;
        }
    }
    return 0; // Not found
}

// Predict execution times for all qubits
void quantum_scheduler_predict(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return;
    
    SERIAL_LOG("Scheduler: Predicting execution times for ");
    SERIAL_LOG_HEX("", reg->count);
    SERIAL_LOG(" qubits\n");
    
    // Allocate prediction array
    if (g_scheduler.predictions) {
        heap_free(g_scheduler.predictions);
    }
    
    g_scheduler.predictions = (qubit_prediction_t*)heap_alloc(
        sizeof(qubit_prediction_t) * reg->count);
    
    if (!g_scheduler.predictions) {
        SERIAL_LOG("Warning: Failed to allocate predictions\n");
        return;
    }
    
    g_scheduler.prediction_count = reg->count;
    
    // Predict each qubit
    for (uint32_t i = 0; i < reg->count; i++) {
        QARMA_QUBIT* qubit = &reg->qubits[i];
        qubit_prediction_t* pred = &g_scheduler.predictions[i];
        
        pred->qubit_index = i;
        pred->confidence = 0.5f; // Base confidence
        
        // Estimate complexity
        pred->complexity_score = estimate_complexity(qubit->result_size);
        
        // Look up learned patterns
        uint32_t learned_time = lookup_learned_time(qubit->result_size);
        
        if (learned_time > 0) {
            // We have historical data
            pred->predicted_time_ms = learned_time;
            pred->confidence = 0.8f;
            
            SERIAL_LOG("  Qubit ");
            SERIAL_LOG_HEX("", i);
            SERIAL_LOG(": predicted=");
            SERIAL_LOG_HEX("", learned_time);
            SERIAL_LOG("ms (learned)\n");
        } else {
            // Use heuristic
            pred->predicted_time_ms = pred->complexity_score / 100;
            if (pred->predicted_time_ms < 1) pred->predicted_time_ms = 1;
            pred->confidence = 0.3f;
            
            SERIAL_LOG("  Qubit ");
            SERIAL_LOG_HEX("", i);
            SERIAL_LOG(": predicted=");
            SERIAL_LOG_HEX("", pred->predicted_time_ms);
            SERIAL_LOG("ms (heuristic)\n");
        }
    }
    
    g_scheduler.total_scheduled += reg->count;
}

// Get optimal execution order
uint32_t* quantum_scheduler_get_order(QARMA_QUANTUM_REGISTER* reg, uint32_t* order_size) {
    if (!reg || !g_scheduler.predictions) {
        *order_size = 0;
        return NULL;
    }
    
    *order_size = reg->count;
    uint32_t* order = (uint32_t*)heap_alloc(sizeof(uint32_t) * reg->count);
    if (!order) return NULL;
    
    // Copy indices
    for (uint32_t i = 0; i < reg->count; i++) {
        order[i] = i;
    }
    
    // Sort based on strategy
    if (g_scheduler.strategy == SCHEDULE_LONGEST_FIRST || 
        g_scheduler.strategy == SCHEDULE_AI_PREDICTED) {
        
        // Bubble sort - longest first
        for (uint32_t i = 0; i < reg->count - 1; i++) {
            for (uint32_t j = 0; j < reg->count - i - 1; j++) {
                uint32_t idx1 = order[j];
                uint32_t idx2 = order[j + 1];
                
                if (g_scheduler.predictions[idx1].predicted_time_ms < 
                    g_scheduler.predictions[idx2].predicted_time_ms) {
                    // Swap
                    uint32_t temp = order[j];
                    order[j] = order[j + 1];
                    order[j + 1] = temp;
                }
            }
        }
        
        SERIAL_LOG("Scheduler: Ordered longest-first\n");
        for (uint32_t i = 0; i < reg->count; i++) {
            SERIAL_LOG("  ");
            SERIAL_LOG_HEX("", i);
            SERIAL_LOG(": qubit ");
            SERIAL_LOG_HEX("", order[i]);
            SERIAL_LOG(" (");
            SERIAL_LOG_HEX("", g_scheduler.predictions[order[i]].predicted_time_ms);
            SERIAL_LOG("ms)\n");
        }
    } else if (g_scheduler.strategy == SCHEDULE_SHORTEST_FIRST) {
        // Sort shortest first
        for (uint32_t i = 0; i < reg->count - 1; i++) {
            for (uint32_t j = 0; j < reg->count - i - 1; j++) {
                uint32_t idx1 = order[j];
                uint32_t idx2 = order[j + 1];
                
                if (g_scheduler.predictions[idx1].predicted_time_ms > 
                    g_scheduler.predictions[idx2].predicted_time_ms) {
                    uint32_t temp = order[j];
                    order[j] = order[j + 1];
                    order[j + 1] = temp;
                }
            }
        }
        
        SERIAL_LOG("Scheduler: Ordered shortest-first\n");
    }
    // SCHEDULE_SEQUENTIAL and SCHEDULE_RANDOM keep original order
    
    return order;
}

// Learn from actual execution
void quantum_scheduler_learn(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg || !g_scheduler.predictions) return;
    
    SERIAL_LOG("Scheduler: Learning from execution results\n");
    
    // Compare predictions to actual times
    uint32_t accurate_count = 0;
    float total_error = 0.0f;
    
    for (uint32_t i = 0; i < reg->count; i++) {
        QARMA_QUBIT* qubit = &reg->qubits[i];
        
        if (qubit->status != QUBIT_STATUS_COMPLETED) continue;
        
        uint32_t actual_time = qubit->end_time - qubit->start_time;
        qubit_prediction_t* pred = &g_scheduler.predictions[i];
        
        // Calculate error
        int32_t error = (int32_t)actual_time - (int32_t)pred->predicted_time_ms;
        float error_pct = (actual_time > 0) ? 
                         ((float)error / (float)actual_time) : 0.0f;
        
        if (error_pct < 0) error_pct = -error_pct;
        total_error += error_pct;
        
        if (error_pct < 0.2f) {  // Within 20%
            accurate_count++;
        }
        
        SERIAL_LOG("  Qubit ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG(": predicted=");
        SERIAL_LOG_HEX("", pred->predicted_time_ms);
        SERIAL_LOG("ms, actual=");
        SERIAL_LOG_HEX("", actual_time);
        SERIAL_LOG("ms, error=");
        SERIAL_LOG_HEX("", (uint32_t)(error_pct * 100));
        SERIAL_LOG("%\n");
        
        // Update learned patterns
        bool found = false;
        for (uint32_t j = 0; j < g_scheduler.pattern_count; j++) {
            if (g_scheduler.learned_patterns[j].data_size == qubit->result_size) {
                // Update existing pattern
                uint32_t old_avg = g_scheduler.learned_patterns[j].avg_time_ms;
                uint32_t samples = g_scheduler.learned_patterns[j].sample_count;
                
                // Exponential moving average
                g_scheduler.learned_patterns[j].avg_time_ms = 
                    (old_avg * samples + actual_time) / (samples + 1);
                g_scheduler.learned_patterns[j].sample_count++;
                
                found = true;
                break;
            }
        }
        
        if (!found && g_scheduler.pattern_count < 32) {
            // Add new pattern
            g_scheduler.learned_patterns[g_scheduler.pattern_count].data_size = qubit->result_size;
            g_scheduler.learned_patterns[g_scheduler.pattern_count].avg_time_ms = actual_time;
            g_scheduler.learned_patterns[g_scheduler.pattern_count].sample_count = 1;
            g_scheduler.pattern_count++;
        }
    }
    
    // Update global statistics
    if (reg->count > 0) {
        g_scheduler.avg_prediction_error = 
            (g_scheduler.avg_prediction_error * 0.7f) + (total_error / reg->count * 0.3f);
        g_scheduler.predictions_accurate += accurate_count;
    }
    
    SERIAL_LOG("Scheduler: ");
    SERIAL_LOG_HEX("", accurate_count);
    SERIAL_LOG("/");
    SERIAL_LOG_HEX("", reg->count);
    SERIAL_LOG(" predictions accurate, avg_error=");
    SERIAL_LOG_HEX("", (uint32_t)(g_scheduler.avg_prediction_error * 100));
    SERIAL_LOG("%\n");
}

// Get prediction for specific qubit
qubit_prediction_t* quantum_scheduler_get_prediction(QARMA_QUANTUM_REGISTER* reg, uint32_t index) {
    if (!reg || !g_scheduler.predictions || index >= g_scheduler.prediction_count) {
        return NULL;
    }
    return &g_scheduler.predictions[index];
}

// Print statistics
void quantum_scheduler_print_stats(void) {
    GFX_LOG("\n=== Quantum Scheduler Statistics ===\n");
    GFX_LOG("Total qubits scheduled: ");
    GFX_LOG_HEX("", g_scheduler.total_scheduled);
    GFX_LOG("\nAccurate predictions: ");
    GFX_LOG_HEX("", g_scheduler.predictions_accurate);
    GFX_LOG("\nLearned patterns: ");
    GFX_LOG_HEX("", g_scheduler.pattern_count);
    GFX_LOG("\n");
    
    SERIAL_LOG("\nScheduler Statistics:\n");
    SERIAL_LOG("  Total scheduled: ");
    SERIAL_LOG_HEX("", g_scheduler.total_scheduled);
    SERIAL_LOG("\n  Accurate: ");
    SERIAL_LOG_HEX("", g_scheduler.predictions_accurate);
    SERIAL_LOG("\n  Patterns: ");
    SERIAL_LOG_HEX("", g_scheduler.pattern_count);
    SERIAL_LOG("\n");
}
