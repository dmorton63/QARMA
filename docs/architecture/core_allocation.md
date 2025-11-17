# QARMA Core Allocation Manager

## Overview

The Core Allocation Manager provides hybrid CPU core allocation with dynamic scaling and guaranteed minimums for subsystems. It enables efficient multi-core utilization across AI, Quantum, Security, Parallel, Video, and other subsystems.

## Architecture

### Subsystem Priorities & Default Policies

```
SUBSYSTEM         MIN  MAX  NUMA  PRIORITY  SHARING  PREEMPTION
Kernel            1    2    0     0         No       No
AI                2    4    0     1         Yes      No
Quantum           2    4    1     1         Yes      No
Parallel          1    8    -     2         Yes      Yes
Security          1    2    0     1         No       No
Video             1    2    -     2         Yes      Yes
I/O               0    2    -     3         Yes      Yes
Network           0    2    -     2         Yes      Yes
```

### Core States

- **FREE**: Available for allocation
- **RESERVED**: Reserved for subsystem minimum guarantee
- **ALLOCATED**: Actively allocated to a subsystem
- **SHARED**: Shared between multiple subsystems
- **OFFLINE**: Core is disabled

## API Usage

### For Subsystems

Each subsystem has convenience functions:

```c
// AI Subsystem
bool ai_request_cores(uint32_t count);
void ai_release_cores(void);
uint32_t ai_get_allocated_cores(void);
bool ai_run_on_dedicated_core(void (*function)(void*), void* data);

// Quantum Subsystem
bool quantum_request_cores(uint32_t count);
void quantum_release_cores(void);
uint32_t quantum_get_allocated_cores(void);
bool quantum_run_on_dedicated_core(void (*function)(void*), void* data);

// Security Manager
bool security_request_cores(uint32_t count);
void security_release_cores(void);
uint32_t security_get_allocated_cores(void);
bool security_run_on_dedicated_core(void (*function)(void*), void* data);
```

### Direct Core Manager API

```c
#include "core/core_manager.h"

// Initialize (called by kernel)
void core_manager_init(void);

// Request cores
core_request_t request = {
    .subsystem = SUBSYSTEM_AI,
    .core_count = 2,
    .preferred_numa = 0,
    .flags = CORE_ALLOC_SHARED
};
core_response_t response = core_request_allocate(&request);

// Release cores
core_release(SUBSYSTEM_AI, core_id);
core_release_all(SUBSYSTEM_AI);

// Query cores
uint32_t available = core_get_available_count(SUBSYSTEM_AI);
bool is_free = core_is_available(core_id);
subsystem_id_t owner = core_get_owner(core_id);

// Pin tasks to cores
core_pin_task(core_id, my_function, data);
core_pin_task_subsystem(SUBSYSTEM_AI, my_function, data);
```

### Allocation Flags

```c
CORE_ALLOC_PREFER_NUMA  = 0x01  // Prefer specific NUMA node
CORE_ALLOC_EXCLUSIVE    = 0x02  // Request exclusive access
CORE_ALLOC_SHARED       = 0x04  // Allow shared usage
CORE_ALLOC_PERSISTENT   = 0x08  // Keep allocation
CORE_ALLOC_HIGH_PRIORITY= 0x10  // High priority
```

## Example Usage

### AI Subsystem Requesting Cores

```c
void ai_start_training(void) {
    // Request 2 additional cores for training
    if (ai_request_cores(2)) {
        gfx_print("AI: Allocated 2 cores for training\n");
        
        // Run training on dedicated cores
        ai_run_on_dedicated_core(train_model_task, model_data);
    } else {
        gfx_print("AI: Could not allocate cores, using default\n");
    }
}

void ai_stop_training(void) {
    // Release cores when done
    ai_release_cores();
    gfx_print("AI: Released training cores\n");
}
```

### Quantum Subsystem with NUMA Awareness

```c
void quantum_start_simulation(void) {
    // Quantum prefers NUMA node 1 (set in default policy)
    if (quantum_request_cores(2)) {
        gfx_print("Quantum: Got 2 cores on NUMA node 1\n");
        
        // Run quantum simulation
        quantum_run_on_dedicated_core(simulate_qubits, qstate);
    }
}
```

### Security Manager Exclusive Access

```c
void security_scan_memory(void) {
    // Security uses EXCLUSIVE flag for sensitive operations
    if (security_request_cores(1)) {
        gfx_print("Security: Got exclusive core for scanning\n");
        
        // Run security scan on isolated core
        security_run_on_dedicated_core(memory_scan_task, scan_params);
    }
}
```

## Shell Commands

### `cores` Command

Display current core allocation map and statistics:

```
Shell> cores
=== Core Allocation Map ===
Core 0: RESERVED - AI
Core 1: RESERVED - AI
Core 2: RESERVED - Quantum
Core 3: RESERVED - Quantum
Core 4: FREE - None
Core 5: ALLOCATED - Parallel
Core 6: SHARED - Video
Core 7: FREE - None

=== Core Manager Statistics ===
Total cores: 8
Available cores: 3
Reserved cores: 4
Allocated cores: 5
```

## Boot Sequence

1. **Parallel Engine Init**: Detects CPU topology (cores, NUMA nodes)
2. **Core Manager Init**: 
   - Applies default policies
   - Reserves minimum cores for each subsystem by priority
   - Creates allocation tracking structures
3. **Subsystem Init**: Each subsystem can now request additional cores

## Load Balancing

The core manager supports:

- **Adaptive Load Balancing**: Automatically redistributes tasks
- **Work Stealing**: Idle cores steal work from busy cores
- **NUMA-Aware Scheduling**: Prefers local memory access
- **Priority Preemption**: Higher priority subsystems can preempt lower priority ones

## Future Enhancements

- [ ] Real CPUID-based topology detection
- [ ] APIC/SMP initialization for true multi-core
- [ ] CPU affinity masks for processes
- [ ] Hot-plug CPU support
- [ ] Power management integration
- [ ] Performance counter monitoring
- [ ] Dynamic policy adjustment based on load

## Integration with Parallel Engine

The core manager works with the parallel engine's task scheduler:

```
Subsystem → Core Manager → Parallel Engine → Hardware Cores
   ↓            ↓               ↓                 ↓
Request      Allocate        Schedule          Execute
Cores        & Track         Tasks             on Core
```

## Files

- `kernel/core/core_manager.h` - API definitions
- `kernel/core/core_manager.c` - Implementation
- `kernel/parallel/parallel_engine.h` - Task scheduling
- `kernel/parallel/parallel_engine.c` - Task execution
- `kernel/ai/ai_subsystem.c` - AI core integration
- `kernel/quantum/quantum_kernel.c` - Quantum core integration
- `kernel/security/security_manager.c` - Security core integration
