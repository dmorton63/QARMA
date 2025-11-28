#pragma once

#include "core/stdtools.h"

// Forward declarations
struct ControlBase;
typedef struct QARMA_INPUT_EVENT QARMA_INPUT_EVENT;

// ============================================================================
// Control Function Pointers
// ============================================================================

// Render callback: control renders itself to the buffer
typedef void (*ControlRenderFunc)(void* instance, uint32_t* buffer, int buffer_width, int buffer_height);

// Event callback: control handles input events
typedef bool (*ControlEventFunc)(void* instance, QARMA_INPUT_EVENT* event);

// Destroy callback: control cleans up resources
typedef void (*ControlDestroyFunc)(void* instance);

// ============================================================================
// Control Base - Common properties for all GUI controls
// ============================================================================

typedef struct ControlBase {
    int x, y;           // Position (relative to parent)
    int width, height;  // Dimensions
    bool visible;       // Is control visible
    bool enabled;       // Is control interactive
    uint32_t id;        // Unique control ID
    
    // Function pointers for polymorphic behavior
    void* instance;                        // Pointer to actual control (Button*, StatusBar*, etc.)
    ControlRenderFunc render;              // Render function
    ControlEventFunc handle_event;         // Event handler
    ControlDestroyFunc destroy;            // Cleanup function
} ControlBase;

// ============================================================================
// Helper Functions
// ============================================================================

// Check if a point is within control bounds
bool control_point_in_bounds(ControlBase* ctrl, int x, int y);

// Generate unique control ID
uint32_t control_generate_id(void);
