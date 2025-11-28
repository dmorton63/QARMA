#include "qarma_win_factory.h"
//#include "../window_types/qarma_splash_app.h"  // For splash-specific init
#include "qarma_window_manager.h"
#include "splash_app/qarma_splash_window.h"
#include "graphics/graphics.h"
#include "config.h"

QARMA_WIN_HANDLE* qarma_win_create(QARMA_WIN_TYPE type, const char* title, uint32_t flags) {
    SERIAL_LOG("[WINFACTORY] Creating window\n");
    QARMA_WIN_HANDLE* win = malloc(sizeof(QARMA_WIN_HANDLE));
    SERIAL_LOG("[WINFACTORY] malloc returned\n");
    if (!win) {
        SERIAL_LOG("[WINFACTORY] malloc failed!\n");
        return NULL;
    }

    SERIAL_LOG("[WINFACTORY] Initializing window structure\n");
    win->id = qarma_generate_window_id();
    win->type = type;
    win->flags = flags;
    win->x = 100;
    win->y = 100;
    win->size.width = 400;    // Default width
    win->size.height = 300;   // Default height
    win->alpha = 1.0f;
    win->title = title;
    
    // Create empty vtable to avoid panic in window manager
    // Login screen doesn't need vtable functions since it handles its own rendering
    static QARMA_WIN_VTABLE empty_vtable = {0};
    win->vtable = &empty_vtable;
    win->traits = NULL;   // No traits attached
    
    // Allocate pixel buffer for window using heap_alloc (20MB heap)
    extern void* heap_alloc(size_t size);
    size_t buffer_size = win->size.width * win->size.height * sizeof(uint32_t);
    win->pixel_buffer = heap_alloc(buffer_size);
    if (!win->pixel_buffer) {
        SERIAL_LOG("[WINFACTORY] Failed to allocate pixel buffer\n");
        free(win);
        return NULL;
    }
    SERIAL_LOG("[WINFACTORY] Pixel buffer allocated\n");
    
    SERIAL_LOG("[WINFACTORY] Adding to window manager\n");
    qarma_window_manager.add_window(&qarma_window_manager, win,"Win Factory");
    SERIAL_LOG("[WINFACTORY] Window created successfully\n");
    return win;
}

QARMA_WIN_HANDLE* qarma_win_create_archetype(uint32_t archetype_id, const char* title, uint32_t flags) {
    switch (archetype_id) {
        case QARMA_WIN_TYPE_SPLASH:
            return (QARMA_WIN_HANDLE*) splash_window_create(title, flags);

        // case QARMA_WIN_TYPE_MODAL:
        //      return (QARMA_WIN_HANDLE*) modal_window_create(title, flags);

        // case QARMA_WIN_TYPE_DIALOG:
        //      return (QARMA_WIN_HANDLE*) dialog_window_create(title, flags);
        case QARMA_WIN_TYPE_CLOCK_OVERLAY:
            return qarma_win_create(QARMA_WIN_TYPE_CLOCK_OVERLAY, title, flags);

        case QARMA_WIN_TYPE_GENERIC:
            
        default:
            panic("qarma_win_create_archetype: unknown archetype ID");
            return NULL;
    }
}