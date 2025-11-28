#ifndef DESKTOP_TOOLBAR_H
#define DESKTOP_TOOLBAR_H

#include "kernel_types.h"
#include "gui/frame.h"

typedef struct {
    qarma_frame_t* frame;
    qarma_control_t* shutdown_button;
    qarma_control_t* restart_button;
    qarma_control_t* cmd_button;
    uint32_t background_color;
} desktop_toolbar_t;

// Create and initialize the desktop toolbar
desktop_toolbar_t* desktop_toolbar_create(void);

// Render the toolbar
void desktop_toolbar_render(desktop_toolbar_t* toolbar);

// Destroy the toolbar
void desktop_toolbar_destroy(desktop_toolbar_t* toolbar);

// Get global toolbar instance
desktop_toolbar_t* desktop_toolbar_get_global(void);

#endif // DESKTOP_TOOLBAR_H
