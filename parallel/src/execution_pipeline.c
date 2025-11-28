/**
 * QARMA - Execution Pipeline Implementation
 * 
 * Core-local execution chains with zero synchronization overhead.
 */

#include "execution_pipeline.h"
#include "graphics/graphics.h"
#include "core/string.h"

extern void* heap_alloc(size_t size);
extern void heap_free(void* ptr);

// ────────────────────────────────────────────────────────────────────────────
// Global State
// ────────────────────────────────────────────────────────────────────────────

static core_pipeline_manager_t core_managers[4];  // Support up to 4 cores
static uint32_t num_cores = 0;
static uint32_t next_pipeline_id = 1;

// ────────────────────────────────────────────────────────────────────────────
// System Initialization
// ────────────────────────────────────────────────────────────────────────────

void pipeline_system_init(void) {
    extern uint32_t get_cpu_core_count(void);
    num_cores = get_cpu_core_count();
    
    if (num_cores > 4) num_cores = 4;  // Limit to 4 for now
    
    gfx_print("Initializing execution pipeline system...\n");
    gfx_print("Cores detected: ");
    gfx_print_hex(num_cores);
    gfx_print("\n");
    
    // Initialize each core's pipeline manager
    for (uint32_t i = 0; i < num_cores; i++) {
        core_managers[i].core_id = i;
        core_managers[i].active_pipeline_count = 0;
        core_managers[i].core_busy = false;
        core_managers[i].total_pipelines_executed = 0;
        core_managers[i].total_cycles_used = 0;
        
        for (uint32_t j = 0; j < MAX_PIPELINES_PER_CORE; j++) {
            core_managers[i].pipelines[j] = NULL;
        }
    }
    
    gfx_print("Pipeline system initialized\n");
}

// ────────────────────────────────────────────────────────────────────────────
// Core Management
// ────────────────────────────────────────────────────────────────────────────

core_pipeline_manager_t* get_core_pipeline_manager(uint32_t core_id) {
    if (core_id >= num_cores) return NULL;
    return &core_managers[core_id];
}

uint32_t find_free_core(void) {
    // Find core with lowest utilization
    uint32_t best_core = 0;
    uint32_t lowest_count = core_managers[0].active_pipeline_count;
    
    for (uint32_t i = 1; i < num_cores; i++) {
        if (core_managers[i].active_pipeline_count < lowest_count) {
            best_core = i;
            lowest_count = core_managers[i].active_pipeline_count;
        }
    }
    
    return best_core;
}

bool assign_pipeline_to_core(execution_pipeline_t* pipeline, uint32_t core_id) {
    if (core_id >= num_cores) return false;
    
    core_pipeline_manager_t* mgr = &core_managers[core_id];
    
    // Find empty slot
    for (uint32_t i = 0; i < MAX_PIPELINES_PER_CORE; i++) {
        if (mgr->pipelines[i] == NULL) {
            mgr->pipelines[i] = pipeline;
            mgr->active_pipeline_count++;
            pipeline->core_id = core_id;
            return true;
        }
    }
    
    return false;  // No free slots
}

// ────────────────────────────────────────────────────────────────────────────
// Node Management
// ────────────────────────────────────────────────────────────────────────────

execution_node_t* node_create(glyph_function_t* func) {
    execution_node_t* node = (execution_node_t*)heap_alloc(sizeof(execution_node_t));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(execution_node_t));
    node->function = func;
    node->completed = false;
    node->result_code = 0;
    
    return node;
}

void node_destroy(execution_node_t* node) {
    if (node) {
        heap_free(node);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Pipeline Management
// ────────────────────────────────────────────────────────────────────────────

execution_pipeline_t* pipeline_create(uint32_t core_id) {
    execution_pipeline_t* pipeline = (execution_pipeline_t*)heap_alloc(sizeof(execution_pipeline_t));
    if (!pipeline) return NULL;
    
    memset(pipeline, 0, sizeof(execution_pipeline_t));
    
    pipeline->pipeline_id = next_pipeline_id++;
    pipeline->core_id = core_id;
    pipeline->head = NULL;
    pipeline->tail = NULL;
    pipeline->current = NULL;
    pipeline->node_count = 0;
    pipeline->is_running = false;
    pipeline->is_complete = false;
    pipeline->has_error = false;
    pipeline->total_cycles = 0;
    
    return pipeline;
}

void pipeline_destroy(execution_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    // Free all nodes
    execution_node_t* node = pipeline->head;
    while (node) {
        execution_node_t* next = node->next;
        node_destroy(node);
        node = next;
    }
    
    // Remove from core manager
    core_pipeline_manager_t* mgr = get_core_pipeline_manager(pipeline->core_id);
    if (mgr) {
        for (uint32_t i = 0; i < MAX_PIPELINES_PER_CORE; i++) {
            if (mgr->pipelines[i] == pipeline) {
                mgr->pipelines[i] = NULL;
                mgr->active_pipeline_count--;
                break;
            }
        }
    }
    
    heap_free(pipeline);
}

bool pipeline_add_node(execution_pipeline_t* pipeline, glyph_function_t* func) {
    if (!pipeline || !func) return false;
    if (pipeline->node_count >= MAX_PIPELINE_NODES) return false;
    
    execution_node_t* node = node_create(func);
    if (!node) return false;
    
    // Add to end of chain
    if (pipeline->tail) {
        pipeline->tail->next = node;
        pipeline->tail = node;
    } else {
        // First node
        pipeline->head = node;
        pipeline->tail = node;
        pipeline->current = node;
    }
    
    pipeline->node_count++;
    return true;
}

// ────────────────────────────────────────────────────────────────────────────
// Pipeline Execution
// ────────────────────────────────────────────────────────────────────────────

void pipeline_execute(execution_pipeline_t* pipeline) {
    if (!pipeline || pipeline->is_running) return;
    
    pipeline->is_running = true;
    pipeline->is_complete = false;
    pipeline->has_error = false;
    
    // TODO: Get actual cycle counter
    uint64_t start_cycles = 0;  // rdtsc() on x86
    
    execution_node_t* node = pipeline->head;
    void* data = NULL;  // Initial input
    
    gfx_print("[Pipeline ");
    gfx_print_hex(pipeline->pipeline_id);
    gfx_print("] Starting execution on core ");
    gfx_print_hex(pipeline->core_id);
    gfx_print("\n");
    
    while (node && !pipeline->has_error) {
        // Execute this node's function
        node->start_cycles = 0;  // rdtsc()
        
        gfx_print("  [Node] Executing: ");
        gfx_print(node->function->semantic_name);
        gfx_print("\n");
        
        // Call function with previous node's output
        data = node->function->func_ptr(data);
        
        node->end_cycles = 0;  // rdtsc()
        node->output_data = data;
        node->completed = true;
        
        // Check for errors (NULL return typically indicates error)
        if (data == NULL && node->function->signature == SIG_PTR_TO_PTR) {
            pipeline->has_error = true;
            node->result_code = -1;
            gfx_print("  [Node] ERROR: Function returned NULL\n");
            break;
        }
        
        // Move to next node
        pipeline->current = node->next;
        node = node->next;
    }
    
    uint64_t end_cycles = 0;  // rdtsc()
    pipeline->total_cycles = end_cycles - start_cycles;
    
    pipeline->is_running = false;
    pipeline->is_complete = !pipeline->has_error;
    
    if (pipeline->is_complete) {
        gfx_print("[Pipeline ");
        gfx_print_hex(pipeline->pipeline_id);
        gfx_print("] Completed successfully\n");
    } else {
        gfx_print("[Pipeline ");
        gfx_print_hex(pipeline->pipeline_id);
        gfx_print("] Failed with error\n");
    }
    
    // Update core manager stats
    core_pipeline_manager_t* mgr = get_core_pipeline_manager(pipeline->core_id);
    if (mgr) {
        mgr->total_pipelines_executed++;
        mgr->total_cycles_used += pipeline->total_cycles;
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Checkpoint/Resume
// ────────────────────────────────────────────────────────────────────────────

pipeline_checkpoint_t* pipeline_checkpoint(execution_pipeline_t* pipeline) {
    if (!pipeline) return NULL;
    
    pipeline_checkpoint_t* checkpoint = (pipeline_checkpoint_t*)heap_alloc(sizeof(pipeline_checkpoint_t));
    if (!checkpoint) return NULL;
    
    memset(checkpoint, 0, sizeof(pipeline_checkpoint_t));
    
    checkpoint->pipeline_id = pipeline->pipeline_id;
    checkpoint->original_core_id = pipeline->core_id;
    
    // Find current node index
    uint32_t index = 0;
    execution_node_t* node = pipeline->head;
    while (node && node != pipeline->current) {
        index++;
        node = node->next;
    }
    checkpoint->current_node_index = index;
    
    // Save intermediate data
    if (pipeline->current && pipeline->current->input_data) {
        checkpoint->intermediate_data = pipeline->current->input_data;
    }
    
    checkpoint->checkpoint_timestamp = 0;  // TODO: get system time
    
    gfx_print("[Checkpoint] Saved pipeline ");
    gfx_print_hex(pipeline->pipeline_id);
    gfx_print(" at node ");
    gfx_print_hex(index);
    gfx_print("\n");
    
    return checkpoint;
}

bool pipeline_resume(pipeline_checkpoint_t* checkpoint, uint32_t new_core_id) {
    // TODO: Implement resume functionality
    // This requires reconstructing the pipeline and jumping to the saved node
    (void)checkpoint;
    (void)new_core_id;
    
    gfx_print("[Resume] Not yet implemented\n");
    return false;
}

void checkpoint_destroy(pipeline_checkpoint_t* checkpoint) {
    if (checkpoint) {
        heap_free(checkpoint);
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Debugging and Introspection
// ────────────────────────────────────────────────────────────────────────────

void pipeline_print_status(execution_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    gfx_print("\n=== Pipeline Status ===\n");
    gfx_print("ID: ");
    gfx_print_hex(pipeline->pipeline_id);
    gfx_print("\n");
    
    gfx_print("Core: ");
    gfx_print_hex(pipeline->core_id);
    gfx_print("\n");
    
    gfx_print("Nodes: ");
    gfx_print_hex(pipeline->node_count);
    gfx_print("\n");
    
    gfx_print("Running: ");
    gfx_print(pipeline->is_running ? "Yes" : "No");
    gfx_print("\n");
    
    gfx_print("Complete: ");
    gfx_print(pipeline->is_complete ? "Yes" : "No");
    gfx_print("\n");
    
    gfx_print("Error: ");
    gfx_print(pipeline->has_error ? "Yes" : "No");
    gfx_print("\n");
}

void pipeline_print_metrics(execution_pipeline_t* pipeline) {
    if (!pipeline) return;
    
    gfx_print("\n=== Pipeline Metrics ===\n");
    gfx_print("Total cycles: ");
    gfx_print_hex((uint32_t)pipeline->total_cycles);
    gfx_print("\n");
    
    gfx_print("Nodes executed: ");
    uint32_t completed = 0;
    execution_node_t* node = pipeline->head;
    while (node) {
        if (node->completed) completed++;
        node = node->next;
    }
    gfx_print_hex(completed);
    gfx_print(" / ");
    gfx_print_hex(pipeline->node_count);
    gfx_print("\n");
}
