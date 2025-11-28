#pragma once

#include "qarma_win_handle/qarma_win_handle.h"
typedef struct {
    QARMA_WIN_HANDLE base;
    float fade_speed;
} QARMA_SPLASH_WINDOW;

void splash_window_render(QARMA_WIN_HANDLE *self);

void splash_window_update(QARMA_WIN_HANDLE *self, QARMA_TICK_CONTEXT *ctx);

//void splash_update(QARMA_APP_HANDLE *self, QARMA_TICK_CONTEXT *ctx);

void splash_shutdown(QARMA_APP_HANDLE *self);

void splash_event(QARMA_APP_HANDLE *self, QARMA_EVENT *event);

void splash_window_destroy(QARMA_WIN_HANDLE *self);

void qarma_winhandler_remove(QARMA_WIN_HANDLE *handler, uint32_t id);
