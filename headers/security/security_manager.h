/**
 * QARMA - Security Manager Header
 * 
 * Dynamic function relocation and ASLR++ security features
 */

#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include "kernel_types.h"
#include "core/kernel.h"

// Security levels
typedef enum {
    SECURITY_LEVEL_MINIMAL  = 0,  // Basic protection
    SECURITY_LEVEL_STANDARD = 1,  // Standard security
    SECURITY_LEVEL_HIGH     = 2,  // High security  
    SECURITY_LEVEL_MAXIMUM  = 3,  // Maximum security
    SECURITY_LEVEL_QUANTUM  = 4   // Quantum-enhanced security
} security_level_t;

// Function relocation entry
typedef struct relocation_entry {
    void* original_address;      // Original function address
    void* relocated_address;     // New relocated address
    uint32_t function_size;      // Size of function
    uint32_t permissions;        // Access permissions
    uint64_t quantum_key;        // Quantum encryption key
    
    struct relocation_entry* next;
} relocation_entry_t;

// Memory mapping information
typedef struct memory_mapping {
    uint64_t virtual_base;       // Virtual base address
    uint64_t physical_base;      // Physical base address
    uint32_t size;               // Mapping size
    uint32_t permissions;        // Access permissions
    bool randomized;             // Address randomized
    uint32_t entropy_bits;       // Randomization entropy
    
    struct memory_mapping* next;
} memory_mapping_t;

// Security statistics
typedef struct {
    uint32_t functions_relocated;    // Functions relocated
    uint32_t memory_mappings;        // Active memory mappings
    uint32_t exploit_attempts;       // Exploit attempts blocked
    uint32_t quantum_encryptions;    // Quantum encryptions performed
    uint64_t entropy_generated;      // Entropy bits generated
} security_stats_t;

// Function declarations
void security_manager_init(void);
void security_monitor_tick(void);

// Memory mapping functions
void security_memory_init(void);
memory_mapping_t* create_secure_mapping(uint64_t virtual_addr, uint64_t physical_addr, 
                                       uint32_t size, uint32_t permissions);
bool randomize_memory_layout(void);

// Function relocation
relocation_entry_t* relocate_function(void* original_func, uint32_t size);
void* get_relocated_address(void* original_func);
bool verify_function_integrity(relocation_entry_t* entry);

// Statistics
security_stats_t* security_get_stats(void);

// Core management integration
bool security_request_cores(uint32_t count);
void security_release_cores(void);
uint32_t security_get_allocated_cores(void);
bool security_run_on_dedicated_core(void (*function)(void*), void* data);

#endif // SECURITY_MANAGER_H