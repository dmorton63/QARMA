#ifndef QARMA_SPLASH_WINDOW_H
#define QARMA_SPLASH_WINDOW_H

#include "qarma_win_handle/qarma_win_handle.h"
typedef struct QARMA_SPLASH_HANDLE QARMA_SPLASH_HANDLE;
// typedef struct {
//     QARMA_WIN_HANDLE base;
//     float fade_speed;
// } QARMA_SPLASH_HANDLE;

QARMA_SPLASH_HANDLE* splash_window_create(const char* title, uint32_t flags);
// static void splash_update(QARMA_WIN_HANDLE* self, QARMA_TICK_CONTEXT* ctx);
// static void splash_render(QARMA_WIN_HANDLE* self);
// static void splash_destroy(QARMA_WIN_HANDLE* self);
void splash_app_update(QARMA_APP_HANDLE *self, QARMA_TICK_CONTEXT *ctx);

#endif // QARMA_SPLASH_WINDOW_H