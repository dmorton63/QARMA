/**
 * QARMA - Quantum Register Usage Examples
 * 
 * Examples demonstrating how to use the quantum register system
 * for parallel execution with different collapse strategies.
 */

#include "quantum/quantum_register.h"
#include "quantum/quantum_ai_observer.h"
#include "quantum/quantum_adaptive.h"
#include "quantum/quantum_scheduler.h"
#include "quantum/quantum_cross_learning.h"
#include "core/memory/heap.h"
#include "graphics/graphics.h"
#include "core/memory.h"
#include "config.h"

// ============================================================================
// Example 1: Simple Parallel Computation
// ============================================================================

// Simple function that squares a number
static void square_number(void* data) {
    int* num = (int*)data;
    int result = (*num) * (*num);
    *num = result;  // Store result back
}

void example_simple_parallel(void) {
    GFX_LOG("\n=== Example 1: Simple Parallel Computation ===\n");
    
    // Create register with 4 qubits
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(4);
    if (!reg) {
        GFX_LOG("Failed to create quantum register\n");
        return;
    }
    
    // Prepare data for each qubit
    int numbers[4] = {2, 3, 4, 5};
    
    // Initialize each qubit
    for (int i = 0; i < 4; i++) {
        qarma_qubit_init(reg, i, square_number, &numbers[i], sizeof(int));
    }
    
    // Set collapse strategy - first one to finish wins
    qarma_quantum_set_collapse(reg, COLLAPSE_FIRST_WINS);
    
    // Execute all qubits
    qarma_quantum_execute_sync(reg);
    
    // Print results
    GFX_LOG("Results:\n");
    for (int i = 0; i < 4; i++) {
        GFX_LOG("  ");
        GFX_LOG_HEX("", i);
        GFX_LOG(": ");
        GFX_LOG_HEX("", numbers[i]);
        GFX_LOG("\n");
    }
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 2: Algorithm Race (COLLAPSE_BEST)
// ============================================================================

typedef struct {
    int* array;
    int size;
    int result_time;  // Simulated execution time
} sort_data_t;

static void sort_bubble(void* data) {
    sort_data_t* sd = (sort_data_t*)data;
    // Simulate bubble sort (just set a time)
    sd->result_time = sd->size * sd->size;  // O(n²)
}

static void sort_quick(void* data) {
    sort_data_t* sd = (sort_data_t*)data;
    // Simulate quicksort
    sd->result_time = sd->size * 10;  // O(n log n) approximation
}

static void sort_merge(void* data) {
    sort_data_t* sd = (sort_data_t*)data;
    // Simulate mergesort
    sd->result_time = sd->size * 12;  // O(n log n) with overhead
}

// Evaluation function - lower time is better
static int evaluate_sort_time(void* result) {
    sort_data_t* sd = (sort_data_t*)result;
    return -sd->result_time;  // Negative because higher score wins
}

void example_algorithm_race(void) {
    GFX_LOG("\n=== Example 2: Algorithm Race ===\n");
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(3);
    if (!reg) return;
    
    // Prepare test data
    int test_array[100];
    sort_data_t data[3];
    for (int i = 0; i < 3; i++) {
        data[i].array = test_array;
        data[i].size = 100;
        data[i].result_time = 0;
    }
    
    // Initialize qubits with different sorting algorithms
    qarma_qubit_init(reg, 0, sort_bubble, &data[0], sizeof(sort_data_t));
    qarma_qubit_init(reg, 1, sort_quick, &data[1], sizeof(sort_data_t));
    qarma_qubit_init(reg, 2, sort_merge, &data[2], sizeof(sort_data_t));
    
    // Use COLLAPSE_BEST strategy with evaluation function
    qarma_quantum_set_collapse(reg, COLLAPSE_BEST);
    qarma_quantum_set_evaluate(reg, evaluate_sort_time);
    
    // Execute and find best algorithm
    qarma_quantum_execute_sync(reg);
    
    GFX_LOG("Algorithm times:\n");
    GFX_LOG("  Bubble: ");
    GFX_LOG_HEX("", data[0].result_time);
    GFX_LOG("\n  Quick: ");
    GFX_LOG_HEX("", data[1].result_time);
    GFX_LOG("\n  Merge: ");
    GFX_LOG_HEX("", data[2].result_time);
    GFX_LOG("\n");
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 3: Redundant Computation (COLLAPSE_VALIDATE)
// ============================================================================

typedef struct {
    int a;
    int b;
    int result;
} compute_data_t;

static void compute_sum(void* data) {
    compute_data_t* cd = (compute_data_t*)data;
    cd->result = cd->a + cd->b;
}

void example_redundant_computation(void) {
    GFX_LOG("\n=== Example 3: Redundant Computation ===\n");
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(3);
    if (!reg) return;
    
    // Three identical computations for error detection
    compute_data_t data[3];
    for (int i = 0; i < 3; i++) {
        data[i].a = 10;
        data[i].b = 20;
        data[i].result = 0;
        qarma_qubit_init(reg, i, compute_sum, &data[i], sizeof(compute_data_t));
    }
    
    // Use COLLAPSE_VALIDATE - all results must match
    qarma_quantum_set_collapse(reg, COLLAPSE_VALIDATE);
    reg->result_size = sizeof(compute_data_t);
    
    qarma_quantum_execute_sync(reg);
    
    GFX_LOG("All three computations completed:\n");
    for (int i = 0; i < 3; i++) {
        GFX_LOG("  Result ");
        GFX_LOG_HEX("", i);
        GFX_LOG(": ");
        GFX_LOG_HEX("", data[i].result);
        GFX_LOG("\n");
    }
    
    // Collapse will validate all match
    void* collapsed = qarma_quantum_collapse(reg);
    if (collapsed) {
        GFX_LOG("Validation passed - all results match!\n");
    }
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 4: Data Parallel Processing (COLLAPSE_COMBINE)
// ============================================================================

typedef struct {
    int start_index;
    int count;
    int sum;
} range_sum_t;

static void sum_range(void* data) {
    range_sum_t* rs = (range_sum_t*)data;
    rs->sum = 0;
    // Simulate summing a range of numbers
    SERIAL_LOG("sum_range: start=");
    SERIAL_LOG_HEX("", rs->start_index);
    SERIAL_LOG(" count=");
    SERIAL_LOG_HEX("", rs->count);
    SERIAL_LOG("\n");
    for (int i = rs->start_index; i < rs->start_index + rs->count; i++) {
        rs->sum += i;
    }
    SERIAL_LOG("sum_range: result=");
    SERIAL_LOG_HEX("", rs->sum);
    SERIAL_LOG("\n");
}

static void combine_sums(void** results, uint32_t count, void* output) {
    SERIAL_LOG("combine_sums: count=");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG("\n");
    
    int* total = (int*)output;
    *total = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        range_sum_t* rs = (range_sum_t*)results[i];
        SERIAL_LOG("  result[");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG("]=");
        SERIAL_LOG_HEX("", (uint32_t)rs);
        SERIAL_LOG(" sum=");
        SERIAL_LOG_HEX("", rs->sum);
        SERIAL_LOG("\n");
        *total += rs->sum;
    }
    
    SERIAL_LOG("combine_sums: total=");
    SERIAL_LOG_HEX("", *total);
    SERIAL_LOG("\n");
}

void example_data_parallel(void) {
    GFX_LOG("\n=== Example 4: Data Parallel Processing ===\n");
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(4);
    if (!reg) return;
    
    // Split work across 4 qubits (sum 0-99)
    range_sum_t ranges[4];
    for (int i = 0; i < 4; i++) {
        ranges[i].start_index = i * 25;
        ranges[i].count = 25;
        ranges[i].sum = 0;
        qarma_qubit_init(reg, i, sum_range, &ranges[i], sizeof(range_sum_t));
    }
    
    // Use COLLAPSE_COMBINE to merge partial results
    qarma_quantum_set_collapse(reg, COLLAPSE_COMBINE);
    qarma_quantum_set_combine(reg, combine_sums);
    
    int total_sum = 0;
    reg->collapse_output = &total_sum;
    
    qarma_quantum_execute_sync(reg);
    
    GFX_LOG("Partial sums:\n");
    SERIAL_LOG("Partial sums:\n");
    for (int i = 0; i < 4; i++) {
        GFX_LOG("  Range ");
        GFX_LOG_HEX("", i);
        GFX_LOG(": ");
        GFX_LOG_HEX("", ranges[i].sum);
        GFX_LOG("\n");
        
        SERIAL_LOG("  Range ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG(": ");
        SERIAL_LOG_HEX("", ranges[i].sum);
        SERIAL_LOG("\n");
    }
    
    qarma_quantum_collapse(reg);
    
    GFX_LOG("Total sum: ");
    GFX_LOG_HEX("", total_sum);
    GFX_LOG(" (expected: 4950)\n");
    
    SERIAL_LOG("Total sum: ");
    SERIAL_LOG_HEX("", total_sum);
    SERIAL_LOG(" (expected 4950)\n");
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 5: AI-Recommended Strategy
// ============================================================================

void example_ai_recommended(void) {
    GFX_LOG("\n=== Example 5: AI-Recommended Strategy ===\n");
    SERIAL_LOG("\n=== Example 5: AI-Recommended Strategy ===\n");
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(4);
    if (!reg) return;
    
    // Setup similar to data parallel example
    range_sum_t ranges[4];
    for (int i = 0; i < 4; i++) {
        ranges[i].start_index = i * 25;
        ranges[i].count = 25;
        ranges[i].sum = 0;
        qarma_qubit_init(reg, i, sum_range, &ranges[i], sizeof(range_sum_t));
    }
    
    // Profile the workload
    quantum_workload_profile_t profile = quantum_ai_profile_register(reg);
    profile.requires_all = true;
    
    // Get AI recommendation
    QARMA_COLLAPSE_STRATEGY recommended = quantum_ai_recommend_strategy(&profile);
    float confidence = quantum_ai_get_confidence(&profile, recommended);
    
    GFX_LOG("AI recommends strategy: ");
    GFX_LOG_HEX("", recommended);
    GFX_LOG(" (confidence: ");
    GFX_LOG_HEX("", (uint32_t)(confidence * 100));
    GFX_LOG("%)");
    GFX_LOG("\n");
    
    SERIAL_LOG("AI recommends strategy: ");
    SERIAL_LOG_HEX("", recommended);
    SERIAL_LOG(" confidence=");
    SERIAL_LOG_HEX("", (uint32_t)(confidence * 100));
    SERIAL_LOG("%\n");
    
    // Use AI recommendation
    qarma_quantum_set_collapse(reg, recommended);
    if (recommended == COLLAPSE_COMBINE) {
        qarma_quantum_set_combine(reg, combine_sums);
        int total_sum = 0;
        reg->collapse_output = &total_sum;
    }
    
    // Execute and time it
    quantum_ai_observe_start(reg);
    uint32_t start_time = 0; // TODO: Get actual timestamp
    
    qarma_quantum_execute_sync(reg);
    
    uint32_t elapsed = 1; // TODO: Calculate actual elapsed time
    float quality = 1.0f; // Assume success
    
    quantum_ai_observe_complete(reg, elapsed, quality);
    
    GFX_LOG("Execution completed successfully!\n");
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 6: Adaptive Strategy Switching
// ============================================================================

void example_adaptive_execution(void) {
    GFX_LOG("\n=== Example 6: Adaptive Strategy Switching ===\n");
    SERIAL_LOG("\n=== Example 6: Adaptive Strategy Switching ===\n");
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(8);
    if (!reg) return;
    
    // Setup a larger workload
    range_sum_t ranges[8];
    for (int i = 0; i < 8; i++) {
        ranges[i].start_index = i * 50;
        ranges[i].count = 50;
        ranges[i].sum = 0;
        qarma_qubit_init(reg, i, sum_range, &ranges[i], sizeof(range_sum_t));
    }
    
    // Start with FIRST_WINS strategy
    qarma_quantum_set_collapse(reg, COLLAPSE_FIRST_WINS);
    
    // Enable adaptive execution with AGGRESSIVE policy
    quantum_adaptive_init(reg, ADAPTIVE_POLICY_AGGRESSIVE);
    
    GFX_LOG("Starting with FIRST_WINS, adaptive mode enabled\n");
    SERIAL_LOG("Adaptive execution starting with FIRST_WINS\n");
    
    // Execute with adaptive monitoring
    qarma_quantum_execute_sync(reg);
    
    // Check if strategy was switched
    quantum_adaptive_state_t* adaptive = quantum_adaptive_get_state(reg);
    if (adaptive && adaptive->has_switched) {
        GFX_LOG("Strategy was adapted to: ");
        GFX_LOG_HEX("", adaptive->current_strategy);
        GFX_LOG(" (switches: ");
        GFX_LOG_HEX("", adaptive->switch_count);
        GFX_LOG(")\n");
        
        SERIAL_LOG("Adaptive: Final strategy=");
        SERIAL_LOG_HEX("", adaptive->current_strategy);
        SERIAL_LOG(" switches=");
        SERIAL_LOG_HEX("", adaptive->switch_count);
        SERIAL_LOG("\n");
    } else {
        GFX_LOG("Strategy remained FIRST_WINS (no adaptation needed)\n");
        SERIAL_LOG("No adaptation occurred\n");
    }
    
    // Show partial results
    GFX_LOG("Partial sums: ");
    int total = 0;
    for (int i = 0; i < 8; i++) {
        total += ranges[i].sum;
    }
    GFX_LOG_HEX("", total);
    GFX_LOG(" (expected: 19900)\n");
    
    SERIAL_LOG("Total: ");
    SERIAL_LOG_HEX("", total);
    SERIAL_LOG(" (expected 0x4DBC = 19900)\n");
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 7: Predictive Scheduling
// ============================================================================

// Variable-time task (simulates different workloads)
static void variable_task(void* data) {
    range_sum_t* rs = (range_sum_t*)data;
    rs->sum = 0;
    
    // Simulate variable complexity based on count
    for (int i = rs->start_index; i < rs->start_index + rs->count; i++) {
        rs->sum += i;
        // Simulate more work for larger counts
        for (int j = 0; j < rs->count / 10; j++) {
            rs->sum += 0; // Busy work
        }
    }
}

void example_predictive_scheduling(void) {
    GFX_LOG("\n=== Example 7: Predictive Scheduling ===\n");
    SERIAL_LOG("\n=== Example 7: Predictive Scheduling ===\n");
    
    // Initialize scheduler
    quantum_scheduler_init();
    quantum_scheduler_set_strategy(SCHEDULE_AI_PREDICTED);
    
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(6);
    if (!reg) return;
    
    // Create tasks with varying complexity
    range_sum_t ranges[6];
    uint32_t counts[] = {10, 50, 20, 80, 30, 100}; // Varying sizes
    
    for (int i = 0; i < 6; i++) {
        ranges[i].start_index = i * 20;
        ranges[i].count = counts[i];
        ranges[i].sum = 0;
        qarma_qubit_init(reg, i, variable_task, &ranges[i], sizeof(range_sum_t));
    }
    
    // Let scheduler predict and order
    quantum_scheduler_predict(reg);
    
    uint32_t order_size = 0;
    uint32_t* order = quantum_scheduler_get_order(reg, &order_size);
    
    if (order) {
        GFX_LOG("Scheduler predicted optimal order (longest first)\n");
        SERIAL_LOG("Optimal execution order determined\n");
        
        // Execute in predicted order
        qarma_quantum_execute_sync(reg);
        
        // Learn from results
        quantum_scheduler_learn(reg);
        
        heap_free(order);
    }
    
    GFX_LOG("All tasks completed with predictive scheduling!\n");
    
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 8: Cross-System Learning (qubits communicate during execution)
// ============================================================================

// Shared structure for distributed search
typedef struct {
    uint32_t target_value;      // What we're searching for
    uint32_t search_start;      // Start of this qubit's range
    uint32_t search_end;        // End of this qubit's range
    uint32_t* found_at;         // Output: where target was found
    bool_t* found;              // Output: was target found
    uint32_t qubit_id;          // This qubit's ID
} search_task_t;

// Qubit function: Search for target in range, check for messages from others
static int search_with_communication(void* arg) {
    search_task_t* task = (search_task_t*)arg;
    cross_message_t msg;
    
    SERIAL_LOG("Qubit searching range for target\n");
    
    // Search our range
    for (uint32_t i = task->search_start; i < task->search_end; i++) {
        // Check if another qubit found it
        if (cross_learning_receive_message(task->qubit_id, &msg)) {
            if (msg.type == MSG_BEST_FOUND) {
                SERIAL_LOG("Qubit received found message, aborting\n");
                *(task->found) = TRUE;
                
                // Copy found location from message
                if (msg.data && msg.data_size >= sizeof(uint32_t)) {
                    *(task->found_at) = *(uint32_t*)msg.data;
                }
                
                return 1; // Success (someone else found it)
            }
        }
        
        // Check if we found it
        if (i == task->target_value) {
            SERIAL_LOG("Qubit FOUND target!\n");
            
            *(task->found) = TRUE;
            *(task->found_at) = i;
            
            // Broadcast to other qubits to stop searching
            cross_learning_broadcast_best(task->qubit_id, &i, sizeof(i), 100);
            
            return 0; // Success (we found it)
        }
    }
    
    SERIAL_LOG("Qubit target not in range\n");
    return -1; // Not found in our range
}

static void example_cross_system_learning(void) {
    GFX_LOG("\n=== Example 8: Cross-System Learning ===\n");
    SERIAL_LOG("\n=== Example 8: Cross-System Learning ===\n");
    
    // Initialize cross-learning
    quantum_cross_learning_init();
    
    // Create register - use QARMA_QUANTUM_REGISTER type
    QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(6);
    if (!reg) {
        SERIAL_LOG("Failed to create quantum register\n");
        return;
    }
    
    // Set collapse strategy
    qarma_quantum_set_collapse(reg, COLLAPSE_FIRST_WINS);
    
    // Shared outputs
    uint32_t found_at = 0;
    bool_t found = FALSE;
    
    // Target value to search for (in range 0-30000)
    uint32_t target = 23456;
    
    SERIAL_LOG("Distributed search across 6 qubits\n");
    SERIAL_LOG("Target: ");
    SERIAL_LOG_HEX("", target);
    SERIAL_LOG("\n");
    
    // Create search tasks for each qubit
    search_task_t tasks[6];
    int results[6];  // Result storage for each qubit
    
    for (uint32_t i = 0; i < 6; i++) {
        tasks[i].target_value = target;
        tasks[i].search_start = i * 5000;
        tasks[i].search_end = (i + 1) * 5000;
        tasks[i].found_at = &found_at;
        tasks[i].found = &found;
        tasks[i].qubit_id = i;
        
        // Wrap int-returning function to void-returning function
        // For now, just use the function directly
        qarma_qubit_init(reg, i, (void (*)(void*))search_with_communication, &tasks[i], sizeof(int));
    }
    
    // Execute with FIRST_WINS - first qubit to find it stops others
    SERIAL_LOG("Starting distributed search...\n");
    qarma_quantum_execute_sync(reg);
    
    // Check result
    if (found) {
        GFX_LOG("SUCCESS: Target found!\n");
        SERIAL_LOG("Target found at: ");
        SERIAL_LOG_HEX("", found_at);
        SERIAL_LOG("\n");
        
        // Show communication benefits
        uint32_t best_qubit = 0;
        if (cross_learning_check_convergence(&best_qubit)) {
            SERIAL_LOG("Convergence by qubit: ");
            SERIAL_LOG_HEX("", best_qubit);
            SERIAL_LOG("\n");
        }
    } else {
        SERIAL_LOG("Target not found\n");
    }
    
    // Print cross-learning statistics
    cross_learning_print_stats();
    
    // Cleanup
    cross_learning_clear_messages();
    qarma_quantum_register_destroy(reg);
}

// ============================================================================
// Example 9: Advanced Collapse Strategies (Fuzzy, Progressive, Speculative)
// ============================================================================

typedef struct {
    int quality;           // Quality score (0-100)
    int computation_cost;  // Simulated computation cost
    int result_value;      // The actual result
} optimization_result_t;

// Qubit function: Simulate optimization algorithm with varying quality
static void optimization_task(void* arg) {
    optimization_result_t* result = (optimization_result_t*)arg;
    
    // Simulate different optimization approaches
    // Each qubit just keeps its data as-is
    // The quality is already set in the structure
    (void)result; // Result is already prepared
}

// Evaluation function for optimization results
static int evaluate_optimization(void* result) {
    optimization_result_t* opt = (optimization_result_t*)result;
    return opt->quality;
}

static void example_advanced_collapse_strategies(void) {
    GFX_LOG("\n=== Example 9: Advanced Collapse Strategies ===\n");
    SERIAL_LOG("\n=== Example 9: Advanced Collapse Strategies ===\n");
    
    // Test data: 5 optimization results with varying quality
    optimization_result_t fuzzy_results[5] = {
        {.quality = 60, .computation_cost = 10, .result_value = 100},
        {.quality = 85, .computation_cost = 25, .result_value = 170},  // Best
        {.quality = 45, .computation_cost = 5,  .result_value = 90},
        {.quality = 70, .computation_cost = 15, .result_value = 140},
        {.quality = 55, .computation_cost = 8,  .result_value = 110}
    };
    
    optimization_result_t prog_results[5] = {
        {.quality = 40, .computation_cost = 10, .result_value = 80},
        {.quality = 55, .computation_cost = 12, .result_value = 110},
        {.quality = 70, .computation_cost = 15, .result_value = 140},  // Will find this
        {.quality = 50, .computation_cost = 8,  .result_value = 100},
        {.quality = 45, .computation_cost = 7,  .result_value = 90}
    };
    
    optimization_result_t spec_results[5] = {
        {.quality = 65, .computation_cost = 10, .result_value = 130},  // Speculate this
        {.quality = 90, .computation_cost = 30, .result_value = 180},  // But this is better!
        {.quality = 60, .computation_cost = 12, .result_value = 120},
        {.quality = 55, .computation_cost = 8,  .result_value = 110},
        {.quality = 70, .computation_cost = 15, .result_value = 140}
    };
    
    // Test 1: FUZZY collapse (probabilistic weighting)
    GFX_LOG("\n--- Test 1: FUZZY Collapse ---\n");
    SERIAL_LOG("\n--- Test 1: FUZZY Collapse ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(optimization_result_t);  // Set result size for collapse_output allocation
        qarma_quantum_set_collapse(reg, COLLAPSE_FUZZY);
        qarma_quantum_set_evaluate(reg, evaluate_optimization);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, optimization_task, 
                           &fuzzy_results[i], sizeof(optimization_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        optimization_result_t* result = (optimization_result_t*)reg->collapse_output;
        
        GFX_LOG("FUZZY result: quality=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(", value=");
        GFX_LOG_DEC("", result->result_value);
        GFX_LOG("\n");
        
        SERIAL_LOG("FUZZY final: quality=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(", value=");
        SERIAL_LOG_DEC("", result->result_value);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 2: PROGRESSIVE collapse (iterative refinement)
    GFX_LOG("\n--- Test 2: PROGRESSIVE Collapse ---\n");
    SERIAL_LOG("\n--- Test 2: PROGRESSIVE Collapse ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(optimization_result_t);  // Set result size for collapse_output allocation
        qarma_quantum_set_collapse(reg, COLLAPSE_PROGRESSIVE);
        qarma_quantum_set_evaluate(reg, evaluate_optimization);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, optimization_task, 
                           &prog_results[i], sizeof(optimization_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        optimization_result_t* result = (optimization_result_t*)reg->collapse_output;
        
        GFX_LOG("PROGRESSIVE result: quality=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(", value=");
        GFX_LOG_DEC("", result->result_value);
        GFX_LOG("\n");
        
        SERIAL_LOG("PROGRESSIVE final: quality=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(", value=");
        SERIAL_LOG_DEC("", result->result_value);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 3: SPECULATIVE collapse (predictive execution)
    GFX_LOG("\n--- Test 3: SPECULATIVE Collapse ---\n");
    SERIAL_LOG("\n--- Test 3: SPECULATIVE Collapse ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(optimization_result_t);  // Set result size for collapse_output allocation
        qarma_quantum_set_collapse(reg, COLLAPSE_SPECULATIVE);
        qarma_quantum_set_evaluate(reg, evaluate_optimization);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, optimization_task, 
                           &spec_results[i], sizeof(optimization_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        optimization_result_t* result = (optimization_result_t*)reg->collapse_output;
        
        GFX_LOG("SPECULATIVE result: quality=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(", value=");
        GFX_LOG_DEC("", result->result_value);
        GFX_LOG("\n");
        
        SERIAL_LOG("SPECULATIVE final: quality=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(", value=");
        SERIAL_LOG_DEC("", result->result_value);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    SERIAL_LOG("\nAdvanced strategies comparison complete\n");
}

// ============================================================================
// Example 10: Multi-dimensional Collapse
// ============================================================================

typedef struct {
    int quality;        // Output quality (0-100)
    int speed;          // Execution speed (operations per tick)
    int resources;      // Resource usage (lower is better, 0-100)
    int result_value;   // Actual computed result
} multidim_result_t;

static multidim_result_t multidim_data[5];

static void multidim_algorithm(void* data) {
    multidim_result_t* result = (multidim_result_t*)data;
    
    // Simulate different algorithm characteristics
    // Each algorithm has tradeoffs between quality, speed, and resources
    
    // Algorithm already has preset quality/speed/resource values
    // Just compute a result based on quality
    result->result_value = result->quality * 10 + result->speed;
}

static int evaluate_quality(void* data) {
    multidim_result_t* result = (multidim_result_t*)data;
    return result->quality;
}

static int evaluate_speed(void* data) {
    multidim_result_t* result = (multidim_result_t*)data;
    return result->speed;
}

static int evaluate_resources(void* data) {
    multidim_result_t* result = (multidim_result_t*)data;
    // Invert resource usage so lower usage = higher score
    return 100 - result->resources;
}

static void example_multidimensional_collapse(void) {
    GFX_LOG("\n=== Example 10: Multi-dimensional Collapse ===\n");
    SERIAL_LOG("\n=== Example 10: Multi-dimensional Collapse ===\n");
    
    // Test: Balance quality, speed, and resource usage
    // Different algorithms with different characteristics:
    // - High quality but slow and resource-heavy
    // - Fast but lower quality
    // - Resource-efficient but moderate quality/speed
    // - Balanced approach
    // - Extremely fast but low quality and high resources
    
    multidim_data[0] = (multidim_result_t){95, 30, 80, 0};  // High quality, slow, heavy
    multidim_data[1] = (multidim_result_t){60, 90, 40, 0};  // Fast, moderate quality
    multidim_data[2] = (multidim_result_t){70, 50, 20, 0};  // Resource-efficient
    multidim_data[3] = (multidim_result_t){80, 70, 50, 0};  // Balanced
    multidim_data[4] = (multidim_result_t){40, 95, 90, 0};  // Very fast, poor quality/resources
    
    GFX_LOG("\nTesting multi-dimensional collapse with different weight configurations\n");
    SERIAL_LOG("\nTesting multi-dimensional collapse with different weight configurations\n");
    
    // Test 1: Prioritize quality (70% quality, 20% speed, 10% resources)
    GFX_LOG("\n--- Test 1: Quality-focused (70/20/10) ---\n");
    SERIAL_LOG("\n--- Test 1: Quality-focused (70/20/10) ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(multidim_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_MULTIDIM);
        qarma_quantum_set_multidim(reg, evaluate_quality, evaluate_speed, evaluate_resources,
                                   70, 20, 10);  // Quality-focused weights
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, multidim_algorithm, 
                           &multidim_data[i], sizeof(multidim_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        multidim_result_t* result = (multidim_result_t*)reg->collapse_output;
        
        GFX_LOG("Quality-focused result: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" R=");
        GFX_LOG_DEC("", result->resources);
        GFX_LOG("\n");
        
        SERIAL_LOG("Quality-focused: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" R=");
        SERIAL_LOG_DEC("", result->resources);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 2: Prioritize speed (20% quality, 70% speed, 10% resources)
    GFX_LOG("\n--- Test 2: Speed-focused (20/70/10) ---\n");
    SERIAL_LOG("\n--- Test 2: Speed-focused (20/70/10) ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(multidim_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_MULTIDIM);
        qarma_quantum_set_multidim(reg, evaluate_quality, evaluate_speed, evaluate_resources,
                                   20, 70, 10);  // Speed-focused weights
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, multidim_algorithm, 
                           &multidim_data[i], sizeof(multidim_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        multidim_result_t* result = (multidim_result_t*)reg->collapse_output;
        
        GFX_LOG("Speed-focused result: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" R=");
        GFX_LOG_DEC("", result->resources);
        GFX_LOG("\n");
        
        SERIAL_LOG("Speed-focused: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" R=");
        SERIAL_LOG_DEC("", result->resources);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 3: Balanced (33% quality, 33% speed, 34% resources)
    GFX_LOG("\n--- Test 3: Balanced (33/33/34) ---\n");
    SERIAL_LOG("\n--- Test 3: Balanced (33/33/34) ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(multidim_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_MULTIDIM);
        qarma_quantum_set_multidim(reg, evaluate_quality, evaluate_speed, evaluate_resources,
                                   33, 33, 34);  // Balanced weights
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, multidim_algorithm, 
                           &multidim_data[i], sizeof(multidim_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        multidim_result_t* result = (multidim_result_t*)reg->collapse_output;
        
        GFX_LOG("Balanced result: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" R=");
        GFX_LOG_DEC("", result->resources);
        GFX_LOG("\n");
        
        SERIAL_LOG("Balanced: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" R=");
        SERIAL_LOG_DEC("", result->resources);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    GFX_LOG("\nMulti-dimensional collapse allows selecting results based on\n");
    GFX_LOG("multiple criteria with configurable weights.\n");
    
    SERIAL_LOG("\nMulti-dimensional collapse demonstration complete\n");
}

// ============================================================================
// Example 11: Temporal Collapse
// ============================================================================

typedef struct {
    int iteration;      // Which iteration this is
    int base_quality;   // Base quality level
    int trend;          // Trend direction (+improving, -degrading, 0=stable)
    int current_quality; // Computed quality for this iteration
} temporal_result_t;

static temporal_result_t temporal_data[5];

static void temporal_algorithm(void* data) {
    temporal_result_t* result = (temporal_result_t*)data;
    
    // Simulate algorithm behavior over time
    // Apply trend to base quality
    result->current_quality = result->base_quality + (result->trend * result->iteration);
    
    // Cap quality between 0 and 100
    if (result->current_quality < 0) result->current_quality = 0;
    if (result->current_quality > 100) result->current_quality = 100;
}

static int evaluate_temporal(void* data) {
    temporal_result_t* result = (temporal_result_t*)data;
    return result->current_quality;
}

static void run_temporal_iteration(QARMA_QUANTUM_REGISTER* reg, int iteration) {
    // Update iteration for all algorithms
    for (uint32_t i = 0; i < 5; i++) {
        temporal_data[i].iteration = iteration;
    }
    
    // Reset register for new iteration
    reg->collapsed = false;
    
    for (uint32_t i = 0; i < 5; i++) {
        qarma_qubit_init(reg, i, temporal_algorithm, 
                       &temporal_data[i], sizeof(temporal_result_t));
    }
    
    qarma_quantum_execute_sync(reg);
    qarma_quantum_collapse(reg);
    temporal_result_t* result = (temporal_result_t*)reg->collapse_output;
    
    GFX_LOG("Iter ");
    GFX_LOG_HEX("", iteration);
    GFX_LOG(": Alg");
    GFX_LOG_HEX("", result->base_quality);  // Use base as ID
    GFX_LOG(" Q=");
    GFX_LOG_DEC("", result->current_quality);
    GFX_LOG(" T=");
    if (result->trend >= 0) {
        GFX_LOG("+");
    }
    GFX_LOG_DEC("", result->trend);
    GFX_LOG("\n");
    
    SERIAL_LOG("Iteration ");
    SERIAL_LOG_HEX("", iteration);
    SERIAL_LOG(": Selected base=");
    SERIAL_LOG_DEC("", result->base_quality);
    SERIAL_LOG(" quality=");
    SERIAL_LOG_DEC("", result->current_quality);
    SERIAL_LOG(" trend=");
    SERIAL_LOG_DEC("", result->trend);
    SERIAL_LOG("\n");
}

static void example_temporal_collapse(void) {
    GFX_LOG("\n=== Example 11: Temporal Collapse ===\n");
    SERIAL_LOG("\n=== Example 11: Temporal Collapse ===\n");
    
    // Initialize algorithms with different characteristics:
    // 0: Starts high (85) but degrading (-3 per iteration)
    // 1: Starts medium (60) but improving (+5 per iteration)
    // 2: Starts low (40) but improving rapidly (+8 per iteration)
    // 3: Starts high (80) and stable (0 trend)
    // 4: Starts medium (65) but degrading slowly (-2 per iteration)
    
    temporal_data[0] = (temporal_result_t){0, 85, -3, 0};  // High but degrading
    temporal_data[1] = (temporal_result_t){0, 60, 5, 0};   // Medium, improving
    temporal_data[2] = (temporal_result_t){0, 40, 8, 0};   // Low, improving rapidly
    temporal_data[3] = (temporal_result_t){0, 80, 0, 0};   // High, stable
    temporal_data[4] = (temporal_result_t){0, 65, -2, 0};  // Medium, degrading slowly
    
    GFX_LOG("\nTemporal collapse tracks execution history and trends\n");
    GFX_LOG("to predict which algorithm will perform best over time.\n");
    SERIAL_LOG("\nTemporal collapse tracks execution history and trends\n");
    
    // Test 1: Low trend weight (focus on current quality)
    GFX_LOG("\n--- Test 1: Low trend weight (20) - Focus on current ---\n");
    SERIAL_LOG("\n--- Test 1: Low trend weight (20) - Focus on current ---\n");
    SERIAL_LOG("Expected: Should initially prefer algorithms with high current quality\n");
    
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(temporal_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_TEMPORAL);
        qarma_quantum_set_evaluate(reg, evaluate_temporal);
        qarma_quantum_set_temporal(reg, 5, 20);  // Low trend weight
        
        for (int iter = 1; iter <= 4; iter++) {
            run_temporal_iteration(reg, iter);
        }
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Reset for next test
    temporal_data[0] = (temporal_result_t){0, 85, -3, 0};
    temporal_data[1] = (temporal_result_t){0, 60, 5, 0};
    temporal_data[2] = (temporal_result_t){0, 40, 8, 0};
    temporal_data[3] = (temporal_result_t){0, 80, 0, 0};
    temporal_data[4] = (temporal_result_t){0, 65, -2, 0};
    
    // Test 2: High trend weight (focus on trends)
    GFX_LOG("\n--- Test 2: High trend weight (80) - Focus on trends ---\n");
    SERIAL_LOG("\n--- Test 2: High trend weight (80) - Focus on trends ---\n");
    SERIAL_LOG("Expected: Should prefer algorithms with positive trends\n");
    
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(temporal_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_TEMPORAL);
        qarma_quantum_set_evaluate(reg, evaluate_temporal);
        qarma_quantum_set_temporal(reg, 5, 80);  // High trend weight
        
        for (int iter = 1; iter <= 4; iter++) {
            run_temporal_iteration(reg, iter);
        }
        
        qarma_quantum_register_destroy(reg);
    }
    
    GFX_LOG("\nTemporal collapse enables predicting future performance\n");
    GFX_LOG("based on historical trends and current quality.\n");
    
    SERIAL_LOG("\nTemporal collapse demonstration complete\n");
}

// ============================================================================
// Example 12: Ensemble Collapse
// ============================================================================

typedef struct {
    int quality;
    int speed;
    int consistency;  // How consistent/reliable
    int result_value;
} ensemble_result_t;

static ensemble_result_t ensemble_data[5];

static void ensemble_algorithm(void* data) {
    ensemble_result_t* result = (ensemble_result_t*)data;
    result->result_value = result->quality + result->speed + result->consistency;
}

static int evaluate_ensemble_quality(void* data) {
    ensemble_result_t* result = (ensemble_result_t*)data;
    return result->quality;
}

static int evaluate_ensemble_speed(void* data) {
    ensemble_result_t* result = (ensemble_result_t*)data;
    return result->speed;
}

static int evaluate_ensemble_consistency(void* data) {
    ensemble_result_t* result = (ensemble_result_t*)data;
    return result->consistency;
}

static void example_ensemble_collapse(void) {
    GFX_LOG("\n=== Example 12: Ensemble Collapse ===\n");
    SERIAL_LOG("\n=== Example 12: Ensemble Collapse ===\n");
    
    // Initialize algorithms with different characteristics:
    // 0: Best quality (90) but slower (40) and less consistent (60)
    // 1: Balanced all around (70, 70, 70)
    // 2: Very fast (95) but lower quality (50) and consistency (55)
    // 3: Most consistent (95) with good quality (80) but slower (45)
    // 4: Poor all around (40, 45, 50)
    
    ensemble_data[0] = (ensemble_result_t){90, 40, 60, 0};  // Quality winner
    ensemble_data[1] = (ensemble_result_t){70, 70, 70, 0};  // Balanced
    ensemble_data[2] = (ensemble_result_t){50, 95, 55, 0};  // Speed winner
    ensemble_data[3] = (ensemble_result_t){80, 45, 95, 0};  // Consistency winner
    ensemble_data[4] = (ensemble_result_t){40, 45, 50, 0};  // Poor performer
    
    GFX_LOG("\nEnsemble collapse combines multiple strategies\n");
    GFX_LOG("to make robust decisions through voting.\n");
    SERIAL_LOG("\nEnsemble collapse combines multiple strategies\n");
    
    // Test 1: Quality + Speed ensemble (equal weights)
    GFX_LOG("\n--- Test 1: BEST(quality) + BEST(speed) ---\n");
    SERIAL_LOG("\n--- Test 1: BEST(quality) + BEST(speed) ensemble ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(ensemble_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_ENSEMBLE);
        qarma_quantum_set_evaluate(reg, evaluate_ensemble_quality);
        
        // Configure multidim for speed evaluation
        qarma_quantum_set_multidim(reg, evaluate_ensemble_quality, evaluate_ensemble_speed, NULL,
                                   0, 100, 0);  // Use speed dimension
        
        qarma_quantum_set_ensemble(reg, COLLAPSE_BEST, 50, COLLAPSE_MULTIDIM, 50,
                                   COLLAPSE_STRATEGY_COUNT, 0);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, ensemble_algorithm, 
                           &ensemble_data[i], sizeof(ensemble_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        ensemble_result_t* result = (ensemble_result_t*)reg->collapse_output;
        
        GFX_LOG("Ensemble result: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" C=");
        GFX_LOG_DEC("", result->consistency);
        GFX_LOG("\n");
        
        SERIAL_LOG("Quality+Speed ensemble: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" C=");
        SERIAL_LOG_DEC("", result->consistency);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 2: Three-way ensemble (quality, speed, consistency)
    GFX_LOG("\n--- Test 2: 3-way ensemble (Q+S+C) ---\n");
    SERIAL_LOG("\n--- Test 2: 3-way ensemble with quality, speed, consistency ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(ensemble_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_ENSEMBLE);
        qarma_quantum_set_evaluate(reg, evaluate_ensemble_quality);
        
        // Set up multidim for each dimension
        qarma_quantum_set_multidim(reg, evaluate_ensemble_quality, NULL, NULL,
                                   100, 0, 0);  // Quality
        
        // Three strategies: BEST(quality), FUZZY(speed), BEST(consistency)
        qarma_quantum_set_ensemble(reg, COLLAPSE_BEST, 40, COLLAPSE_FUZZY, 30,
                                   COLLAPSE_MULTIDIM, 30);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, ensemble_algorithm, 
                           &ensemble_data[i], sizeof(ensemble_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        ensemble_result_t* result = (ensemble_result_t*)reg->collapse_output;
        
        GFX_LOG("3-way ensemble: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" C=");
        GFX_LOG_DEC("", result->consistency);
        GFX_LOG("\n");
        
        SERIAL_LOG("3-way ensemble: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" C=");
        SERIAL_LOG_DEC("", result->consistency);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    // Test 3: Weighted ensemble (prefer quality over speed)
    GFX_LOG("\n--- Test 3: Weighted (Q=70, S=30) ---\n");
    SERIAL_LOG("\n--- Test 3: Weighted ensemble (quality=70, speed=30) ---\n");
    {
        QARMA_QUANTUM_REGISTER* reg = qarma_quantum_register_create(5);
        reg->result_size = sizeof(ensemble_result_t);
        qarma_quantum_set_collapse(reg, COLLAPSE_ENSEMBLE);
        qarma_quantum_set_evaluate(reg, evaluate_ensemble_quality);
        
        qarma_quantum_set_multidim(reg, NULL, evaluate_ensemble_speed, NULL,
                                   0, 100, 0);  // Speed
        
        qarma_quantum_set_ensemble(reg, COLLAPSE_BEST, 70, COLLAPSE_MULTIDIM, 30,
                                   COLLAPSE_STRATEGY_COUNT, 0);
        
        for (uint32_t i = 0; i < 5; i++) {
            qarma_qubit_init(reg, i, ensemble_algorithm, 
                           &ensemble_data[i], sizeof(ensemble_result_t));
        }
        
        qarma_quantum_execute_sync(reg);
        qarma_quantum_collapse(reg);
        ensemble_result_t* result = (ensemble_result_t*)reg->collapse_output;
        
        GFX_LOG("Weighted ensemble: Q=");
        GFX_LOG_DEC("", result->quality);
        GFX_LOG(" S=");
        GFX_LOG_DEC("", result->speed);
        GFX_LOG(" C=");
        GFX_LOG_DEC("", result->consistency);
        GFX_LOG("\n");
        
        SERIAL_LOG("Weighted ensemble: Q=");
        SERIAL_LOG_DEC("", result->quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", result->speed);
        SERIAL_LOG(" C=");
        SERIAL_LOG_DEC("", result->consistency);
        SERIAL_LOG("\n");
        
        qarma_quantum_register_destroy(reg);
    }
    
    GFX_LOG("\nEnsemble collapse provides robust decision-making by\n");
    GFX_LOG("combining insights from multiple strategies.\n");
    
    SERIAL_LOG("\nEnsemble collapse demonstration complete\n");
    SERIAL_LOG("\n========================================\n");
    SERIAL_LOG("   ALL 7 FEATURES COMPLETE!\n");
    SERIAL_LOG("========================================\n");
}

// ============================================================================
// Run all examples
// ============================================================================

void quantum_register_run_examples(void) {
    GFX_LOG("\n");
    GFX_LOG("========================================\n");
    GFX_LOG("   QARMA Quantum Register Examples\n");
    GFX_LOG("========================================\n");
    
    // Initialize AI observer
    quantum_ai_init();
    
    example_simple_parallel();
    example_algorithm_race();
    example_redundant_computation();
    example_data_parallel();
    example_ai_recommended();
    example_adaptive_execution();
    example_predictive_scheduling();
    example_cross_system_learning();
    example_advanced_collapse_strategies();
    example_multidimensional_collapse();
    example_temporal_collapse();
    example_ensemble_collapse();
    
    // Print AI and scheduler statistics
    quantum_ai_print_stats();
    quantum_scheduler_print_stats();
    
    GFX_LOG("\n");
    GFX_LOG("========================================\n");
    GFX_LOG("   All examples completed!\n");
    GFX_LOG("========================================\n");
    GFX_LOG("\n");
}
