#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "qarma_win_handle/qarma_win_handle.h"
#include "qarma_win_handle/qarma_input_events.h"
#include "gui/gui.h"

// Main desktop window - full screen with close button
typedef struct {
    QARMA_WIN_HANDLE* win;
    CloseButton close_btn;
    bool should_exit;
} MainWindow;

// Create the main desktop window
MainWindow* main_window_create(void);

// Update main window
void main_window_update(MainWindow* mw);

// Render main window
void main_window_render(MainWindow* mw);

// Handle events
void main_window_handle_event(MainWindow* mw, QARMA_INPUT_EVENT* event);

// Destroy main window
void main_window_destroy(MainWindow* mw);

// Check if should exit (close button clicked)
bool main_window_should_exit(MainWindow* mw);

#endif // MAIN_WINDOW_H
