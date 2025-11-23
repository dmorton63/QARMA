/**
 * Nomain IDE - Text Editor Component
 * 
 * Multi-line text editor with syntax highlighting for nomain language
 */

#ifndef NOMAIN_EDITOR_H
#define NOMAIN_EDITOR_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_LINES       1000
#define MAX_LINE_LENGTH 256
#define TAB_SIZE        4

// Text buffer line
typedef struct {
    char text[MAX_LINE_LENGTH];
    int length;
    bool modified;
} EditorLine;

// Cursor position
typedef struct {
    int line;
    int column;
} CursorPos;

// Selection range
typedef struct {
    CursorPos start;
    CursorPos end;
    bool active;
} Selection;

// Text editor state
typedef struct TextEditor {
    // Buffer
    EditorLine lines[MAX_LINES];
    int line_count;
    
    // Cursor
    CursorPos cursor;
    int scroll_offset;          // First visible line
    
    // Selection
    Selection selection;
    
    // Display
    int visible_lines;          // Lines that fit on screen
    int visible_columns;        // Characters per line
    bool show_line_numbers;
    
    // State
    bool modified;
    bool read_only;
} TextEditor;

// ============================================================================
// Editor Management
// ============================================================================

/**
 * Initialize text editor
 */
TextEditor* text_editor_create(int width, int height);

/**
 * Cleanup editor
 */
void text_editor_destroy(TextEditor* editor);

/**
 * Clear all text
 */
void text_editor_clear(TextEditor* editor);

/**
 * Load text from buffer
 */
bool text_editor_load_text(TextEditor* editor, const char* text, int length);

/**
 * Get all text as string
 */
char* text_editor_get_text(TextEditor* editor, int* length);

// ============================================================================
// Cursor Operations
// ============================================================================

/**
 * Move cursor to position
 */
void text_editor_move_cursor(TextEditor* editor, int line, int column);

/**
 * Move cursor up
 */
void text_editor_cursor_up(TextEditor* editor);

/**
 * Move cursor down
 */
void text_editor_cursor_down(TextEditor* editor);

/**
 * Move cursor left
 */
void text_editor_cursor_left(TextEditor* editor);

/**
 * Move cursor right
 */
void text_editor_cursor_right(TextEditor* editor);

/**
 * Move to start of line
 */
void text_editor_cursor_home(TextEditor* editor);

/**
 * Move to end of line
 */
void text_editor_cursor_end(TextEditor* editor);

// ============================================================================
// Text Operations
// ============================================================================

/**
 * Insert character at cursor
 */
void text_editor_insert_char(TextEditor* editor, char c);

/**
 * Insert string at cursor
 */
void text_editor_insert_string(TextEditor* editor, const char* str);

/**
 * Delete character before cursor (backspace)
 */
void text_editor_delete_char(TextEditor* editor);

/**
 * Delete character at cursor (delete key)
 */
void text_editor_delete_forward(TextEditor* editor);

/**
 * Insert new line at cursor
 */
void text_editor_insert_newline(TextEditor* editor);

/**
 * Delete current line
 */
void text_editor_delete_line(TextEditor* editor);

// ============================================================================
// Selection Operations
// ============================================================================

/**
 * Start selection at cursor
 */
void text_editor_selection_start(TextEditor* editor);

/**
 * Update selection to cursor
 */
void text_editor_selection_update(TextEditor* editor);

/**
 * Clear selection
 */
void text_editor_selection_clear(TextEditor* editor);

/**
 * Get selected text
 */
char* text_editor_get_selection(TextEditor* editor);

/**
 * Delete selected text
 */
void text_editor_delete_selection(TextEditor* editor);

// ============================================================================
// Clipboard Operations
// ============================================================================

/**
 * Copy selected text to clipboard
 */
void text_editor_copy(TextEditor* editor);

/**
 * Cut selected text to clipboard
 */
void text_editor_cut(TextEditor* editor);

/**
 * Paste from clipboard at cursor
 */
void text_editor_paste(TextEditor* editor);

/**
 * Select all text
 */
void text_editor_select_all(TextEditor* editor);

// ============================================================================
// Undo/Redo
// ============================================================================

#define MAX_UNDO_LEVELS 100

typedef enum {
    UNDO_INSERT,
    UNDO_DELETE,
    UNDO_REPLACE
} UndoType;

typedef struct UndoAction {
    UndoType type;
    CursorPos position;
    char* text;
    int length;
} UndoAction;

/**
 * Undo last action
 */
void text_editor_undo(TextEditor* editor);

/**
 * Redo last undone action
 */
void text_editor_redo(TextEditor* editor);

// ============================================================================
// Search Operations
// ============================================================================

/**
 * Find text in editor
 */
bool text_editor_find(TextEditor* editor, const char* search_text, bool case_sensitive);

/**
 * Find and replace
 */
int text_editor_replace_all(TextEditor* editor, const char* find, const char* replace);

// ============================================================================
// Rendering
// ============================================================================

/**
 * Render editor to screen buffer
 */
void text_editor_render(TextEditor* editor, uint32_t* framebuffer, 
                       int fb_width, int fb_height, int x, int y);

/**
 * Handle keyboard input
 * Supports standard shortcuts:
 * - Ctrl+Z: Undo
 * - Ctrl+Y: Redo
 * - Ctrl+C: Copy
 * - Ctrl+X: Cut
 * - Ctrl+V: Paste
 * - Ctrl+A: Select All
 * - Ctrl+F: Find
 * - Ctrl+S: Save (passed to IDE)
 */
void text_editor_handle_key(TextEditor* editor, uint8_t scancode, char ascii, bool shift, bool ctrl);

#endif // NOMAIN_EDITOR_H
