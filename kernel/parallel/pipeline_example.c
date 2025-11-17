/**
 * QARMA - Pipeline Example
 * 
 * Demonstrates execution pipeline with a simple data processing chain.
 */

#include "execution_pipeline.h"
#include "graphics/graphics.h"

extern void* heap_alloc(size_t size);
extern void heap_free(void* ptr);

// ────────────────────────────────────────────────────────────────────────────
// Example Pipeline Functions
// ────────────────────────────────────────────────────────────────────────────

/**
 * Stage 1: Generate initial data
 */
void* pipeline_stage_generate(void* input) {
    (void)input;  // Ignore input for first stage
    
    gfx_print("    [Stage 1] Generating data...\n");
    
    // Simulate some data generation
    uint32_t* data = (uint32_t*)heap_alloc(sizeof(uint32_t));
    if (data) {
        *data = 0x1234;
        gfx_print("    [Stage 1] Generated: ");
        gfx_print_hex(*data);
        gfx_print("\n");
    }
    
    return data;
}

/**
 * Stage 2: Process data
 */
void* pipeline_stage_process(void* input) {
    if (!input) {
        gfx_print("    [Stage 2] ERROR: No input data\n");
        return NULL;
    }
    
    uint32_t* data = (uint32_t*)input;
    gfx_print("    [Stage 2] Processing: ");
    gfx_print_hex(*data);
    gfx_print("\n");
    
    // Transform data
    *data = *data * 2;
    
    gfx_print("    [Stage 2] Result: ");
    gfx_print_hex(*data);
    gfx_print("\n");
    
    return data;
}

/**
 * Stage 3: Validate and output
 */
void* pipeline_stage_output(void* input) {
    if (!input) {
        gfx_print("    [Stage 3] ERROR: No input data\n");
        return NULL;
    }
    
    uint32_t* data = (uint32_t*)input;
    gfx_print("    [Stage 3] Final value: ");
    gfx_print_hex(*data);
    gfx_print("\n");
    
    // Clean up
    heap_free(data);
    
    gfx_print("    [Stage 3] Pipeline complete!\n");
    return (void*)1;  // Success indicator
}

// ────────────────────────────────────────────────────────────────────────────
// Function Metadata Definitions
// ────────────────────────────────────────────────────────────────────────────

static glyph_function_t func_generate = {
    .semantic_name = "example.generate_data",
    .signature = SIG_PTR_TO_PTR,
    .func_ptr = pipeline_stage_generate,
    .version_id = 1,
    .estimated_cycles = 100,
    .is_resumable = false,
    .is_idempotent = true
};

static glyph_function_t func_process = {
    .semantic_name = "example.process_data",
    .signature = SIG_PTR_TO_PTR,
    .func_ptr = pipeline_stage_process,
    .version_id = 1,
    .estimated_cycles = 200,
    .is_resumable = false,
    .is_idempotent = false
};

static glyph_function_t func_output = {
    .semantic_name = "example.output_result",
    .signature = SIG_PTR_TO_PTR,
    .func_ptr = pipeline_stage_output,
    .version_id = 1,
    .estimated_cycles = 50,
    .is_resumable = false,
    .is_idempotent = false
};

// ────────────────────────────────────────────────────────────────────────────
// Test Function
// ────────────────────────────────────────────────────────────────────────────

void pipeline_example_test(void) {
    gfx_print("\n╔═══════════════════════════════════════╗\n");
    gfx_print("║   Execution Pipeline Example Test    ║\n");
    gfx_print("╚═══════════════════════════════════════╝\n\n");
    
    // Find a free core
    uint32_t core_id = find_free_core();
    gfx_print("Assigned to core: ");
    gfx_print_hex(core_id);
    gfx_print("\n\n");
    
    // Create pipeline
    execution_pipeline_t* pipeline = pipeline_create(core_id);
    if (!pipeline) {
        gfx_print("ERROR: Failed to create pipeline\n");
        return;
    }
    
    // Add stages to pipeline
    gfx_print("Building pipeline...\n");
    pipeline_add_node(pipeline, &func_generate);
    pipeline_add_node(pipeline, &func_process);
    pipeline_add_node(pipeline, &func_output);
    gfx_print("Pipeline built with 3 stages\n\n");
    
    // Assign to core
    if (!assign_pipeline_to_core(pipeline, core_id)) {
        gfx_print("ERROR: Failed to assign pipeline to core\n");
        pipeline_destroy(pipeline);
        return;
    }
    
    // Execute pipeline
    gfx_print("Executing pipeline...\n\n");
    pipeline_execute(pipeline);
    
    // Print results
    gfx_print("\n");
    pipeline_print_status(pipeline);
    pipeline_print_metrics(pipeline);
    
    // Cleanup
    gfx_print("\nCleaning up...\n");
    pipeline_destroy(pipeline);
    
    gfx_print("\n╔═══════════════════════════════════════╗\n");
    gfx_print("║        Test Complete!                 ║\n");
    gfx_print("╚═══════════════════════════════════════╝\n\n");
}
