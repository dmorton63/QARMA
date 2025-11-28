/**
 * QARMA - Quantum Kernel Implementation
 * 
 * Implementation of quantum process management with superposition,
 * entanglement, and quantum error correction.
 */

#include "quantum_kernel.h"
#include "quantum_register.h"
#include "quantum_ai_observer.h"
#include "quantum_scheduler.h"
//#include "core/kernel.h"
#include "core_manager.h"
#include "graphics.h"
#include "config.h"
#include "clock_overlay.h"
#include "memory.h"
#include "memory/heap.h"
#include "qarma_window_manager.h"


// Global quantum system state
static quantum_process_t* g_quantum_processes = NULL;
static quantum_process_t* g_current_quantum_process = NULL;
static quantum_scheduler_stats_t g_quantum_stats = {0};
static uint32_t g_next_qpid = 1;
static quantum_entanglement_t* g_entanglements __attribute__((unused)) = NULL;

// Quantum hardware state
static bool g_quantum_hardware_available = false;
static uint32_t g_qubit_count = 0;

// Quantum processing control
static bool g_quantum_enabled = true;  // Toggle for quantum processing (enabled by default)
static bool g_quantum_initialized = false;

/*
    System entry point.  Switch from kernel_main() to quantum_kernel_main().
*/

void quantum_kernel_main(uint64_t magic, uint64_t mbi_addr) {
    // Debug marker J - entered quantum_kernel_main
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'J', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // Debug marker 1 - still alive
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'1', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // // Try to output something immediately to see if we get this far
    // // Use direct VGA text mode as a fallback
    // volatile char* vga_buffer = (volatile char*)0xB8000;
    // const char* msg = "BOOT: quantum_kernel_main started";
    // for (int i = 0; msg[i] != '\0'; i++) {
    //     vga_buffer[i * 2] = msg[i];
    //     vga_buffer[i * 2 + 1] = 0x07; // White on black
    // }
    
    // Debug marker K - after VGA write
    // __asm__ volatile (
    //     "mov $0x3F8, %%dx\n"
    //     "mov $'K', %%al\n"
    //     "out %%al, %%dx\n"
    //     ::: "rax", "rdx"
    // );
    
    // Debug marker 2 - before cast
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'2', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // Convert to proper types
    multiboot_info_t* mbi = (multiboot_info_t*)mbi_addr;
    
    // Debug marker 3 - after cast
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'3', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // Debug marker L - before kernel_main
    __asm__ volatile (
        "mov $0x3F8, %%dx\n"
        "mov $'L', %%al\n"
        "out %%al, %%dx\n"
        ::: "rax", "rdx"
    );
    
    // Call the original kernel main
    kernel_main((uint32_t)magic, mbi);
    
    // Debug marker M - after kernel_main
    // __asm__ volatile (
    //     "mov $0x3F8, %%dx\n"
    //     "mov $'M', %%al\n"
    //     "out %%al, %%dx\n"
    //     ::: "rax", "rdx"
    // );
    
    // Initialize quantum processing (can be toggled at runtime)
    quantum_kernel_init();           // Initialize quantum core
    quantum_ai_init();               // Initialize AI observer
    quantum_scheduler_init();        // Prepare scheduler
    
    SERIAL_LOG("[QUANTUM] Quantum subsystem initialized (disabled by default)\n");
    SERIAL_LOG("[QUANTUM] Use 'quantum on' command to enable\n");

    // Optionally spawn initial quantum process
    // quantum_process_create("init", 0);
    // qarma_window_manager_init();
    // clock_overlay_init();          // Initialize clock overlay
    // reset_clock();               // Reset clock to 00:00:00
    
    // Don't enter the quantum loop yet
    // while (true) {
    //     quantum_scheduler_tick();
    // }
}

/**
 * Initialize quantum kernel subsystem
 */
void quantum_kernel_init(void) {
    if (g_quantum_initialized) {
        return;  // Already initialized
    }
    
    SERIAL_LOG("[QUANTUM] Initializing quantum kernel...\n");
    
    // Initialize quantum hardware
    quantum_hardware_init();
    
    // Initialize quantum error correction
    quantum_error_correction_init();
    
    // Reset statistics
    memset(&g_quantum_stats, 0, sizeof(quantum_scheduler_stats_t));
    
    g_quantum_initialized = true;
    SERIAL_LOG("[QUANTUM] Quantum kernel initialized (status: ");
    SERIAL_LOG(g_quantum_enabled ? "ENABLED" : "DISABLED");
    SERIAL_LOG(")\n");
}

/**
 * Initialize quantum hardware drivers
 */
void quantum_drivers_init(void) {
    GFX_LOG_MIN("Loading quantum drivers...\n");
    
    // Detect quantum processing units
    quantum_hardware_init();
    
    if (g_quantum_hardware_available) {
        GFX_LOG_HEX("Quantum hardware detected: ", g_qubit_count);
        GFX_LOG(" qubits available.\n");
    } else {
        GFX_LOG("No quantum hardware detected. Using simulation mode.\n");
    }
}

/**
 * Create a new quantum process
 */
quantum_process_t* quantum_process_create(const char* name, uint32_t parent_qpid) {
    GFX_LOG("Creating quantum process: ");
    GFX_LOG(name ? name : "(null)");
    GFX_LOG("\n");
    
    // Validate input
    if (!name) {
        GFX_LOG_MIN("Error: null process name\n");
        return NULL;
    }
    
    // Allocate memory for new quantum process
    quantum_process_t* process = (quantum_process_t*)heap_alloc(sizeof(quantum_process_t));
    if (!process) {
        GFX_LOG_MIN("Error: failed to allocate memory for quantum process\n");
        return NULL;
    }
    
    // Initialize quantum process
    memset(process, 0, sizeof(quantum_process_t));
    
    process->qpid = g_next_qpid++;
    process->parent_qpid = parent_qpid;
    
    // Copy process name
    size_t name_len = strlen(name);
    if (name_len >= sizeof(process->name)) {
        name_len = sizeof(process->name) - 1;
    }
    memcpy(process->name, name, name_len);
    process->name[name_len] = '\0';
    
    // Set initial quantum state
    process->quantum_state = QUANTUM_SUPERPOSED;  // Start in superposition
    process->priority = QUANTUM_PRIORITY_NORMAL;
    process->coherence_time = 1000;  // 1000 quantum units
    process->measurement_count = 0;
    
    // Initialize classical process info
    process->pid = process->qpid;  // For now, same as quantum PID
    process->quantum_time_slice = 100;  // 100 quantum units
    process->quantum_remaining = process->quantum_time_slice;
    
    // Add to process list
    process->next = g_quantum_processes;
    if (g_quantum_processes) {
        g_quantum_processes->prev = process;
    }
    g_quantum_processes = process;
    
    // Update statistics
    g_quantum_stats.total_processes++;
    g_quantum_stats.superposed_processes++;
    
    return process;
}

/**
 * Set quantum process state
 */
void quantum_process_set_state(quantum_process_t* process, quantum_state_t state) {
    if (!process) return;
    
    quantum_state_t old_state = process->quantum_state;
    process->quantum_state = state;
    
    // Update statistics based on state change
    if (old_state == QUANTUM_SUPERPOSED && state != QUANTUM_SUPERPOSED) {
        g_quantum_stats.superposed_processes--;
    } else if (old_state != QUANTUM_SUPERPOSED && state == QUANTUM_SUPERPOSED) {
        g_quantum_stats.superposed_processes++;
    }
    
    if (state == QUANTUM_RUNNING) {
        g_quantum_stats.running_processes++;
    } else if (old_state == QUANTUM_RUNNING) {
        g_quantum_stats.running_processes--;
    }
}

/**
 * Measure quantum process state (causes collapse)
 */
quantum_state_t quantum_process_measure_state(quantum_process_t* process) {
    if (!process) return QUANTUM_COLLAPSED;
    
    process->measurement_count++;
    
    // If process is in superposition, collapse to definite state
    if (process->quantum_state == QUANTUM_SUPERPOSED) {
        // Simple collapse algorithm - in real implementation would use quantum mechanics
        quantum_state_t collapsed_state;
        uint32_t random = process->measurement_count % 3;
        
        switch (random) {
            case 0: collapsed_state = QUANTUM_RUNNING; break;
            case 1: collapsed_state = QUANTUM_WAITING; break;
            default: collapsed_state = QUANTUM_SUSPENDED; break;
        }
        
        quantum_collapse_state(process, collapsed_state);
    }
    
    return process->quantum_state;
}

/**
 * Initialize quantum process scheduler (different from quantum_scheduler.c)
 */
void quantum_process_scheduler_init(void) {
    GFX_LOG_MIN("Initializing quantum process scheduler...\n");
    
    g_current_quantum_process = NULL;
    
    GFX_LOG_MIN("Quantum process scheduler ready.\n");
}

/**
 * Quantum scheduler tick - called from main kernel loop
 */
void quantum_scheduler_tick(void) {
    g_quantum_stats.total_quantum_cycles++;
    
    // Check for decoherence in all processes
    quantum_process_t* process = g_quantum_processes;
    while (process) {
        if (!quantum_check_coherence(process)) {
            quantum_restore_coherence(process);
            g_quantum_stats.decoherence_events++;
        }
        process = process->next;
    }
    
    // Select next process to run
    quantum_process_t* next_process = quantum_scheduler_select_next();
    
    if (next_process && next_process != g_current_quantum_process) {
        // Context switch to new quantum process
        g_current_quantum_process = next_process;
        quantum_process_set_state(next_process, QUANTUM_RUNNING);
    }
    
    // Decrease quantum time remaining for current process
    if (g_current_quantum_process) {
        g_current_quantum_process->quantum_remaining--;
        if (g_current_quantum_process->quantum_remaining == 0) {
            // Quantum time slice expired, enter superposition
            quantum_enter_superposition(g_current_quantum_process);
            g_current_quantum_process->quantum_remaining = g_current_quantum_process->quantum_time_slice;
        }
    }
}

/**
 * Select next quantum process to run
 */
quantum_process_t* quantum_scheduler_select_next(void) {
    // Simple round-robin for now - real implementation would use quantum algorithms
    quantum_process_t* process = g_quantum_processes;
    
    while (process) {
        if (process->quantum_state == QUANTUM_RUNNING || 
            process->quantum_state == QUANTUM_SUPERPOSED) {
            return process;
        }
        process = process->next;
    }
    
    return NULL;
}

/**
 * Enter quantum superposition
 */
void quantum_enter_superposition(quantum_process_t* process) {
    if (!process) return;
    
    quantum_process_set_state(process, QUANTUM_SUPERPOSED);
    process->coherence_time = 1000;  // Reset coherence time
}

/**
 * Collapse quantum state to definite state
 */
void quantum_collapse_state(quantum_process_t* process, quantum_state_t final_state) {
    if (!process) return;
    
    quantum_process_set_state(process, final_state);
    process->measurement_count++;
}

/**
 * Check if process is in superposition
 */
bool quantum_is_superposed(quantum_process_t* process) {
    return process && (process->quantum_state == QUANTUM_SUPERPOSED);
}

/**
 * Initialize quantum error correction
 */
void quantum_error_correction_init(void) {
    GFX_LOG_MIN("Quantum error correction initialized.\n");
}

/**
 * Check quantum coherence of process
 */
bool quantum_check_coherence(quantum_process_t* process) {
    if (!process) return false;
    
    // Decrease coherence time
    if (process->coherence_time > 0) {
        process->coherence_time--;
    }
    
    return process->coherence_time > 0;
}

/**
 * Restore quantum coherence
 */
void quantum_restore_coherence(quantum_process_t* process) {
    if (!process) return;
    
    process->coherence_time = 1000;  // Restore full coherence
    
    // If process was collapsed, return to superposition
    if (process->quantum_state == QUANTUM_COLLAPSED) {
        quantum_enter_superposition(process);
    }
}

/**
 * Get quantum scheduler statistics
 */
quantum_scheduler_stats_t* quantum_get_scheduler_stats(void) {
    return &g_quantum_stats;
}

/**
 * Enable quantum processing
 */
void quantum_enable(void) {
    if (!g_quantum_initialized) {
        quantum_kernel_init();
    }
    g_quantum_enabled = true;
    SERIAL_LOG("[QUANTUM] Quantum processing ENABLED\n");
}

/**
 * Disable quantum processing
 */
void quantum_disable(void) {
    g_quantum_enabled = false;
    SERIAL_LOG("[QUANTUM] Quantum processing DISABLED\n");
}

/**
 * Check if quantum processing is enabled
 */
bool quantum_is_enabled(void) {
    return g_quantum_enabled;
}

/**
 * Get quantum status string
 */
const char* quantum_get_status(void) {
    if (!g_quantum_initialized) {
        return "NOT INITIALIZED";
    }
    return g_quantum_enabled ? "ENABLED" : "DISABLED";
}

// Core management integration
bool quantum_request_cores(uint32_t count) {
    core_request_t request = {0};
    request.subsystem = SUBSYSTEM_QUANTUM;
    request.core_count = count;
    request.preferred_numa = 1; // Prefer NUMA node 1
    request.flags = 0x04; // CORE_ALLOC_SHARED
    
    core_response_t response = core_request_allocate(&request);
    return response.success;
}

void quantum_release_cores(void) {
    core_release_all(SUBSYSTEM_QUANTUM);
}

uint32_t quantum_get_allocated_cores(void) {
    return core_get_allocated_count(SUBSYSTEM_QUANTUM);
}

bool quantum_run_on_dedicated_core(void (*function)(void*), void* data) {
    return core_pin_task_subsystem(SUBSYSTEM_QUANTUM, function, data);
}

/**
 * Initialize quantum hardware
 */
void quantum_hardware_init(void) {
    // Simulate quantum hardware detection
    g_quantum_hardware_available = true;  // Assume quantum hardware available
    g_qubit_count = 64;  // Simulate 64-qubit quantum processor
}

/**
 * Check if quantum hardware is available
 */
bool quantum_hardware_available(void) {
    return g_quantum_hardware_available;
}

/**
 * Get number of available qubits
 */
uint32_t quantum_get_qubit_count(void) {
    return g_qubit_count;
}

