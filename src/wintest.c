#include "wintest.h"

void setup_editor_window(void) {
    QarmaWindow editor = {0};
    WINDOW_INIT(&editor, "Text Editor", 1024, 768);
    create_window(&editor);
    #ifdef DEBUG
    LOG("Creating editor window: %s (W: %d, H: %d, Style: %d, VSync: %d, OpenGL: %d.%d)\n",
           editor.header.title,
           editor.geometry.width,
           editor.geometry.height,
           editor.style.resizable,
           editor.vsync_enabled,
           editor.context.context_major,
           editor.context.context_minor);    
    #endif
    // other setup...
}