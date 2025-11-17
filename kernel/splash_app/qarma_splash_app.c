#include "qarma_splash_app.h"
#include "graphics/framebuffer.h"
#include "core/memory.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_win_factory.h"
#include "qarma_splash_window.h"

extern QARMA_WINDOW_MANAGER qarma_window_manager;

QARMA_WIN_TYPE splash_window_type = QARMA_WIN_TYPE_SPLASH;

void splash_init(QARMA_APP_HANDLE *self) {
    QARMA_WIN_HANDLE *win = qarma_win_create_archetype(QARMA_WIN_TYPE_SPLASH, "Welcome", QARMA_FLAG_VISIBLE | QARMA_FLAG_FADE_OUT);
    if (!win) return;

    win->position = (QARMA_COORD){30, 10};
    win->size = (QARMA_DIMENSION){640, 480};
    win->background = (QARMA_COLOR){0, 0, 0, 255};
    win->owner = self;

    self->main_window = win;
}

void splash_app_update(QARMA_APP_HANDLE *self, QARMA_TICK_CONTEXT *ctx) {
    if (self->main_window && self->main_window->vtable && self->main_window->vtable->update) {
        self->main_window->vtable->update(self->main_window, ctx);
    }
}


void splash_shutdown(QARMA_APP_HANDLE *self) {
    
    if (self->main_window && self->main_window->vtable && self->main_window->vtable->destroy) {
        self->main_window->vtable->destroy(self->main_window);
    }
    gfx_printf("[QARMA] Splash app shutdown.\n");
}

void splash_event(QARMA_APP_HANDLE *self, QARMA_EVENT *event) {
    (void)self;
    (void)event;
    // placeholder for future ritual
}

QARMA_APP_HANDLE splash_app = {
    .id = 1,
    .name = "Splash Ritual",
    .main_window = NULL,
    .state = NULL,
    .init = splash_init,
    .update = splash_app_update,
    .handle_event = splash_event,
    .shutdown = splash_shutdown
};

