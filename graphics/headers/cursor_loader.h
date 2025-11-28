/**
 * QARMA - Cursor/Icon Asset Loader
 * 
 * Loads cursor images from ISO filesystem or host shared directory
 */

#pragma once

#include "core/stdtools.h"

// Cursor types
typedef enum {
    CURSOR_ARROW,
    CURSOR_HAND,
    CURSOR_IBEAM,
    CURSOR_WAIT,
    CURSOR_CROSSHAIR,
    CURSOR_MAX
} cursor_type_t;

// Cursor data structure
typedef struct {
    uint32_t* pixels;    // ARGB pixel data
    uint32_t width;
    uint32_t height;
    int32_t hotspot_x;   // Click point offset
    int32_t hotspot_y;
} cursor_data_t;

// Initialize cursor system
void cursor_loader_init(void);

// Load cursor from file
bool cursor_load(cursor_type_t type, const char* filename);

// Get cursor data
const cursor_data_t* cursor_get(cursor_type_t type);

// Load from ISO assets directory
bool cursor_load_from_iso(cursor_type_t type, const char* filename);

// Load from host shared directory (requires 9P)
bool cursor_load_from_host(cursor_type_t type, const char* filename);

// Create default software-rendered cursors
void cursor_create_defaults(void);
