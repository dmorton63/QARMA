/**
 * Nomain IDE - QARMA Window System Integration
 * 
 * Integrates the IDE with QARMA's native window manager
 */

#ifndef NOMAIN_IDE_WINDOW_H
#define NOMAIN_IDE_WINDOW_H

#include "nomain_ide.h"
#include "qarma_win_handle.h"
#include "qarma_window_manager.h"
#include "qarma_input_events.h"

/**
 * Why use QARMA's window system:
 * 
 * ✓ Already integrated with desktop
 * ✓ Handles window compositing and rendering
 * ✓ Manages keyboard/mouse events
 * ✓ Provides standard window decorations (title bar, borders)
 * ✓ Supports multiple windows (for dialogs, file browser, etc.)
 * ✓ Built-in focus management
 * ✓ Consistent UI with rest of QARMA
 */

// IDE Window Components
typedef struct {
    QARMA_WIN_HANDLE* main_window;      // Main IDE window
    QARMA_WIN_HANDLE* editor_pane;      // Text editor pane
    QARMA_WIN_HANDLE* output_pane;      // Compiler output pane
    QARMA_WIN_HANDLE* toolbar;          // Toolbar buttons
    QARMA_WIN_HANDLE* statusbar;        // Status bar
    QARMA_WIN_HANDLE* menubar;          // Menu bar (File, Edit, Build, Help)
} IDEWindows;

// ============================================================================
// Window Creation
// ============================================================================

/**
 * Create IDE window using QARMA window system
 */
IDEWindows* ide_windows_create(NomainIDE* ide);

/**
 * Destroy all IDE windows
 */
void ide_windows_destroy(IDEWindows* windows);

// ============================================================================
// Layout Management
// ============================================================================

/**
 * Layout: 
 * +------------------------------------------+
 * | Menu Bar (File Edit Build Run Help)     |
 * +------------------------------------------+
 * | Toolbar [New][Open][Save]   [Compile]   |
 * +------------------------------------------+
 * |                                          |
 * |         Editor Pane                      |
 * |         (Text Editor)                    |
 * |                                          |
 * +------------------------------------------+
 * |         Output Pane                      |
 * |         (Compiler Messages)              |
 * +------------------------------------------+
 * | Status Bar: Line 1, Col 1 | Ready       |
 * +------------------------------------------+
 */

/**
 * Update window layout (e.g., when toggling output pane)
 */
void ide_windows_update_layout(IDEWindows* windows, bool show_output);

/**
 * Resize windows to fit new dimensions
 */
void ide_windows_resize(IDEWindows* windows, int width, int height);

// ============================================================================
// Event Handling
// ============================================================================

/**
 * Handle QARMA input event
 * Routes events to appropriate component based on focus
 */
void ide_windows_handle_event(IDEWindows* windows, NomainIDE* ide, QARMA_INPUT_EVENT* event);

/**
 * Handle keyboard shortcut
 * Returns: true if handled, false to pass to editor
 */
bool ide_windows_handle_shortcut(NomainIDE* ide, QARMA_INPUT_EVENT* event);

// ============================================================================
// Rendering
// ============================================================================

/**
 * Render IDE to screen
 * QARMA window manager handles compositing
 */
void ide_windows_render(IDEWindows* windows, NomainIDE* ide);

/**
 * Update status bar text
 */
void ide_windows_update_status(IDEWindows* windows, const char* message, 
                               int line, int column);

/**
 * Update output pane with compiler messages
 */
void ide_windows_update_output(IDEWindows* windows, const char* output);

// ============================================================================
// Dialogs
// ============================================================================

/**
 * Show file open dialog
 */
char* ide_windows_show_open_dialog(IDEWindows* windows);

/**
 * Show file save dialog
 */
char* ide_windows_show_save_dialog(IDEWindows* windows, const char* default_name);

/**
 * Show error message box
 */
void ide_windows_show_error(IDEWindows* windows, const char* title, const char* message);

/**
 * Show confirmation dialog (Yes/No)
 */
bool ide_windows_show_confirm(IDEWindows* windows, const char* title, const char* message);

// ============================================================================
// Menu System
// ============================================================================

typedef enum {
    MENU_FILE_NEW,
    MENU_FILE_OPEN,
    MENU_FILE_SAVE,
    MENU_FILE_SAVE_AS,
    MENU_FILE_CLOSE,
    MENU_FILE_EXIT,
    
    MENU_EDIT_UNDO,
    MENU_EDIT_REDO,
    MENU_EDIT_CUT,
    MENU_EDIT_COPY,
    MENU_EDIT_PASTE,
    MENU_EDIT_SELECT_ALL,
    MENU_EDIT_FIND,
    MENU_EDIT_REPLACE,
    
    MENU_BUILD_COMPILE,
    MENU_BUILD_RUN,
    MENU_BUILD_COMPILE_AND_RUN,
    MENU_BUILD_STOP,
    
    MENU_VIEW_TOGGLE_OUTPUT,
    MENU_VIEW_TOGGLE_LINE_NUMBERS,
    MENU_VIEW_FONT_SIZE_INCREASE,
    MENU_VIEW_FONT_SIZE_DECREASE,
    
    MENU_HELP_ABOUT,
    MENU_HELP_NOMAIN_DOCS
} MenuAction;

/**
 * Create menu bar
 */
void ide_windows_create_menu(IDEWindows* windows);

/**
 * Handle menu selection
 */
void ide_windows_handle_menu(NomainIDE* ide, MenuAction action);

#endif // NOMAIN_IDE_WINDOW_H
