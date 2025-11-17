/**
 * QARMA - Execution Pipeline System
 * 
 * Core-local execution chains with zero cross-core synchronization.
 * Each pipeline runs entirely on a single CPU core for maximum cache efficiency.
 * 
 * Architecture:
 *   - Pipelines are linked lists of execution nodes
 *   - Each node contains a function pointer and data
 *   - Output from node N becomes input to node N+1
 *   - All execution happens in one core's cache (L1/L2)
 *   - No locks, no barriers, no cross-core data sharing
 * 
 * Benefits:
 *   - Zero synchronization overhead
 *   - Maximum cache efficiency (~4 cycles per access)
 *   - Predictable performance (no contention)
 *   - Easy checkpoint/resume (save current node + data)
 *   - Linear debugging (single execution path)
 */

#ifndef EXECUTION_PIPELINE_H
#define EXECUTION_PIPELINE_H

#include "kernel_types.h"

// ────────────────────────────────────────────────────────────────────────────
// Configuration
// ────────────────────────────────────────────────────────────────────────────

#define MAX_PIPELINE_NODES      32      // Max functions in a pipeline
#define MAX_PIPELINES_PER_CORE  8       // Max concurrent pipelines per core
#define MAX_FUNCTION_NAME       64      // Max length of function semantic name

// ────────────────────────────────────────────────────────────────────────────
// Function Metadata (Glyphic Headers)
// ────────────────────────────────────────────────────────────────────────────

/**
 * Function signature types for compile-time validation
 */
typedef enum {
    SIG_VOID_TO_VOID,       // void func(void)
    SIG_PTR_TO_PTR,         // void* func(void*)
    SIG_PTR_TO_INT,         // int func(void*)
    SIG_INT_TO_PTR,         // void* func(int)
    SIG_CUSTOM              // Custom signature (user validates)
} function_signature_t;

/**
 * Function metadata - describes a pipeline-compatible function
 * 
 * This is the "glyphic header" - semantic metadata that describes
 * what a function does, its interface, and execution properties.
 */
typedef struct {
    char semantic_name[MAX_FUNCTION_NAME];  // e.g., "network.parse_ipv4"
    function_signature_t signature;         // Function signature type
    void* (*func_ptr)(void*);              // Actual function pointer
    uint32_t version_id;                    // For tracking mutations
    float estimated_cycles;                 // Performance estimate
    bool is_resumable;                      // Can be checkpointed mid-execution
    bool is_idempotent;                     // Safe to retry on failure
} glyph_function_t;

// ────────────────────────────────────────────────────────────────────────────
// Execution Pipeline Structures
// ────────────────────────────────────────────────────────────────────────────

/**
 * Single node in an execution pipeline
 * 
 * Each node executes one function, passing output to the next node.
 * Nodes form a singly-linked list for sequential execution.
 */
typedef struct execution_node {
    glyph_function_t* function;             // Function to execute
    void* input_data;                       // Input from previous node
    void* output_data;                      // Output for next node
    struct execution_node* next;            // Next node in pipeline
    
    // Execution state
    bool completed;                         // Has this node finished?
    uint64_t start_cycles;                  // When execution started
    uint64_t end_cycles;                    // When execution finished
    int result_code;                        // Return code (0 = success)
} execution_node_t;

/**
 * Complete execution pipeline
 * 
 * A pipeline is a chain of execution nodes that runs entirely on
 * one CPU core. No cross-core data sharing occurs.
 */
typedef struct {
    uint32_t pipeline_id;                   // Unique identifier
    uint32_t core_id;                       // Which core owns this pipeline
    
    // Node chain
    execution_node_t* head;                 // First node
    execution_node_t* tail;                 // Last node (for append)
    execution_node_t* current;              // Currently executing node
    uint32_t node_count;                    // Total nodes
    
    // Execution state
    bool is_running;                        // Currently executing?
    bool is_complete;                       // All nodes finished?
    bool has_error;                         // Error occurred?
    
    // Performance metrics
    uint64_t total_cycles;                  // Total execution time
    uint32_t cache_misses;                  // L1 cache misses (if available)
    
    // Context
    void* core_local_context;               // Core-specific state/memory
} execution_pipeline_t;

// ────────────────────────────────────────────────────────────────────────────
// Checkpoint/Resume Structures
// ────────────────────────────────────────────────────────────────────────────

/**
 * Pipeline checkpoint for save/resume
 * 
 * Allows a pipeline to be saved mid-execution and resumed later
 * on the same or different core.
 */
typedef struct {
    uint32_t pipeline_id;                   // Which pipeline
    uint32_t original_core_id;              // Original core
    uint32_t current_node_index;            // Where we are in the chain
    void* intermediate_data;                // Output from last completed node
    uint64_t checkpoint_timestamp;          // When saved
    
    // Function-specific state (for resumable functions)
    void* node_states[MAX_PIPELINE_NODES];
} pipeline_checkpoint_t;

// ────────────────────────────────────────────────────────────────────────────
// Core Pipeline Management
// ────────────────────────────────────────────────────────────────────────────

/**
 * Per-core pipeline manager
 * 
 * Each CPU core has its own pipeline manager that tracks
 * all pipelines currently assigned to that core.
 */
typedef struct {
    uint32_t core_id;                       // Which core
    execution_pipeline_t* pipelines[MAX_PIPELINES_PER_CORE];
    uint32_t active_pipeline_count;         // How many running
    bool core_busy;                         // Is core executing?
    
    // Performance tracking
    uint64_t total_pipelines_executed;
    uint64_t total_cycles_used;
} core_pipeline_manager_t;

// ────────────────────────────────────────────────────────────────────────────
// Function Declarations
// ────────────────────────────────────────────────────────────────────────────

// Pipeline creation and management
execution_pipeline_t* pipeline_create(uint32_t core_id);
void pipeline_destroy(execution_pipeline_t* pipeline);
bool pipeline_add_node(execution_pipeline_t* pipeline, glyph_function_t* func);
void pipeline_execute(execution_pipeline_t* pipeline);

// Node management
execution_node_t* node_create(glyph_function_t* func);
void node_destroy(execution_node_t* node);

// Checkpoint/Resume
pipeline_checkpoint_t* pipeline_checkpoint(execution_pipeline_t* pipeline);
bool pipeline_resume(pipeline_checkpoint_t* checkpoint, uint32_t new_core_id);
void checkpoint_destroy(pipeline_checkpoint_t* checkpoint);

// Core management
void pipeline_system_init(void);
core_pipeline_manager_t* get_core_pipeline_manager(uint32_t core_id);
uint32_t find_free_core(void);
bool assign_pipeline_to_core(execution_pipeline_t* pipeline, uint32_t core_id);

// Debugging and introspection
void pipeline_print_status(execution_pipeline_t* pipeline);
void pipeline_print_metrics(execution_pipeline_t* pipeline);

// ────────────────────────────────────────────────────────────────────────────
// TODO / Future Enhancements
// ────────────────────────────────────────────────────────────────────────────

/*
 * FUTURE: Cross-Core Data Sharing
 * 
 * Current design eliminates cross-core sharing for maximum performance.
 * Future enhancement could add optional sharing mechanisms:
 * 
 * 1. Lock-Free Queues: For producer/consumer patterns
 *    - Single-writer, single-reader ring buffers
 *    - No locks, atomic head/tail pointers
 *    - Best for streaming data between cores
 * 
 * 2. Read-Only Shared State: For broadcast data
 *    - One writer, multiple readers
 *    - Version-tracked updates
 *    - Readers detect stale data via generation counter
 * 
 * 3. Result Aggregation: For fork/join patterns
 *    - Multiple pipelines produce results
 *    - Single aggregator collects and merges
 *    - Only share final outputs, not intermediate state
 * 
 * Design Principle: Keep cross-core sharing EXPLICIT and MINIMAL
 * Default should always be core-local execution.
 */

#endif // EXECUTION_PIPELINE_H
