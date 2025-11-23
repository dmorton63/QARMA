/**
 * Nomain IDE - Keyboard Shortcut Handler
 * 
 * Standard keyboard shortcuts for text editing
 */

#include "text_editor.h"
#include "keyboard/keyboard.h"  // For scancode definitions

// Clipboard (simplified - will need proper implementation)
static char* g_clipboard = NULL;
static int g_clipboard_size = 0;

/**
 * Copy selected text to clipboard
 */
void text_editor_copy(TextEditor* editor) {
    if (!editor || !editor->selection.active) return;
    
    char* selected = text_editor_get_selection(editor);
    if (!selected) return;
    
    // Free old clipboard
    if (g_clipboard) {
        free(g_clipboard);
    }
    
    g_clipboard = selected;
    g_clipboard_size = strlen(selected);
}

/**
 * Cut selected text to clipboard
 */
void text_editor_cut(TextEditor* editor) {
    if (!editor || !editor->selection.active) return;
    
    // Copy first
    text_editor_copy(editor);
    
    // Then delete selection
    text_editor_delete_selection(editor);
    editor->modified = true;
}

/**
 * Paste from clipboard at cursor
 */
void text_editor_paste(TextEditor* editor) {
    if (!editor || !g_clipboard) return;
    
    // Delete selection first if active
    if (editor->selection.active) {
        text_editor_delete_selection(editor);
    }
    
    // Insert clipboard text
    text_editor_insert_string(editor, g_clipboard);
    editor->modified = true;
}

/**
 * Select all text
 */
void text_editor_select_all(TextEditor* editor) {
    if (!editor) return;
    
    editor->selection.active = true;
    editor->selection.start.line = 0;
    editor->selection.start.column = 0;
    editor->selection.end.line = editor->line_count - 1;
    editor->selection.end.column = editor->lines[editor->line_count - 1].length;
}

/**
 * Handle keyboard input with shortcuts
 */
void text_editor_handle_key(TextEditor* editor, uint8_t scancode, char ascii, bool shift, bool ctrl) {
    if (!editor) return;
    
    // Handle Ctrl shortcuts
    if (ctrl) {
        switch (ascii) {
            case 'z':
            case 'Z':
                text_editor_undo(editor);
                return;
                
            case 'y':
            case 'Y':
                text_editor_redo(editor);
                return;
                
            case 'c':
            case 'C':
                text_editor_copy(editor);
                return;
                
            case 'x':
            case 'X':
                text_editor_cut(editor);
                return;
                
            case 'v':
            case 'V':
                text_editor_paste(editor);
                return;
                
            case 'a':
            case 'A':
                text_editor_select_all(editor);
                return;
                
            case 's':
            case 'S':
                // Ctrl+S - Save (handled by IDE, not editor)
                // Signal save request
                return;
                
            case 'f':
            case 'F':
                // Ctrl+F - Find (will open find dialog)
                return;
        }
    }
    
    // Handle Shift for selection
    if (shift) {
        if (!editor->selection.active) {
            text_editor_selection_start(editor);
        }
    } else {
        if (editor->selection.active) {
            text_editor_selection_clear(editor);
        }
    }
    
    // Handle navigation keys
    switch (scancode) {
        case 0x48: // Up arrow
            text_editor_cursor_up(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x50: // Down arrow
            text_editor_cursor_down(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x4B: // Left arrow
            text_editor_cursor_left(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x4D: // Right arrow
            text_editor_cursor_right(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x47: // Home
            text_editor_cursor_home(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x4F: // End
            text_editor_cursor_end(editor);
            if (shift) text_editor_selection_update(editor);
            break;
            
        case 0x0E: // Backspace
            if (editor->selection.active) {
                text_editor_delete_selection(editor);
            } else {
                text_editor_delete_char(editor);
            }
            break;
            
        case 0x53: // Delete
            if (editor->selection.active) {
                text_editor_delete_selection(editor);
            } else {
                text_editor_delete_forward(editor);
            }
            break;
            
        case 0x1C: // Enter
            text_editor_insert_newline(editor);
            break;
            
        case 0x0F: // Tab
            text_editor_insert_string(editor, "    "); // 4 spaces
            break;
            
        default:
            // Regular character input
            if (ascii >= 32 && ascii <= 126) {
                if (editor->selection.active) {
                    text_editor_delete_selection(editor);
                }
                text_editor_insert_char(editor, ascii);
            }
            break;
    }
}
