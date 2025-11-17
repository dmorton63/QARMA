#pragma once

#include "core/stdtools.h"
#include "qarma_win_handle.h"
//#include "qarma_schedtypedefs.h"
typedef struct QARMA_WINDOW_MANAGER QARMA_WINDOW_MANAGER;

struct QARMA_WINDOW_MANAGER{
    QARMA_WIN_HANDLE* windows[QARMA_MAX_WINDOWS];
    uint32_t count;

    void (*add_window)(struct QARMA_WINDOW_MANAGER*, QARMA_WIN_HANDLE*, const char* caller);
    void (*remove_window)(struct QARMA_WINDOW_MANAGER*, uint32_t id);
    void (*update_all)(struct QARMA_WINDOW_MANAGER*, QARMA_TICK_CONTEXT*);
    void (*render_all)(struct QARMA_WINDOW_MANAGER*);
    void (*destroy_all)(struct QARMA_WINDOW_MANAGER*);
};


void qarma_window_manager_init();

extern QARMA_WINDOW_MANAGER qarma_window_manager;