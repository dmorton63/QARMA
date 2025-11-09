#ifndef MESA_BOOTSTRAP_H
#define MESA_BOOTSTRAP_H
#include "log.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initializes EGL, creates a surfaceless OpenGL ES 2.0 context
void mesa_bootstrap_init(int x, int y, int width, int height);

// Cleans up EGL context and surface
void mesa_bootstrap_shutdown(void);

// Main render function that draws everything
void mesa_bootstrap_render(void);

// Text rendering functions
void mesa_add_text_message(const char* message);
void mesa_clear_text_messages(void);

// Check if shutdown was requested via mouse/keyboard
bool mesa_shutdown_requested(void);

void draw_text_centered(const char *msg, float center_x, float center_y, float scale);

// GUI drawing functions
void draw_rectangle(float x, float y, float width, float height, float r, float g, float b, float a);
void draw_taskbar(void);
void draw_title_bar(void);
void draw_main_window(void);
void draw_desktop_interface(void);

#ifdef __cplusplus
}
#endif

#endif // MESA_BOOTSTRAP_H