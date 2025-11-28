/**
 * QARMA - Quantum Register System
 * 
 * Quantum-inspired parallel execution framework using qubits and registers.
 * Provides a high-level API for distributing work across CPU cores with
 * configurable collapse strategies for result aggregation.
 */

#ifndef QUANTUM_REGISTER_H
#define QUANTUM_REGISTER_H

#include "kernel_types.h"

// Forward declarations
typedef struct QARMA_QUBIT QARMA_QUBIT;
typedef struct QARMA_QUANTUM_REGISTER QARMA_QUANTUM_REGISTER;
typedef struct QARMA_MULTIDIM_CRITERIA QARMA_MULTIDIM_CRITERIA;
typedef struct QARMA_TEMPORAL_HISTORY QARMA_TEMPORAL_HISTORY;
typedef struct QARMA_ENSEMBLE_CONFIG QARMA_ENSEMBLE_CONFIG;

// Collapse strategies for result aggregation
typedef enum {
    COLLAPSE_FIRST_WINS,      // First qubit to complete provides result
    COLLAPSE_LAST_WINS,       // Last qubit to complete provides result
    COLLAPSE_BEST,            // Evaluate all results, pick best based on score
    COLLAPSE_VOTE,            // Majority consensus (requires matching results)
    COLLAPSE_COMBINE,         // Merge all results using custom combiner
    COLLAPSE_VALIDATE,        // All results must match or error
    COLLAPSE_CUSTOM,          // User-provided collapse function
    COLLAPSE_FUZZY,           // Probabilistic weighting of results by quality
    COLLAPSE_PROGRESSIVE,     // Iterative refinement over multiple rounds
    COLLAPSE_SPECULATIVE,     // Predictive execution with rollback
    COLLAPSE_MULTIDIM,        // Multi-dimensional evaluation (quality, speed, resources)
    COLLAPSE_TEMPORAL,        // Time-based collapse considering execution history and trends
    COLLAPSE_ENSEMBLE,        // Ensemble: combine multiple strategies for robust decisions
    COLLAPSE_STRATEGY_COUNT   // Number of strategies (keep last)
} QARMA_COLLAPSE_STRATEGY;

// Ensemble collapse configuration
// Combines multiple strategies for robust decision-making
struct QARMA_ENSEMBLE_CONFIG {
    QARMA_COLLAPSE_STRATEGY strategies[3];  // Up to 3 strategies to combine
    int weights[3];                         // Voting weight for each strategy
    uint32_t num_strategies;                // Number of active strategies
};

// Temporal collapse history
// Tracks execution history and trends for time-based collapse
struct QARMA_TEMPORAL_HISTORY {
    uint64_t* timestamps;       // Execution timestamps for each result
    int* quality_history;       // Historical quality scores
    uint32_t history_size;      // Number of historical entries
    uint32_t window_size;       // Maximum history window size
    int trend_weight;           // Weight for trend vs current quality (0-100)
};

// Multi-dimensional collapse criteria
// Allows evaluating results across multiple dimensions (quality, speed, resources)
struct QARMA_MULTIDIM_CRITERIA {
    int (*quality_func)(void*);    // Evaluate quality/accuracy
    int (*speed_func)(void*);      // Evaluate speed/performance
    int (*resource_func)(void*);   // Evaluate resource usage
    int quality_weight;             // Weight for quality (0-100)
    int speed_weight;               // Weight for speed (0-100)
    int resource_weight;            // Weight for resources (0-100)
};

// Qubit execution status
typedef enum {
    QUBIT_STATUS_PENDING,     // Not yet executed
    QUBIT_STATUS_RUNNING,     // Currently executing
    QUBIT_STATUS_COMPLETED,   // Execution completed successfully
    QUBIT_STATUS_FAILED,      // Execution failed
    QUBIT_STATUS_SKIPPED      // Skipped (disabled)
} QARMA_QUBIT_STATUS;

/**
 * QARMA_QUBIT - The atomic unit of quantum-inspired parallel execution
 * 
 * A qubit represents a single unit of work that can be executed in parallel
 * with other qubits. Think of it as a task descriptor with state.
 */
struct QARMA_QUBIT {
    // Control
    bool enabled;                    // Whether this qubit should execute
    QARMA_QUBIT_STATUS status;       // Current execution status
    
    // Execution
    void (*function)(void*);         // Function to execute
    void* data;                      // Data to pass to function
    
    // Results
    void* result;                    // Where this qubit stores its result
    size_t result_size;              // Size of result data
    
    // Timing (for performance analysis)
    uint64_t start_time;             // When execution started
    uint64_t end_time;               // When execution completed
    
    // Core assignment (set by scheduler)
    uint32_t assigned_core;          // Which CPU core executed this
    
    // User data
    uint32_t id;                     // User-defined identifier
    void* userdata;                  // Additional user data
};

/**
 * Collapse function signatures
 */
typedef void (*qarma_collapse_func_t)(void** results, uint32_t count, void* output);
typedef int (*qarma_evaluate_func_t)(void* result);
typedef void (*qarma_combine_func_t)(void** results, uint32_t count, void* output);

/**
 * QARMA_QUANTUM_REGISTER - Collection of qubits with collapse logic
 * 
 * A quantum register manages a set of qubits and controls how their
 * results are collapsed into a final answer.
 */
struct QARMA_QUANTUM_REGISTER {
    // Qubit management
    QARMA_QUBIT* qubits;             // Array of qubits
    uint32_t count;                  // Number of qubits in register
    uint32_t capacity;               // Allocated capacity
    
    // Collapse configuration
    QARMA_COLLAPSE_STRATEGY strategy; // How to collapse results
    qarma_collapse_func_t custom_collapse;     // Custom collapse function
    qarma_evaluate_func_t evaluate;            // Evaluation function for BEST
    qarma_combine_func_t combine;              // Combination function for COMBINE
    QARMA_MULTIDIM_CRITERIA* multidim;         // Multi-dimensional criteria
    QARMA_TEMPORAL_HISTORY* temporal;          // Temporal history for time-based collapse
    QARMA_ENSEMBLE_CONFIG* ensemble;           // Ensemble configuration for hybrid strategies
    
    // Result management
    void* collapse_output;           // Where final collapsed result goes
    size_t result_size;              // Size of individual results
    bool collapsed;                  // Has collapse been performed?
    
    // Execution tracking
    volatile uint32_t completed_count;  // How many qubits have completed
    volatile uint32_t failed_count;     // How many qubits failed
    bool wait_for_all;               // Wait for all qubits before collapse
    
    // Statistics
    uint64_t total_execution_time;   // Total time for all qubits
    uint64_t collapse_time;          // Time spent collapsing
    
    // Synchronization
    volatile bool executing;         // Currently executing
    uint32_t lock;                   // Spinlock for thread safety
    
    // Adaptive execution (opaque pointer to avoid circular dependency)
    void* adaptive_state;            // Adaptive execution state
    
    // Dispatch tracking
    uint32_t dispatched_count;       // Number of qubits dispatched
};

// ============================================================================
// Quantum Register Management API
// ============================================================================

/**
 * Create a new quantum register with specified qubit capacity
 * @param qubit_count Number of qubits to allocate
 * @return Pointer to new register, or NULL on failure
 */
QARMA_QUANTUM_REGISTER* qarma_quantum_register_create(uint32_t qubit_count);

/**
 * Destroy a quantum register and free all resources
 * @param reg Register to destroy
 */
void qarma_quantum_register_destroy(QARMA_QUANTUM_REGISTER* reg);

/**
 * Reset a quantum register for reuse (keeps allocation)
 * @param reg Register to reset
 */
void qarma_quantum_register_reset(QARMA_QUANTUM_REGISTER* reg);

// ============================================================================
// Qubit Configuration API
// ============================================================================

/**
 * Initialize a qubit with function and data
 * @param reg Register containing the qubit
 * @param index Index of qubit to initialize
 * @param function Function to execute
 * @param data Data to pass to function
 * @param result_size Size of result data
 * @return true on success, false on error
 */
bool qarma_qubit_init(QARMA_QUANTUM_REGISTER* reg, uint32_t index,
                      void (*function)(void*), void* data, size_t result_size);

/**
 * Enable or disable a qubit
 * @param reg Register containing the qubit
 * @param index Index of qubit
 * @param enabled Whether qubit should execute
 */
void qarma_qubit_set_enabled(QARMA_QUANTUM_REGISTER* reg, uint32_t index, bool enabled);

/**
 * Set user-defined ID for a qubit
 * @param reg Register containing the qubit
 * @param index Index of qubit
 * @param id User-defined identifier
 */
void qarma_qubit_set_id(QARMA_QUANTUM_REGISTER* reg, uint32_t index, uint32_t id);

// ============================================================================
// Collapse Strategy Configuration
// ============================================================================

/**
 * Set the collapse strategy for a register
 * @param reg Register to configure
 * @param strategy Collapse strategy to use
 */
void qarma_quantum_set_collapse(QARMA_QUANTUM_REGISTER* reg, 
                                 QARMA_COLLAPSE_STRATEGY strategy);

/**
 * Set custom collapse function (for COLLAPSE_CUSTOM strategy)
 * @param reg Register to configure
 * @param collapse_func Custom collapse function
 */
void qarma_quantum_set_custom_collapse(QARMA_QUANTUM_REGISTER* reg,
                                        qarma_collapse_func_t collapse_func);

/**
 * Set evaluation function (for COLLAPSE_BEST strategy)
 * @param reg Register to configure
 * @param eval_func Function that scores a result (higher = better)
 */
void qarma_quantum_set_evaluate(QARMA_QUANTUM_REGISTER* reg,
                                qarma_evaluate_func_t eval_func);

/**
 * Set combination function (for COLLAPSE_COMBINE strategy)
 * @param reg Register to configure
 * @param combine_func Function that merges multiple results
 */
void qarma_quantum_set_combine(QARMA_QUANTUM_REGISTER* reg,
                               qarma_combine_func_t combine_func);

/**
 * Configure whether to wait for all qubits before collapsing
 * @param reg Register to configure
 * @param wait_all If true, wait for all enabled qubits to complete
 */
void qarma_quantum_set_wait_all(QARMA_QUANTUM_REGISTER* reg, bool wait_all);

// ============================================================================
// Execution API
// ============================================================================

/**
 * Execute all enabled qubits in the register across available CPU cores
 * This function dispatches qubits to cores and returns immediately.
 * @param reg Register to execute
 * @return true if dispatch succeeded, false on error
 */
bool qarma_quantum_execute(QARMA_QUANTUM_REGISTER* reg);

/**
 * Execute all enabled qubits and wait for completion
 * @param reg Register to execute
 * @return true if all qubits completed successfully
 */
bool qarma_quantum_execute_sync(QARMA_QUANTUM_REGISTER* reg);

/**
 * Check if all enabled qubits have completed
 * @param reg Register to check
 * @return true if all enabled qubits are done
 */
bool qarma_quantum_is_complete(QARMA_QUANTUM_REGISTER* reg);

/**
 * Wait for all enabled qubits to complete (blocking)
 * @param reg Register to wait on
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return true if completed, false on timeout
 */
bool qarma_quantum_wait(QARMA_QUANTUM_REGISTER* reg, uint32_t timeout_ms);

// ============================================================================
// Result Collapse API
// ============================================================================

/**
 * Collapse the quantum register and get final result
 * This applies the configured collapse strategy to all completed results.
 * @param reg Register to collapse
 * @return Pointer to collapsed result, or NULL on error
 */
void* qarma_quantum_collapse(QARMA_QUANTUM_REGISTER* reg);

/**
 * Get result from a specific qubit (before collapse)
 * @param reg Register containing the qubit
 * @param index Index of qubit
 * @return Pointer to qubit's result, or NULL if not ready
 */
void* qarma_quantum_get_qubit_result(QARMA_QUANTUM_REGISTER* reg, uint32_t index);

// ============================================================================
// Built-in Collapse Implementations
// ============================================================================

/**
 * First-wins collapse: Return result from first completed qubit
 */
void qarma_collapse_first_wins(void** results, uint32_t count, void* output);

/**
 * Last-wins collapse: Return result from last completed qubit
 */
void qarma_collapse_last_wins(void** results, uint32_t count, void* output);

/**
 * Validate collapse: Ensure all results match, error if not
 */
void qarma_collapse_validate(void** results, uint32_t count, void* output, size_t size);

/**
 * Fuzzy collapse: Probabilistically weight results by quality score
 * Uses evaluation function to score results, then selects probabilistically
 * with higher-quality results having higher selection probability
 */
void qarma_collapse_fuzzy(void** results, uint32_t count, void* output, 
                         size_t size, qarma_evaluate_func_t evaluate);

/**
 * Progressive collapse: Iteratively refine solution over multiple rounds
 * Starts with initial result and progressively improves by adopting better results
 */
void qarma_collapse_progressive(void** results, uint32_t count, void* output, 
                               size_t size, qarma_evaluate_func_t evaluate);

/**
 * Speculative collapse: Predict likely outcome and validate
 * Makes early prediction, validates against other results, rolls back if wrong
 */
void qarma_collapse_speculative(void** results, uint32_t count, void* output, 
                               size_t size, qarma_evaluate_func_t evaluate);

/**
 * Multi-dimensional collapse - evaluate results across multiple criteria
 * @param results Array of result pointers
 * @param count Number of results
 * @param output Buffer to store selected result
 * @param size Size of each result
 * @param criteria Multi-dimensional evaluation criteria with weights
 */
void qarma_collapse_multidim(void** results, uint32_t count, void* output, 
                            size_t size, QARMA_MULTIDIM_CRITERIA* criteria);

/**
 * Set multi-dimensional collapse criteria
 * @param reg Register to configure
 * @param quality_func Function to evaluate quality
 * @param speed_func Function to evaluate speed
 * @param resource_func Function to evaluate resource usage
 * @param quality_weight Weight for quality (0-100)
 * @param speed_weight Weight for speed (0-100)
 * @param resource_weight Weight for resources (0-100)
 */
void qarma_quantum_set_multidim(QARMA_QUANTUM_REGISTER* reg,
                                qarma_evaluate_func_t quality_func,
                                qarma_evaluate_func_t speed_func,
                                qarma_evaluate_func_t resource_func,
                                int quality_weight, int speed_weight, int resource_weight);

/**
 * Temporal collapse - evaluate results based on execution history and trends
 * @param results Array of result pointers
 * @param count Number of results
 * @param output Buffer to store selected result
 * @param size Size of each result
 * @param evaluate Function to evaluate result quality
 * @param history Temporal history structure with timestamps and trends
 */
void qarma_collapse_temporal(void** results, uint32_t count, void* output, 
                            size_t size, int (*evaluate)(void*), 
                            QARMA_TEMPORAL_HISTORY* history);

/**
 * Set temporal collapse configuration
 * @param reg Register to configure
 * @param window_size Maximum number of historical entries to track
 * @param trend_weight Weight for trend vs current quality (0-100)
 */
void qarma_quantum_set_temporal(QARMA_QUANTUM_REGISTER* reg,
                                uint32_t window_size, int trend_weight);

/**
 * Ensemble collapse - combine multiple strategies for robust decision-making
 * @param results Array of result pointers
 * @param count Number of results
 * @param output Buffer to store selected result
 * @param size Size of each result
 * @param reg Register with ensemble configuration
 */
void qarma_collapse_ensemble(void** results, uint32_t count, void* output, 
                            size_t size, QARMA_QUANTUM_REGISTER* reg);

/**
 * Set ensemble collapse configuration
 * @param reg Register to configure
 * @param strategy1 First strategy to combine
 * @param weight1 Voting weight for first strategy (0-100)
 * @param strategy2 Second strategy to combine
 * @param weight2 Voting weight for second strategy (0-100)
 * @param strategy3 Third strategy to combine (optional, use COLLAPSE_STRATEGY_COUNT for none)
 * @param weight3 Voting weight for third strategy (0-100)
 */
void qarma_quantum_set_ensemble(QARMA_QUANTUM_REGISTER* reg,
                                QARMA_COLLAPSE_STRATEGY strategy1, int weight1,
                                QARMA_COLLAPSE_STRATEGY strategy2, int weight2,
                                QARMA_COLLAPSE_STRATEGY strategy3, int weight3);

// ============================================================================
// Statistics and Debugging
// ============================================================================

/**
 * Get statistics about quantum register execution
 */
typedef struct {
    uint32_t total_qubits;
    uint32_t enabled_qubits;
    uint32_t completed_qubits;
    uint32_t failed_qubits;
    uint64_t total_execution_time;
    uint64_t collapse_time;
    uint64_t avg_qubit_time;
} qarma_quantum_stats_t;

/**
 * Get execution statistics for a register
 * @param reg Register to query
 * @param stats Output structure for statistics
 */
void qarma_quantum_get_stats(QARMA_QUANTUM_REGISTER* reg, qarma_quantum_stats_t* stats);

/**
 * Print debug information about a quantum register
 * @param reg Register to debug
 */
void qarma_quantum_debug_print(QARMA_QUANTUM_REGISTER* reg);

#endif // QUANTUM_REGISTER_H
