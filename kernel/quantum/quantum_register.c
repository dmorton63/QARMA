/**
 * QARMA - Quantum Register Implementation
 * 
 * Implementation of quantum-inspired parallel execution framework.
 */

#include "quantum/quantum_register.h"
#include "core/memory.h"
#include "core/memory/heap.h"
#include "core/core_manager.h"
#include "parallel/parallel_engine.h"
#include "graphics/graphics.h"
#include "config.h"

// Memory comparison (avoid including string.h due to type conflicts)
extern int memcmp(const void* s1, const void* s2, size_t n);

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * Allocate result buffer for a qubit
 */
static bool qubit_allocate_result(QARMA_QUBIT* qubit, size_t size) {
    if (!qubit || size == 0) return false;
    
    qubit->result = heap_alloc(size);
    if (!qubit->result) {
        return false;
    }
    
    qubit->result_size = size;
    memset(qubit->result, 0, size);
    return true;
}

/**
 * Free result buffer for a qubit
 */
static void qubit_free_result(QARMA_QUBIT* qubit) {
    if (qubit && qubit->result) {
        heap_free(qubit->result);
        qubit->result = NULL;
        qubit->result_size = 0;
    }
}

/**
 * Completion callback structure for tracking qubit execution
 */
typedef struct {
    QARMA_QUBIT* qubit;
    QARMA_QUANTUM_REGISTER* reg;
} qubit_context_t;

/**
 * Wrapper function for qubit execution
 * This is what actually gets dispatched to CPU cores via parallel task system
 */
static void qubit_execute_wrapper(void* context) {
    qubit_context_t* ctx = (qubit_context_t*)context;
    if (!ctx || !ctx->qubit || !ctx->qubit->function) {
        return;
    }
    
    QARMA_QUBIT* qubit = ctx->qubit;
    
    // Mark as running
    qubit->status = QUBIT_STATUS_RUNNING;
    qubit->start_time = 0; // TODO: Get actual timestamp from parallel engine
    
    // Execute the qubit's function
    qubit->function(qubit->data);
    
    // Mark as completed
    qubit->end_time = 0; // TODO: Get actual timestamp
    qubit->status = QUBIT_STATUS_COMPLETED;
    
    // Update register's completion count atomically
    if (ctx->reg) {
        __sync_fetch_and_add(&ctx->reg->completed_count, 1);
    }
    
    // Free the context
    heap_free(ctx);
}

// ============================================================================
// Quantum Register Management
// ============================================================================

QARMA_QUANTUM_REGISTER* qarma_quantum_register_create(uint32_t qubit_count) {
    if (qubit_count == 0) {
        GFX_LOG("Error: Cannot create quantum register with 0 qubits\n");
        return NULL;
    }
    
    // Allocate register structure
    QARMA_QUANTUM_REGISTER* reg = (QARMA_QUANTUM_REGISTER*)heap_alloc(
        sizeof(QARMA_QUANTUM_REGISTER));
    if (!reg) {
        GFX_LOG("Error: Failed to allocate quantum register\n");
        return NULL;
    }
    
    memset(reg, 0, sizeof(QARMA_QUANTUM_REGISTER));
    
    // Allocate qubit array
    reg->qubits = (QARMA_QUBIT*)heap_alloc(sizeof(QARMA_QUBIT) * qubit_count);
    if (!reg->qubits) {
        GFX_LOG("Error: Failed to allocate qubit array\n");
        heap_free(reg);
        return NULL;
    }
    
    memset(reg->qubits, 0, sizeof(QARMA_QUBIT) * qubit_count);
    reg->count = qubit_count;
    reg->capacity = qubit_count;
    
    // Initialize default settings
    reg->strategy = COLLAPSE_FIRST_WINS;
    reg->wait_for_all = true;
    reg->collapsed = false;
    reg->executing = false;
    reg->completed_count = 0;
    reg->failed_count = 0;
    reg->lock = 0;
    
    // Initialize all qubits to disabled/pending
    for (uint32_t i = 0; i < qubit_count; i++) {
        reg->qubits[i].enabled = false;
        reg->qubits[i].status = QUBIT_STATUS_PENDING;
        reg->qubits[i].id = i;
    }
    
    GFX_LOG("Created quantum register with ");
    GFX_LOG_HEX("", qubit_count);
    GFX_LOG(" qubits\n");
    
    return reg;
}

void qarma_quantum_register_destroy(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return;
    
    // Free all qubit results
    if (reg->qubits) {
        for (uint32_t i = 0; i < reg->count; i++) {
            qubit_free_result(&reg->qubits[i]);
        }
        heap_free(reg->qubits);
    }
    
    // Free collapse output if allocated
    if (reg->collapse_output) {
        heap_free(reg->collapse_output);
    }
    
    heap_free(reg);
}

void qarma_quantum_register_reset(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return;
    
    // Reset execution state
    reg->completed_count = 0;
    reg->failed_count = 0;
    reg->collapsed = false;
    reg->executing = false;
    reg->total_execution_time = 0;
    reg->collapse_time = 0;
    
    // Reset all qubits
    for (uint32_t i = 0; i < reg->count; i++) {
        reg->qubits[i].status = QUBIT_STATUS_PENDING;
        reg->qubits[i].start_time = 0;
        reg->qubits[i].end_time = 0;
        reg->qubits[i].assigned_core = 0;
        
        // Note: We don't reset enabled flag or function pointers
        // so the register can be re-executed with same configuration
    }
}

// ============================================================================
// Qubit Configuration
// ============================================================================

bool qarma_qubit_init(QARMA_QUANTUM_REGISTER* reg, uint32_t index,
                      void (*function)(void*), void* data, size_t result_size) {
    if (!reg || index >= reg->count || !function) {
        return false;
    }
    
    QARMA_QUBIT* qubit = &reg->qubits[index];
    
    // Set function and data
    qubit->function = function;
    qubit->data = data;
    qubit->enabled = true;
    qubit->status = QUBIT_STATUS_PENDING;
    
    // Allocate result buffer if needed
    if (result_size > 0) {
        if (!qubit_allocate_result(qubit, result_size)) {
            GFX_LOG("Warning: Failed to allocate result buffer for qubit ");
            GFX_LOG_HEX("", index);
            GFX_LOG("\n");
            return false;
        }
    }
    
    return true;
}

void qarma_qubit_set_enabled(QARMA_QUANTUM_REGISTER* reg, uint32_t index, bool enabled) {
    if (!reg || index >= reg->count) return;
    
    reg->qubits[index].enabled = enabled;
    if (!enabled) {
        reg->qubits[index].status = QUBIT_STATUS_SKIPPED;
    }
}

void qarma_qubit_set_id(QARMA_QUANTUM_REGISTER* reg, uint32_t index, uint32_t id) {
    if (!reg || index >= reg->count) return;
    reg->qubits[index].id = id;
}

// ============================================================================
// Collapse Strategy Configuration
// ============================================================================

void qarma_quantum_set_collapse(QARMA_QUANTUM_REGISTER* reg, 
                                 QARMA_COLLAPSE_STRATEGY strategy) {
    if (!reg) return;
    reg->strategy = strategy;
}

void qarma_quantum_set_custom_collapse(QARMA_QUANTUM_REGISTER* reg,
                                        qarma_collapse_func_t collapse_func) {
    if (!reg) return;
    reg->custom_collapse = collapse_func;
    reg->strategy = COLLAPSE_CUSTOM;
}

void qarma_quantum_set_evaluate(QARMA_QUANTUM_REGISTER* reg,
                                qarma_evaluate_func_t eval_func) {
    if (!reg) return;
    reg->evaluate = eval_func;
}

void qarma_quantum_set_combine(QARMA_QUANTUM_REGISTER* reg,
                               qarma_combine_func_t combine_func) {
    if (!reg) return;
    reg->combine = combine_func;
}

void qarma_quantum_set_wait_all(QARMA_QUANTUM_REGISTER* reg, bool wait_all) {
    if (!reg) return;
    reg->wait_for_all = wait_all;
}

// ============================================================================
// Execution
// ============================================================================

bool qarma_quantum_execute(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg || reg->executing) {
        return false;
    }
    
    reg->executing = true;
    reg->collapsed = false;
    reg->completed_count = 0;
    reg->failed_count = 0;
    
    GFX_LOG("Executing quantum register with ");
    GFX_LOG_HEX("", reg->count);
    GFX_LOG(" qubits across CPU cores...\n");
    
    // Count enabled qubits
    uint32_t enabled_count = 0;
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->qubits[i].enabled) {
            enabled_count++;
        }
    }
    
    GFX_LOG_HEX("Enabled qubits: ", enabled_count);
    GFX_LOG("\n");
    
    // Try to request cores from core manager for quantum subsystem
    // If this fails, parallel engine will still distribute tasks across available cores
    uint32_t cores_needed = enabled_count;
    bool cores_allocated = false;
    if (cores_needed > 0) {
        core_request_t request = {0};
        request.subsystem = SUBSYSTEM_QUANTUM;
        request.core_count = cores_needed;
        request.preferred_numa = UINT32_MAX; // No preference
        request.flags = CORE_ALLOC_SHARED;   // Allow sharing
        
        core_response_t response = core_request_allocate(&request);
        
        if (response.success && response.cores_allocated > 0) {
            GFX_LOG("Allocated ");
            GFX_LOG_HEX("", response.cores_allocated);
            GFX_LOG(" cores for quantum execution\n");
            cores_allocated = true;
        } else {
            GFX_LOG("Note: Using parallel engine default core distribution\n");
        }
    }
    
    // Dispatch each enabled qubit as a parallel task
    uint32_t dispatched = 0;
    for (uint32_t i = 0; i < reg->count; i++) {
        QARMA_QUBIT* qubit = &reg->qubits[i];
        
        if (!qubit->enabled) {
            qubit->status = QUBIT_STATUS_SKIPPED;
            continue;
        }
        
        // Create context for this qubit
        qubit_context_t* ctx = (qubit_context_t*)heap_alloc(sizeof(qubit_context_t));
        if (!ctx) {
            GFX_LOG("Warning: Failed to allocate context for qubit ");
            GFX_LOG_HEX("", i);
            GFX_LOG("\n");
            qubit->status = QUBIT_STATUS_FAILED;
            __sync_fetch_and_add(&reg->failed_count, 1);
            continue;
        }
        
        ctx->qubit = qubit;
        ctx->reg = reg;
        
        // Create parallel task for this qubit
        char task_name[32];
        task_name[0] = 'q';
        task_name[1] = 'b';
        task_name[2] = ':';
        // Simple hex conversion for task ID
        uint32_t id = qubit->id;
        int pos = 3;
        for (int shift = 28; shift >= 0; shift -= 4) {
            uint8_t nibble = (id >> shift) & 0xF;
            task_name[pos++] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
        }
        task_name[pos] = '\0';
        
        // For now, execute directly since parallel engine may not be ticking
        // TODO: Properly integrate with parallel engine tick system
        GFX_LOG("Executing qubit ");
        GFX_LOG_HEX("", i);
        GFX_LOG("...\n");
        
        qubit_execute_wrapper(ctx);
        dispatched++;
        
        // Alternative: Use parallel task system (requires engine to be ticking)
        /*
        parallel_task_t* task = parallel_task_create(task_name, qubit_execute_wrapper, 
                                                     ctx, sizeof(qubit_context_t));
        
        if (task) {
            task->priority = PARALLEL_PRIORITY_NORMAL;
            parallel_task_submit(task);
            dispatched++;
        } else {
            GFX_LOG("Warning: Failed to create parallel task for qubit ");
            GFX_LOG_HEX("", i);
            GFX_LOG("\n");
            heap_free(ctx);
            qubit->status = QUBIT_STATUS_FAILED;
            __sync_fetch_and_add(&reg->failed_count, 1);
        }
        */
    }
    
    GFX_LOG("Dispatched ");
    GFX_LOG_HEX("", dispatched);
    GFX_LOG(" qubits to parallel execution engine\n");
    
    reg->executing = false;
    
    return dispatched > 0;
}

bool qarma_quantum_execute_sync(QARMA_QUANTUM_REGISTER* reg) {
    // Since we're executing qubits directly now (not via parallel engine),
    // all tasks complete synchronously when execute() returns
    if (!qarma_quantum_execute(reg)) {
        return false;
    }
    
    GFX_LOG("All quantum tasks completed\n");
    return qarma_quantum_is_complete(reg);
}

bool qarma_quantum_is_complete(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return false;
    
    // Count enabled qubits
    uint32_t enabled_count = 0;
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->qubits[i].enabled) {
            enabled_count++;
        }
    }
    
    // Check if all enabled qubits have finished
    uint32_t finished = reg->completed_count + reg->failed_count;
    return finished >= enabled_count;
}

bool qarma_quantum_wait(QARMA_QUANTUM_REGISTER* reg, uint32_t timeout_ms) {
    (void)timeout_ms; // TODO: Implement timeout
    
    // Simple spin-wait for now
    while (!qarma_quantum_is_complete(reg)) {
        // In real implementation, would use proper synchronization
    }
    
    return true;
}

// ============================================================================
// Result Collapse
// ============================================================================

void* qarma_quantum_collapse(QARMA_QUANTUM_REGISTER* reg) {
    SERIAL_LOG("qarma_quantum_collapse called, reg=");
    SERIAL_LOG_HEX("", (uint32_t)reg);
    SERIAL_LOG("\n");
    if (!reg) {
        SERIAL_LOG("  reg is NULL!\n");
        return NULL;
    }
    SERIAL_LOG("  collapsed flag=");
    SERIAL_LOG_HEX("", reg->collapsed);
    SERIAL_LOG("\n");
    if (reg->collapsed) {
        SERIAL_LOG("  Already collapsed, returning cached result\n");
        return reg->collapse_output;
    }
    
    SERIAL_LOG("  Not collapsed yet, proceeding\n");
    
    // Ensure all qubits have completed
    if (!qarma_quantum_is_complete(reg)) {
        GFX_LOG("Warning: Collapsing quantum register before all qubits complete\n");
    }
    
    SERIAL_LOG("  Allocating results array\n");
    // Collect results from all completed qubits
    void** results = (void**)heap_alloc(sizeof(void*) * reg->count);
    if (!results) {
        GFX_LOG("Error: Failed to allocate results array for collapse\n");
        return NULL;
    }
    
    uint32_t result_count = 0;
    SERIAL_LOG("  Collecting results from ");
    SERIAL_LOG_HEX("", reg->count);
    SERIAL_LOG(" qubits\n");
    for (uint32_t i = 0; i < reg->count; i++) {
        SERIAL_LOG("    Qubit ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG(": status=");
        SERIAL_LOG_HEX("", reg->qubits[i].status);
        SERIAL_LOG("\n");
        if (reg->qubits[i].status == QUBIT_STATUS_COMPLETED) {
            // Use data pointer (where function operates) rather than result buffer
            // Most qubit functions modify their input data in-place
            void* result_ptr = reg->qubits[i].data;
            if (result_ptr) {
                results[result_count++] = result_ptr;
            }
        }
    }
    
    SERIAL_LOG("  Collected ");
    SERIAL_LOG_HEX("", result_count);
    SERIAL_LOG(" results\n");
    
    SERIAL_LOG("  About to call GFX_LOG\n");
    GFX_LOG("Collapsing ");
    SERIAL_LOG("  After first GFX_LOG\n");
    GFX_LOG_HEX("", result_count);
    SERIAL_LOG("  After GFX_LOG_HEX\n");
    GFX_LOG(" results using strategy ");
    GFX_LOG_HEX("", reg->strategy);
    GFX_LOG("\n");
    SERIAL_LOG("  After all GFX_LOG calls\n");
    
    // Allocate collapse output buffer if not already allocated
    SERIAL_LOG("  Checking collapse_output: current=");
    SERIAL_LOG_HEX("", (uint32_t)reg->collapse_output);
    SERIAL_LOG(" result_size=");
    SERIAL_LOG_HEX("", reg->result_size);
    SERIAL_LOG("\n");
    
    if (!reg->collapse_output && reg->result_size > 0) {
        SERIAL_LOG("  Allocating collapse_output buffer\n");
        reg->collapse_output = heap_alloc(reg->result_size);
        SERIAL_LOG("  After heap_alloc: collapse_output=");
        SERIAL_LOG_HEX("", (uint32_t)reg->collapse_output);
        SERIAL_LOG("\n");
        if (!reg->collapse_output) {
            GFX_LOG("Error: Failed to allocate collapse output buffer\n");
            heap_free(results);
            return NULL;
        }
        memset(reg->collapse_output, 0, reg->result_size);
        SERIAL_LOG("  Buffer initialized\n");
    } else {
        SERIAL_LOG("  Skipping allocation (already allocated or size=0)\n");
    }
    
    // Apply collapse strategy
    SERIAL_LOG("  Entering collapse switch, strategy=");
    SERIAL_LOG_HEX("", reg->strategy);
    SERIAL_LOG("\n");
    switch (reg->strategy) {
        case COLLAPSE_FIRST_WINS:
            SERIAL_LOG("  COLLAPSE_FIRST_WINS case\n");
            if (result_count > 0) {
                qarma_collapse_first_wins(results, result_count, reg->collapse_output);
            }
            break;
            
        case COLLAPSE_LAST_WINS:
            if (result_count > 0) {
                qarma_collapse_last_wins(results, result_count, reg->collapse_output);
            }
            break;
            
        case COLLAPSE_BEST:
            if (result_count > 0 && reg->evaluate) {
                // Find best result
                int best_idx = 0;
                int best_score = reg->evaluate(results[0]);
                
                for (uint32_t i = 1; i < result_count; i++) {
                    int score = reg->evaluate(results[i]);
                    if (score > best_score) {
                        best_score = score;
                        best_idx = i;
                    }
                }
                
                if (reg->collapse_output && reg->result_size > 0) {
                    memcpy(reg->collapse_output, results[best_idx], reg->result_size);
                }
            }
            break;
            
        case COLLAPSE_VOTE:
            // TODO: Implement voting logic
            GFX_LOG("Warning: COLLAPSE_VOTE not yet implemented\n");
            break;
            
        case COLLAPSE_COMBINE:
            SERIAL_LOG("COLLAPSE_COMBINE: result_count=");
            SERIAL_LOG_HEX("", result_count);
            SERIAL_LOG(" combine=");
            SERIAL_LOG_HEX("", (uint32_t)reg->combine);
            SERIAL_LOG(" output=");
            SERIAL_LOG_HEX("", (uint32_t)reg->collapse_output);
            SERIAL_LOG("\n");
            if (result_count > 0 && reg->combine) {
                reg->combine(results, result_count, reg->collapse_output);
            }
            break;
            
        case COLLAPSE_VALIDATE:
            if (result_count > 0 && reg->result_size > 0) {
                qarma_collapse_validate(results, result_count, reg->collapse_output, reg->result_size);
            }
            break;
            
        case COLLAPSE_CUSTOM:
            if (result_count > 0 && reg->custom_collapse) {
                reg->custom_collapse(results, result_count, reg->collapse_output);
            }
            break;
            
        case COLLAPSE_FUZZY:
            SERIAL_LOG("  COLLAPSE_FUZZY case: result_count=");
            SERIAL_LOG_HEX("", result_count);
            SERIAL_LOG(" evaluate=");
            SERIAL_LOG_HEX("", (uint32_t)reg->evaluate);
            SERIAL_LOG("\n");
            if (result_count > 0 && reg->evaluate) {
                SERIAL_LOG("  Calling qarma_collapse_fuzzy\n");
                qarma_collapse_fuzzy(results, result_count, reg->collapse_output, 
                                    reg->result_size, reg->evaluate);
                SERIAL_LOG("  Returned from qarma_collapse_fuzzy\n");
            } else {
                SERIAL_LOG("  Skipping fuzzy (no results or evaluate)\n");
            }
            break;
            
        case COLLAPSE_PROGRESSIVE:
            if (result_count > 0) {
                qarma_collapse_progressive(results, result_count, reg->collapse_output, 
                                          reg->result_size, reg->evaluate);
            }
            break;
            
        case COLLAPSE_SPECULATIVE:
            if (result_count > 0 && reg->evaluate) {
                qarma_collapse_speculative(results, result_count, reg->collapse_output, 
                                          reg->result_size, reg->evaluate);
            }
            break;
            
        case COLLAPSE_MULTIDIM:
            if (result_count > 0 && reg->multidim) {
                qarma_collapse_multidim(results, result_count, reg->collapse_output, 
                                       reg->result_size, reg->multidim);
            }
            break;
            
        case COLLAPSE_TEMPORAL:
            if (result_count > 0 && reg->temporal) {
                qarma_collapse_temporal(results, result_count, reg->collapse_output, 
                                       reg->result_size, reg->evaluate, reg->temporal);
            }
            break;
            
        case COLLAPSE_ENSEMBLE:
            if (result_count > 0 && reg->ensemble) {
                qarma_collapse_ensemble(results, result_count, reg->collapse_output, 
                                       reg->result_size, reg);
            }
            break;
            
        default:
            GFX_LOG("Error: Unknown collapse strategy\n");
            break;
    }
    
    heap_free(results);
    reg->collapsed = true;
    
    return reg->collapse_output;
}

void* qarma_quantum_get_qubit_result(QARMA_QUANTUM_REGISTER* reg, uint32_t index) {
    if (!reg || index >= reg->count) {
        return NULL;
    }
    
    QARMA_QUBIT* qubit = &reg->qubits[index];
    if (qubit->status != QUBIT_STATUS_COMPLETED) {
        return NULL;
    }
    
    return qubit->result;
}

// ============================================================================
// Built-in Collapse Implementations
// ============================================================================

void qarma_collapse_first_wins(void** results, uint32_t count, void* output) {
    if (count == 0 || !results || !output) return;
    
    // Simply return first result
    // Note: Caller must know the size to copy
    // For now, just set the pointer
    *(void**)output = results[0];
}

void qarma_collapse_last_wins(void** results, uint32_t count, void* output) {
    if (count == 0 || !results || !output) return;
    
    // Return last result
    *(void**)output = results[count - 1];
}

void qarma_collapse_validate(void** results, uint32_t count, void* output, size_t size) {
    if (count == 0 || !results || !output || size == 0) return;
    
    // Check that all results match the first one
    for (uint32_t i = 1; i < count; i++) {
        if (memcmp(results[0], results[i], size) != 0) {
            GFX_LOG("Error: COLLAPSE_VALIDATE failed - results don't match!\n");
            return;
        }
    }
    
    // All results match, copy first one
    memcpy(output, results[0], size);
}

void qarma_collapse_fuzzy(void** results, uint32_t count, void* output, 
                         size_t size, qarma_evaluate_func_t evaluate) {
    SERIAL_LOG("qarma_collapse_fuzzy ENTRY: count=");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG(" results=");
    SERIAL_LOG_HEX("", (uint32_t)results);
    SERIAL_LOG(" output=");
    SERIAL_LOG_HEX("", (uint32_t)output);
    SERIAL_LOG(" evaluate=");
    SERIAL_LOG_HEX("", (uint32_t)evaluate);
    SERIAL_LOG("\n");
    
    if (count == 0 || !results || !output || !evaluate) {
        SERIAL_LOG("qarma_collapse_fuzzy: Early return due to null params\n");
        if (count > 0 && results && output) {
            memcpy(output, results[0], size);
        }
        return;
    }
    
    SERIAL_LOG("COLLAPSE_FUZZY: Probabilistic weighting of ");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG(" results\n");
    
    // Evaluate all results and compute weights
    int* scores = (int*)heap_alloc(count * sizeof(int));
    int total_score = 0;
    int max_score = -999999;
    uint32_t best_idx = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        scores[i] = evaluate(results[i]);
        total_score += scores[i];
        
        if (scores[i] > max_score) {
            max_score = scores[i];
            best_idx = i;
        }
        
        SERIAL_LOG("  Result ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG(": score=");
        SERIAL_LOG_DEC("", scores[i]);
        SERIAL_LOG("\n");
    }
    
    // Use probabilistic selection weighted by quality
    // Higher scores have higher probability of selection
    // But lower scores still have a chance (fuzzy logic)
    
    // Simple approach: 70% chance pick best, 30% distributed among others
    uint32_t rand_val = (uint32_t)scores[0] * 1103515245 + 12345; // Simple LCG
    rand_val = (rand_val / 65536) % 100;
    
    uint32_t selected_idx;
    if (rand_val < 70) {
        // Pick best
        selected_idx = best_idx;
        SERIAL_LOG("FUZZY: Selected best (70% probability)\n");
    } else {
        // Pick randomly weighted by scores
        if (total_score > 0) {
            int target = (rand_val * total_score) / 100;
            int cumulative = 0;
            selected_idx = 0;
            
            for (uint32_t i = 0; i < count; i++) {
                cumulative += scores[i];
                if (cumulative >= target) {
                    selected_idx = i;
                    break;
                }
            }
        } else {
            selected_idx = rand_val % count;
        }
        SERIAL_LOG("FUZZY: Selected weighted random (30% probability)\n");
    }
    
    SERIAL_LOG("FUZZY: Final selection: index=");
    SERIAL_LOG_HEX("", selected_idx);
    SERIAL_LOG(" score=");
    SERIAL_LOG_DEC("", scores[selected_idx]);
    SERIAL_LOG("\n");
    
    memcpy(output, results[selected_idx], size);
    heap_free(scores);
}

void qarma_collapse_progressive(void** results, uint32_t count, void* output, 
                               size_t size, qarma_evaluate_func_t evaluate) {
    if (count == 0 || !results || !output) return;
    
    SERIAL_LOG("COLLAPSE_PROGRESSIVE: Iterative refinement over ");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG(" results\n");
    
    // Progressive refinement: start with first result, 
    // then iteratively refine by blending with better results
    memcpy(output, results[0], size);
    
    if (!evaluate) {
        // Without evaluator, just average/blend all results
        SERIAL_LOG("PROGRESSIVE: No evaluator, using simple blend\n");
        return;
    }
    
    int current_score = evaluate(output);
    uint32_t improvements = 0;
    
    SERIAL_LOG("PROGRESSIVE: Initial score=");
    SERIAL_LOG_DEC("", current_score);
    SERIAL_LOG("\n");
    
    // Iterate through results, adopting better ones
    for (uint32_t round = 0; round < 3; round++) {
        for (uint32_t i = 1; i < count; i++) {
            int candidate_score = evaluate(results[i]);
            
            if (candidate_score > current_score) {
                // Found better result, adopt it
                memcpy(output, results[i], size);
                current_score = candidate_score;
                improvements++;
                
                SERIAL_LOG("PROGRESSIVE: Round ");
                SERIAL_LOG_HEX("", round);
                SERIAL_LOG(" improved to score=");
                SERIAL_LOG_DEC("", current_score);
                SERIAL_LOG("\n");
            }
        }
    }
    
    SERIAL_LOG("PROGRESSIVE: Final score=");
    SERIAL_LOG_DEC("", current_score);
    SERIAL_LOG(" (");
    SERIAL_LOG_HEX("", improvements);
    SERIAL_LOG(" improvements)\n");
}

void qarma_collapse_speculative(void** results, uint32_t count, void* output, 
                               size_t size, qarma_evaluate_func_t evaluate) {
    if (count == 0 || !results || !output) return;
    
    SERIAL_LOG("COLLAPSE_SPECULATIVE: Predictive execution with validation\n");
    
    // Speculative execution: predict most likely outcome,
    // validate against other results, rollback if wrong
    
    if (!evaluate || count < 2) {
        // Need at least 2 results and evaluator for speculation
        memcpy(output, results[0], size);
        SERIAL_LOG("SPECULATIVE: Insufficient data, using first result\n");
        return;
    }
    
    // Quick speculation: assume first result is likely correct
    memcpy(output, results[0], size);
    int speculative_score = evaluate(results[0]);
    
    SERIAL_LOG("SPECULATIVE: Predicted result 0, score=");
    SERIAL_LOG_DEC("", speculative_score);
    SERIAL_LOG("\n");
    
    // Validate speculation against other results
    uint32_t confirmations = 0;
    uint32_t contradictions = 0;
    int best_alternative_score = -999999;
    uint32_t best_alternative_idx = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        int score = evaluate(results[i]);
        
        if (score >= speculative_score * 0.9) {
            // Close enough - confirms speculation
            confirmations++;
        } else if (score > best_alternative_score) {
            // Potential contradiction
            contradictions++;
            best_alternative_score = score;
            best_alternative_idx = i;
        }
    }
    
    SERIAL_LOG("SPECULATIVE: Confirmations=");
    SERIAL_LOG_HEX("", confirmations);
    SERIAL_LOG(" contradictions=");
    SERIAL_LOG_HEX("", contradictions);
    SERIAL_LOG("\n");
    
    // Rollback if speculation was wrong
    if (contradictions > confirmations && best_alternative_score > speculative_score) {
        SERIAL_LOG("SPECULATIVE: ROLLBACK! Using alternative result ");
        SERIAL_LOG_HEX("", best_alternative_idx);
        SERIAL_LOG(" with score=");
        SERIAL_LOG_DEC("", best_alternative_score);
        SERIAL_LOG("\n");
        
        memcpy(output, results[best_alternative_idx], size);
    } else {
        SERIAL_LOG("SPECULATIVE: Prediction confirmed\n");
    }
}

void qarma_collapse_multidim(void** results, uint32_t count, void* output, 
                            size_t size, QARMA_MULTIDIM_CRITERIA* criteria) {
    if (count == 0 || !results || !output || !criteria) {
        if (count > 0 && results && output) {
            memcpy(output, results[0], size);
        }
        return;
    }
    
    SERIAL_LOG("COLLAPSE_MULTIDIM: Multi-dimensional evaluation of ");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG(" results\n");
    SERIAL_LOG("  Weights: quality=");
    SERIAL_LOG_DEC("", criteria->quality_weight);
    SERIAL_LOG(" speed=");
    SERIAL_LOG_DEC("", criteria->speed_weight);
    SERIAL_LOG(" resource=");
    SERIAL_LOG_DEC("", criteria->resource_weight);
    SERIAL_LOG("\n");
    
    // Evaluate each result across all dimensions
    int* quality_scores = criteria->quality_func ? (int*)heap_alloc(count * sizeof(int)) : NULL;
    int* speed_scores = criteria->speed_func ? (int*)heap_alloc(count * sizeof(int)) : NULL;
    int* resource_scores = criteria->resource_func ? (int*)heap_alloc(count * sizeof(int)) : NULL;
    int* aggregate_scores = (int*)heap_alloc(count * sizeof(int));
    
    if (!aggregate_scores) {
        memcpy(output, results[0], size);
        return;
    }
    
    // Compute total weight for normalization
    int total_weight = criteria->quality_weight + criteria->speed_weight + criteria->resource_weight;
    if (total_weight == 0) total_weight = 1; // Avoid division by zero
    
    for (uint32_t i = 0; i < count; i++) {
        int quality = criteria->quality_func ? criteria->quality_func(results[i]) : 0;
        int speed = criteria->speed_func ? criteria->speed_func(results[i]) : 0;
        int resource = criteria->resource_func ? criteria->resource_func(results[i]) : 0;
        
        if (quality_scores) quality_scores[i] = quality;
        if (speed_scores) speed_scores[i] = speed;
        if (resource_scores) resource_scores[i] = resource;
        
        // Compute weighted aggregate score
        aggregate_scores[i] = 
            (quality * criteria->quality_weight +
             speed * criteria->speed_weight +
             resource * criteria->resource_weight) / total_weight;
        
        SERIAL_LOG("  Result ");
        SERIAL_LOG_HEX("", i);
        SERIAL_LOG(": Q=");
        SERIAL_LOG_DEC("", quality);
        SERIAL_LOG(" S=");
        SERIAL_LOG_DEC("", speed);
        SERIAL_LOG(" R=");
        SERIAL_LOG_DEC("", resource);
        SERIAL_LOG(" → AGG=");
        SERIAL_LOG_DEC("", aggregate_scores[i]);
        SERIAL_LOG("\n");
    }
    
    // Find result with best aggregate score
    int best_score = aggregate_scores[0];
    uint32_t best_idx = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        if (aggregate_scores[i] > best_score) {
            best_score = aggregate_scores[i];
            best_idx = i;
        }
    }
    
    SERIAL_LOG("MULTIDIM: Selected result ");
    SERIAL_LOG_HEX("", best_idx);
    SERIAL_LOG(" with aggregate score=");
    SERIAL_LOG_DEC("", best_score);
    SERIAL_LOG("\n");
    
    memcpy(output, results[best_idx], size);
    
    // Cleanup
    if (quality_scores) heap_free(quality_scores);
    if (speed_scores) heap_free(speed_scores);
    if (resource_scores) heap_free(resource_scores);
    heap_free(aggregate_scores);
}

void qarma_quantum_set_multidim(QARMA_QUANTUM_REGISTER* reg,
                                qarma_evaluate_func_t quality_func,
                                qarma_evaluate_func_t speed_func,
                                qarma_evaluate_func_t resource_func,
                                int quality_weight, int speed_weight, int resource_weight) {
    if (!reg) return;
    
    // Allocate criteria structure if not already allocated
    if (!reg->multidim) {
        reg->multidim = (QARMA_MULTIDIM_CRITERIA*)heap_alloc(sizeof(QARMA_MULTIDIM_CRITERIA));
        if (!reg->multidim) return;
    }
    
    reg->multidim->quality_func = quality_func;
    reg->multidim->speed_func = speed_func;
    reg->multidim->resource_func = resource_func;
    reg->multidim->quality_weight = quality_weight;
    reg->multidim->speed_weight = speed_weight;
    reg->multidim->resource_weight = resource_weight;
}

void qarma_collapse_temporal(void** results, uint32_t count, void* output, 
                            size_t size, int (*evaluate)(void*), 
                            QARMA_TEMPORAL_HISTORY* history) {
    if (count == 0 || !results || !output) {
        if (count > 0 && results && output) {
            memcpy(output, results[0], size);
        }
        return;
    }
    
    SERIAL_LOG("COLLAPSE_TEMPORAL: Time-based evaluation of ");
    SERIAL_LOG_HEX("", count);
    SERIAL_LOG(" results\n");
    
    if (!history || !evaluate) {
        // No history or evaluator - just pick first
        SERIAL_LOG("TEMPORAL: No history/evaluator, using first result\n");
        memcpy(output, results[0], size);
        return;
    }
    
    SERIAL_LOG("  Window size: ");
    SERIAL_LOG_HEX("", history->window_size);
    SERIAL_LOG(", Trend weight: ");
    SERIAL_LOG_DEC("", history->trend_weight);
    SERIAL_LOG("\n");
    
    // Evaluate current results
    int* current_scores = (int*)heap_alloc(count * sizeof(int));
    int* temporal_scores = (int*)heap_alloc(count * sizeof(int));
    
    if (!current_scores || !temporal_scores) {
        if (current_scores) heap_free(current_scores);
        if (temporal_scores) heap_free(temporal_scores);
        memcpy(output, results[0], size);
        return;
    }
    
    // Get current quality scores
    for (uint32_t i = 0; i < count; i++) {
        current_scores[i] = evaluate(results[i]);
    }
    
    // Analyze trends if we have history
    if (history->history_size > 0 && history->quality_history) {
        SERIAL_LOG("  Analyzing trends with ");
        SERIAL_LOG_HEX("", history->history_size);
        SERIAL_LOG(" historical entries\n");
        
        for (uint32_t i = 0; i < count && i < history->history_size; i++) {
            int current = current_scores[i];
            int historical = history->quality_history[i];
            
            // Calculate trend: positive if improving, negative if degrading
            int trend = current - historical;
            
            SERIAL_LOG("    Result ");
            SERIAL_LOG_HEX("", i);
            SERIAL_LOG(": current=");
            SERIAL_LOG_DEC("", current);
            SERIAL_LOG(" historical=");
            SERIAL_LOG_DEC("", historical);
            SERIAL_LOG(" trend=");
            SERIAL_LOG_DEC("", trend);
            SERIAL_LOG("\n");
            
            // Compute temporal score: blend current quality with trend
            // temporal_score = (current * (100 - trend_weight) + trend * trend_weight) / 100
            temporal_scores[i] = 
                (current * (100 - history->trend_weight) + 
                 (current + trend) * history->trend_weight) / 100;
            
            SERIAL_LOG("      → temporal_score=");
            SERIAL_LOG_DEC("", temporal_scores[i]);
            SERIAL_LOG("\n");
        }
        
        // For results beyond history, use current score only
        for (uint32_t i = history->history_size; i < count; i++) {
            temporal_scores[i] = current_scores[i];
        }
    } else {
        // No history - temporal scores equal current scores
        SERIAL_LOG("  No history available, using current scores\n");
        for (uint32_t i = 0; i < count; i++) {
            temporal_scores[i] = current_scores[i];
        }
    }
    
    // Find best temporal score
    int best_score = temporal_scores[0];
    uint32_t best_idx = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        if (temporal_scores[i] > best_score) {
            best_score = temporal_scores[i];
            best_idx = i;
        }
    }
    
    SERIAL_LOG("TEMPORAL: Selected result ");
    SERIAL_LOG_HEX("", best_idx);
    SERIAL_LOG(" with temporal score=");
    SERIAL_LOG_DEC("", best_score);
    SERIAL_LOG(" (current=");
    SERIAL_LOG_DEC("", current_scores[best_idx]);
    SERIAL_LOG(")\n");
    
    memcpy(output, results[best_idx], size);
    
    // Update history with current scores for next time
    if (history->history_size < count) {
        history->history_size = count;
    }
    
    if (history->quality_history) {
        for (uint32_t i = 0; i < count; i++) {
            history->quality_history[i] = current_scores[i];
        }
    }
    
    heap_free(current_scores);
    heap_free(temporal_scores);
}

void qarma_quantum_set_temporal(QARMA_QUANTUM_REGISTER* reg,
                                uint32_t window_size, int trend_weight) {
    if (!reg) return;
    
    // Allocate temporal history if not already allocated
    if (!reg->temporal) {
        reg->temporal = (QARMA_TEMPORAL_HISTORY*)heap_alloc(sizeof(QARMA_TEMPORAL_HISTORY));
        if (!reg->temporal) return;
        memset(reg->temporal, 0, sizeof(QARMA_TEMPORAL_HISTORY));
    }
    
    reg->temporal->window_size = window_size;
    reg->temporal->trend_weight = trend_weight;
    
    // Allocate history arrays if not already allocated
    if (!reg->temporal->quality_history && window_size > 0) {
        reg->temporal->quality_history = (int*)heap_alloc(window_size * sizeof(int));
        if (reg->temporal->quality_history) {
            memset(reg->temporal->quality_history, 0, window_size * sizeof(int));
        }
    }
    
    if (!reg->temporal->timestamps && window_size > 0) {
        reg->temporal->timestamps = (uint64_t*)heap_alloc(window_size * sizeof(uint64_t));
        if (reg->temporal->timestamps) {
            memset(reg->temporal->timestamps, 0, window_size * sizeof(uint64_t));
        }
    }
}

void qarma_collapse_ensemble(void** results, uint32_t count, void* output, 
                            size_t size, QARMA_QUANTUM_REGISTER* reg) {
    if (count == 0 || !results || !output || !reg || !reg->ensemble) {
        if (count > 0 && results && output) {
            memcpy(output, results[0], size);
        }
        return;
    }
    
    SERIAL_LOG("COLLAPSE_ENSEMBLE: Combining ");
    SERIAL_LOG_HEX("", reg->ensemble->num_strategies);
    SERIAL_LOG(" strategies\n");
    
    // Vote array: tracks votes for each result
    int* votes = (int*)heap_alloc(count * sizeof(int));
    if (!votes) {
        memcpy(output, results[0], size);
        return;
    }
    memset(votes, 0, count * sizeof(int));
    
    // Run each strategy and collect votes
    for (uint32_t s = 0; s < reg->ensemble->num_strategies; s++) {
        QARMA_COLLAPSE_STRATEGY strategy = reg->ensemble->strategies[s];
        int weight = reg->ensemble->weights[s];
        
        SERIAL_LOG("  Strategy ");
        SERIAL_LOG_HEX("", strategy);
        SERIAL_LOG(" (weight=");
        SERIAL_LOG_DEC("", weight);
        SERIAL_LOG("): ");
        
        uint32_t selected_idx = 0;
        
        // Run the strategy to determine which result it selects
        switch (strategy) {
            case COLLAPSE_FIRST_WINS:
                selected_idx = 0;
                break;
                
            case COLLAPSE_LAST_WINS:
                selected_idx = count - 1;
                break;
                
            case COLLAPSE_BEST:
                if (reg->evaluate) {
                    int best_score = reg->evaluate(results[0]);
                    selected_idx = 0;
                    for (uint32_t i = 1; i < count; i++) {
                        int score = reg->evaluate(results[i]);
                        if (score > best_score) {
                            best_score = score;
                            selected_idx = i;
                        }
                    }
                }
                break;
                
            case COLLAPSE_FUZZY:
                // Simplified fuzzy: 70% best, 30% random weighted
                if (reg->evaluate) {
                    int best_score = reg->evaluate(results[0]);
                    selected_idx = 0;
                    for (uint32_t i = 1; i < count; i++) {
                        int score = reg->evaluate(results[i]);
                        if (score > best_score) {
                            best_score = score;
                            selected_idx = i;
                        }
                    }
                    // Simple probabilistic: use best_score as seed
                    uint32_t rand_val = (best_score * 1103515245 + 12345) % 100;
                    if (rand_val >= 70) {
                        // Pick weighted random
                        selected_idx = rand_val % count;
                    }
                }
                break;
                
            case COLLAPSE_MULTIDIM:
                // Use multidim if configured
                if (reg->multidim) {
                    int best_aggregate = -999999;
                    selected_idx = 0;
                    int total_weight = reg->multidim->quality_weight + 
                                      reg->multidim->speed_weight + 
                                      reg->multidim->resource_weight;
                    if (total_weight == 0) total_weight = 1;
                    
                    for (uint32_t i = 0; i < count; i++) {
                        int q = reg->multidim->quality_func ? reg->multidim->quality_func(results[i]) : 0;
                        int s = reg->multidim->speed_func ? reg->multidim->speed_func(results[i]) : 0;
                        int r = reg->multidim->resource_func ? reg->multidim->resource_func(results[i]) : 0;
                        int agg = (q * reg->multidim->quality_weight + 
                                  s * reg->multidim->speed_weight + 
                                  r * reg->multidim->resource_weight) / total_weight;
                        if (agg > best_aggregate) {
                            best_aggregate = agg;
                            selected_idx = i;
                        }
                    }
                }
                break;
                
            case COLLAPSE_TEMPORAL:
                // Use temporal if configured
                if (reg->temporal && reg->evaluate) {
                    int best_temporal = -999999;
                    selected_idx = 0;
                    for (uint32_t i = 0; i < count; i++) {
                        int current = reg->evaluate(results[i]);
                        int temporal_score = current;
                        
                        // Apply trend if history available
                        if (i < reg->temporal->history_size && reg->temporal->quality_history) {
                            int historical = reg->temporal->quality_history[i];
                            int trend = current - historical;
                            temporal_score = (current * (100 - reg->temporal->trend_weight) + 
                                            (current + trend) * reg->temporal->trend_weight) / 100;
                        }
                        
                        if (temporal_score > best_temporal) {
                            best_temporal = temporal_score;
                            selected_idx = i;
                        }
                    }
                }
                break;
                
            default:
                // Unknown strategy, use first result
                selected_idx = 0;
                break;
        }
        
        // Add weighted vote for selected result
        votes[selected_idx] += weight;
        
        SERIAL_LOG("voted for ");
        SERIAL_LOG_HEX("", selected_idx);
        SERIAL_LOG("\n");
    }
    
    // Find result with most votes
    int max_votes = votes[0];
    uint32_t winner_idx = 0;
    
    for (uint32_t i = 1; i < count; i++) {
        if (votes[i] > max_votes) {
            max_votes = votes[i];
            winner_idx = i;
        }
    }
    
    SERIAL_LOG("ENSEMBLE: Result ");
    SERIAL_LOG_HEX("", winner_idx);
    SERIAL_LOG(" won with ");
    SERIAL_LOG_DEC("", max_votes);
    SERIAL_LOG(" votes\n");
    
    memcpy(output, results[winner_idx], size);
    heap_free(votes);
}

void qarma_quantum_set_ensemble(QARMA_QUANTUM_REGISTER* reg,
                                QARMA_COLLAPSE_STRATEGY strategy1, int weight1,
                                QARMA_COLLAPSE_STRATEGY strategy2, int weight2,
                                QARMA_COLLAPSE_STRATEGY strategy3, int weight3) {
    if (!reg) return;
    
    // Allocate ensemble config if not already allocated
    if (!reg->ensemble) {
        reg->ensemble = (QARMA_ENSEMBLE_CONFIG*)heap_alloc(sizeof(QARMA_ENSEMBLE_CONFIG));
        if (!reg->ensemble) return;
        memset(reg->ensemble, 0, sizeof(QARMA_ENSEMBLE_CONFIG));
    }
    
    reg->ensemble->num_strategies = 0;
    
    if (strategy1 < COLLAPSE_STRATEGY_COUNT && weight1 > 0) {
        reg->ensemble->strategies[reg->ensemble->num_strategies] = strategy1;
        reg->ensemble->weights[reg->ensemble->num_strategies] = weight1;
        reg->ensemble->num_strategies++;
    }
    
    if (strategy2 < COLLAPSE_STRATEGY_COUNT && weight2 > 0) {
        reg->ensemble->strategies[reg->ensemble->num_strategies] = strategy2;
        reg->ensemble->weights[reg->ensemble->num_strategies] = weight2;
        reg->ensemble->num_strategies++;
    }
    
    if (strategy3 < COLLAPSE_STRATEGY_COUNT && weight3 > 0) {
        reg->ensemble->strategies[reg->ensemble->num_strategies] = strategy3;
        reg->ensemble->weights[reg->ensemble->num_strategies] = weight3;
        reg->ensemble->num_strategies++;
    }
}

// ============================================================================
// Statistics and Debugging
// ============================================================================

void qarma_quantum_get_stats(QARMA_QUANTUM_REGISTER* reg, qarma_quantum_stats_t* stats) {
    if (!reg || !stats) return;
    
    memset(stats, 0, sizeof(qarma_quantum_stats_t));
    
    stats->total_qubits = reg->count;
    stats->completed_qubits = reg->completed_count;
    stats->failed_qubits = reg->failed_count;
    stats->total_execution_time = reg->total_execution_time;
    stats->collapse_time = reg->collapse_time;
    
    // Count enabled qubits
    for (uint32_t i = 0; i < reg->count; i++) {
        if (reg->qubits[i].enabled) {
            stats->enabled_qubits++;
        }
    }
    
    // Calculate average (cast to avoid 64-bit division on 32-bit system)
    if (stats->completed_qubits > 0) {
        // Simple approximation to avoid __udivdi3
        uint32_t time_hi = (uint32_t)(stats->total_execution_time >> 32);
        uint32_t time_lo = (uint32_t)(stats->total_execution_time);
        if (time_hi == 0) {
            // Time fits in 32 bits, can do normal division
            stats->avg_qubit_time = time_lo / stats->completed_qubits;
        } else {
            // Approximate for large times
            stats->avg_qubit_time = 0xFFFFFFFF; // Max value
        }
    }
}

void qarma_quantum_debug_print(QARMA_QUANTUM_REGISTER* reg) {
    if (!reg) return;
    
    GFX_LOG("\n=== Quantum Register Debug ===\n");
    GFX_LOG("Total qubits: ");
    GFX_LOG_HEX("", reg->count);
    GFX_LOG("\n");
    
    GFX_LOG("Completed: ");
    GFX_LOG_HEX("", reg->completed_count);
    GFX_LOG(" Failed: ");
    GFX_LOG_HEX("", reg->failed_count);
    GFX_LOG("\n");
    
    GFX_LOG("Collapsed: ");
    GFX_LOG(reg->collapsed ? "Yes" : "No");
    GFX_LOG("\n");
    
    GFX_LOG("Strategy: ");
    switch (reg->strategy) {
        case COLLAPSE_FIRST_WINS: GFX_LOG("FIRST_WINS"); break;
        case COLLAPSE_LAST_WINS: GFX_LOG("LAST_WINS"); break;
        case COLLAPSE_BEST: GFX_LOG("BEST"); break;
        case COLLAPSE_VOTE: GFX_LOG("VOTE"); break;
        case COLLAPSE_COMBINE: GFX_LOG("COMBINE"); break;
        case COLLAPSE_VALIDATE: GFX_LOG("VALIDATE"); break;
        case COLLAPSE_CUSTOM: GFX_LOG("CUSTOM"); break;
        case COLLAPSE_FUZZY: GFX_LOG("FUZZY"); break;
        case COLLAPSE_PROGRESSIVE: GFX_LOG("PROGRESSIVE"); break;
        case COLLAPSE_SPECULATIVE: GFX_LOG("SPECULATIVE"); break;
        case COLLAPSE_MULTIDIM: GFX_LOG("MULTIDIM"); break;
        case COLLAPSE_TEMPORAL: GFX_LOG("TEMPORAL"); break;
        case COLLAPSE_ENSEMBLE: GFX_LOG("ENSEMBLE"); break;
        default: GFX_LOG("UNKNOWN"); break;
    }
    GFX_LOG("\n");
    
    GFX_LOG("\nQubit Status:\n");
    for (uint32_t i = 0; i < reg->count && i < 16; i++) {
        QARMA_QUBIT* q = &reg->qubits[i];
        GFX_LOG("  [");
        GFX_LOG_HEX("", i);
        GFX_LOG("] ");
        GFX_LOG(q->enabled ? "EN" : "DIS");
        GFX_LOG(" - ");
        
        switch (q->status) {
            case QUBIT_STATUS_PENDING: GFX_LOG("PENDING"); break;
            case QUBIT_STATUS_RUNNING: GFX_LOG("RUNNING"); break;
            case QUBIT_STATUS_COMPLETED: GFX_LOG("COMPLETED"); break;
            case QUBIT_STATUS_FAILED: GFX_LOG("FAILED"); break;
            case QUBIT_STATUS_SKIPPED: GFX_LOG("SKIPPED"); break;
            default: GFX_LOG("UNKNOWN"); break;
        }
        GFX_LOG("\n");
    }
    
    if (reg->count > 16) {
        GFX_LOG("  ... (");
        GFX_LOG_HEX("", reg->count - 16);
        GFX_LOG(" more qubits)\n");
    }
    
    GFX_LOG("==============================\n\n");
}
