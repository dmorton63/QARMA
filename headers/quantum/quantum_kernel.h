/**
 * QARMA - Quantum Process Management
 * 
 * This module implements quantum computing principles in process management,
 * including quantum superposition for scheduling and quantum entanglement 
 * for inter-process communication.
 */

#ifndef QUANTUM_KERNEL_H
#define QUANTUM_KERNEL_H

#include "kernel_types.h"
#include "core/kernel.h"

// Quantum process states using superposition principle
typedef enum {
    QUANTUM_RUNNING     = 0x01,  // Process is actively running
    QUANTUM_WAITING     = 0x02,  // Process is waiting for resources
    QUANTUM_SUSPENDED   = 0x04,  // Process is suspended
    QUANTUM_SUPERPOSED  = 0x07,  // Process exists in multiple states (superposition)
    QUANTUM_ENTANGLED   = 0x08,  // Process is quantum entangled with another
    QUANTUM_COLLAPSED   = 0x10   // Process state has collapsed to classical state
} quantum_state_t;

// Quantum process priority levels
typedef enum {
    QUANTUM_PRIORITY_CRITICAL = 0,  // Quantum error correction processes
    QUANTUM_PRIORITY_HIGH     = 1,  // Real-time quantum computations
    QUANTUM_PRIORITY_NORMAL   = 2,  // Standard quantum processes
    QUANTUM_PRIORITY_LOW      = 3,  // Background quantum tasks
    QUANTUM_PRIORITY_IDLE     = 4   // Idle quantum processes
} quantum_priority_t;

// Quantum entanglement pair structure
typedef struct quantum_entanglement {
    uint32_t qpid_a;                    // First entangled process ID
    uint32_t qpid_b;                    // Second entangled process ID
    uint64_t entanglement_key;          // Quantum encryption key
    bool active;                        // Entanglement active flag
    struct quantum_entanglement* next;  // Next in entanglement list
} quantum_entanglement_t;

// Quantum process control block
typedef struct quantum_process {
    uint32_t qpid;                      // Quantum Process ID
    char name[32];                      // Process name
    
    // Quantum state information
    quantum_state_t quantum_state;      // Current quantum state
    quantum_priority_t priority;        // Quantum priority level
    uint32_t coherence_time;            // Time before decoherence
    uint32_t measurement_count;         // Number of state measurements
    
    // Classical process information
    uint32_t pid;                       // Classical process ID
    uint32_t parent_qpid;               // Parent quantum process
    void* memory_space;                 // Virtual memory space
    uint32_t stack_pointer;             // Stack pointer
    uint32_t instruction_pointer;       // Instruction pointer
    
    // Quantum scheduling
    uint32_t quantum_time_slice;        // Time slice in quantum units
    uint32_t quantum_remaining;         // Remaining quantum time
    uint64_t total_quantum_time;        // Total quantum execution time
    
    // Entanglement information
    quantum_entanglement_t* entanglements;  // List of quantum entanglements
    
    // Process tree
    struct quantum_process* parent;     // Parent process
    struct quantum_process* children;   // Child processes
    struct quantum_process* sibling;    // Sibling process
    
    // Scheduler queue pointers
    struct quantum_process* next;       // Next in scheduler queue
    struct quantum_process* prev;       // Previous in scheduler queue
    
} quantum_process_t;

// Quantum scheduler statistics
typedef struct {
    uint32_t total_processes;           // Total quantum processes
    uint32_t running_processes;         // Currently running processes
    uint32_t superposed_processes;      // Processes in superposition
    uint32_t entangled_pairs;           // Number of entangled pairs
    uint64_t total_quantum_cycles;      // Total quantum execution cycles
    uint32_t decoherence_events;        // Number of decoherence events
} quantum_scheduler_stats_t;

// Function declarations

// Quantum kernel initialization
void quantum_kernel_init(void);
void quantum_drivers_init(void);

// Quantum process management
quantum_process_t* quantum_process_create(const char* name, uint32_t parent_qpid);
void quantum_process_destroy(quantum_process_t* process);
void quantum_kernel_main(uint32_t magic, multiboot_info_t *mbi);
void quantum_process_set_state(quantum_process_t *process, quantum_state_t state);
quantum_state_t quantum_process_measure_state(quantum_process_t* process);

// Quantum scheduling
void quantum_scheduler_init(void);
void quantum_scheduler_tick(void);
void quantum_scheduler_add_process(quantum_process_t* process);
void quantum_scheduler_remove_process(quantum_process_t* process);
quantum_process_t* quantum_scheduler_select_next(void);

// Quantum superposition
void quantum_enter_superposition(quantum_process_t* process);
void quantum_collapse_state(quantum_process_t* process, quantum_state_t final_state);
bool quantum_is_superposed(quantum_process_t* process);

// Quantum entanglement
quantum_entanglement_t* quantum_entangle_processes(uint32_t qpid_a, uint32_t qpid_b);
void quantum_disentangle_processes(quantum_entanglement_t* entanglement);
void quantum_entangled_communication(quantum_entanglement_t* entanglement, 
                                    const void* data, size_t size);

// Quantum error correction
void quantum_error_correction_init(void);
bool quantum_check_coherence(quantum_process_t* process);
void quantum_restore_coherence(quantum_process_t* process);

// Quantum statistics and monitoring
quantum_scheduler_stats_t* quantum_get_scheduler_stats(void);
void quantum_print_process_info(quantum_process_t* process);
void quantum_debug_dump_scheduler(void);

// Quantum hardware abstraction
void quantum_hardware_init(void);
bool quantum_hardware_available(void);
uint32_t quantum_get_qubit_count(void);
void quantum_execute_gate(uint32_t qubit, uint32_t gate_type);

// Core management integration
bool quantum_request_cores(uint32_t count);
void quantum_release_cores(void);
uint32_t quantum_get_allocated_cores(void);
bool quantum_run_on_dedicated_core(void (*function)(void*), void* data);

#endif // QUANTUM_KERNEL_H