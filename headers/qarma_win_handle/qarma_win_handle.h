#pragma once

#include "core/stdtools.h"
#include "../memory.h"
#define QARMA_MAX_PROCESSES 32
#define QARMA_TICK_RATE 60
#define QARMA_MAX_MODULES 32

#define QARMA_EVENT_INPUT     0x01
#define QARMA_EVENT_RENDER    0x02
#define QARMA_EVENT_MEMORY    0x03
#define QARMA_EVENT_SHUTDOWN  0xFF

#define QARMA_FLAG_WIN_MODAL       0x01
#define QARMA_FLAG_WIN_FADE_OUT    0x02
#define QARMA_FLAG_WIN_VISIBLE     0x04

#define QARMA_MEM_TAG_WIN     "QWIN"
#define QARMA_MEM_TAG_APP     "APP"
#define QARMA_MEM_TAG_EVENT   "EVENT"


#define QARMA_MAX_WINDOWS 64
typedef struct QARMA_WIN_HANDLE QARMA_WIN_HANDLE;
typedef struct QARMA_INPUT_EVENT QARMA_INPUT_EVENT;

typedef struct {
    uint8_t r, g, b, a;
} QARMA_COLOR;

typedef enum {
    QARMA_WIN_TYPE_GENERIC,
    QARMA_WIN_TYPE_SPLASH,
    QARMA_WIN_TYPE_MODAL,
    QARMA_WIN_TYPE_DEBUG,
    QARMA_WIN_TYPE_CUSTOM,
    QARMA_WIN_TYPE_CLOCK_OVERLAY,
    QARMA_WIN_TYPE_DIALOG
} QARMA_WIN_TYPE;

typedef enum {
    QARMA_FLAG_VISIBLE      = 1 << 0,
    QARMA_FLAG_FADE_OUT     = 1 << 1,
    QARMA_FLAG_TOPMOST      = 1 << 2,
    QARMA_FLAG_INTERACTIVE  = 1 << 3
} QARMA_WIN_FLAGS;

typedef struct {
    int x;
    int y;
} QARMA_COORD;

typedef struct {
    int width;
    int height;
} QARMA_DIMENSION;

typedef struct {
    uint64_t tick_count;
    float delta_time;         // Time since last tick
    float uptime_seconds;
} QARMA_TICK_CONTEXT;

typedef struct QARMA_WIN_VTABLE {
    void (*init)(QARMA_WIN_HANDLE* self, const char* title, uint32_t flags);
    void (*update)(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx);
    void (*render)(QARMA_WIN_HANDLE* self);
    void (*destroy)(QARMA_WIN_HANDLE* self);
} QARMA_WIN_VTABLE;

// Forward declaration
struct QARMA_WIN_VTABLE;

// Forward declaration for controls
struct ControlBase;

#define QARMA_MAX_CONTROLS_PER_WINDOW 32

typedef struct QARMA_WIN_HANDLE {
    uint32_t id;
    QARMA_WIN_TYPE type;
    uint32_t flags;
    int x, y;
    //int width, height;
    float alpha;
    const char* title;
    QARMA_COLOR background;
    QARMA_COORD position;
    QARMA_DIMENSION size;
    uint32_t* pixel_buffer;
    QARMA_DIMENSION buffer_size;
    struct QARMA_WIN_VTABLE* vtable;  // NEW: behavior dispatch
    void* traits;                     // OPTIONAL: extended memory
    bool dirty;
    void* owner;                    // Owning application
    
    // Control management
    struct ControlBase* controls[QARMA_MAX_CONTROLS_PER_WINDOW];
    int control_count;
} QARMA_WIN_HANDLE;

typedef struct {
    uint32_t type;
    void *payload;
    const char *origin;
    const char *target;
} QARMA_EVENT;

typedef struct QARMA_APP_HANDLE {
    uint32_t id;                          // Unique app ID
    const char *name;                     // App name or title
    QARMA_WIN_HANDLE *main_window;        // Primary window glyph
    void *state;                          // App-specific state or context
    void (*init)(struct QARMA_APP_HANDLE *self);     // Initialization ritual
    void (*update)(struct QARMA_APP_HANDLE *self, QARMA_TICK_CONTEXT *ctx); // Tick update
    void (*handle_event)(struct QARMA_APP_HANDLE *self, QARMA_EVENT *event); // Event dispatch
    void (*shutdown)(struct QARMA_APP_HANDLE *self); // Destruction ritual
} QARMA_APP_HANDLE;

uint32_t qarma_generate_window_id();
void qarma_win_assign_vtable(QARMA_WIN_HANDLE* win, QARMA_WIN_VTABLE* vtable);

// ============================================================================
// Control Management Functions
// ============================================================================

// Add a control to the window
bool qarma_win_add_control(QARMA_WIN_HANDLE* win, struct ControlBase* control);

// Remove a control from the window
bool qarma_win_remove_control(QARMA_WIN_HANDLE* win, struct ControlBase* control);

// Get a control by ID
struct ControlBase* qarma_win_get_control(QARMA_WIN_HANDLE* win, uint32_t control_id);

// Render all visible controls
void qarma_win_render_controls(QARMA_WIN_HANDLE* win);

// Dispatch an event to controls (returns true if handled)
bool qarma_win_dispatch_event(QARMA_WIN_HANDLE* win, QARMA_INPUT_EVENT* event);

