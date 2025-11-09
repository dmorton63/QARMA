# Qarma Copilot Instructions

## Project Overview

Qarma is a C-based CPU core management framework for multi-threaded subsystem dispatch. The core architecture revolves around registering subsystems and dispatching them across CPU cores using pthreads.

## Architecture

### Core Components

- **`cpu_core_manager`**: Central component managing subsystem registration and dispatch
  - Header: `headers/cpu_core_manager.h` - Defines the public API and function pointer interface
  - Implementation: `src/cpu_core_manager.c` - Contains pthread-based dispatch logic
- **`src/subsystems/`**: Directory for subsystem implementations (currently empty, extensible)

### Key Patterns

**Subsystem Registration Pattern:**
```c
typedef void (*subsystem_fn)(void);
void register_subsystem(const char* name, subsystem_fn fn);
```

**Thread Dispatch Model:**
- Maximum 8 concurrent subsystems (`MAX_SUBSYSTEMS`)
- Each subsystem runs in its own pthread
- Synchronous execution - main thread waits for all subsystems to complete
- Thread entry point wrapper logs subsystem names for debugging

### Data Structures

**Core Subsystem Structure:**
```c
typedef struct {
    const char* name;
    subsystem_fn fn;
} Subsystem;
```

**Manager Interface (vtable pattern):**
```c
typedef struct {
    void (*register_subsystem)(const char* name, void (*entry_point)(void));
    void (*dispatch)(void);
    void (*set_affinity)(const char* name, int core_id);  // Not yet implemented
    void (*debug)(void);  // Not yet implemented
} cpu_core_manager_t;
```

## Development Workflow

### Build System
- Uses `MakeFile` (currently empty - needs implementation)
- Standard C compilation with pthread linking required
- Compile command should include: `gcc -pthread src/cpu_core_manager.c`

### File Organization
- Headers in `headers/` directory with `.h` extension
- Source files in `src/` directory with `.c` extension
- Relative includes using `../headers/` pattern from source files

### Adding New Subsystems
1. Create subsystem source file in `src/subsystems/`
2. Implement subsystem function with signature `void subsystem_name(void)`
3. Register using `register_subsystem("name", subsystem_function)`
4. Call `dispatch_subsystems()` to execute all registered subsystems

## Threading Model

**Concurrency Constraints:**
- Static array limits to 8 subsystems maximum
- No dynamic memory allocation - uses static storage
- pthread_create/pthread_join pattern for synchronous execution
- Each subsystem gets its own thread but no CPU affinity control yet

**Critical Implementation Details:**
- Thread safety relies on static initialization and no shared mutable state during dispatch
- Subsystem registration must happen before dispatch (not thread-safe)
- Main thread blocks until all subsystems complete execution

## Extension Points

**Planned Features (based on interface):**
- CPU affinity control (`set_affinity` function pointer exists but unimplemented)
- Debug functionality (`debug` function pointer exists but unimplemented)
- Dynamic subsystem loading (would require refactoring static arrays)

When implementing new features, maintain the existing patterns of static storage and explicit threading control.