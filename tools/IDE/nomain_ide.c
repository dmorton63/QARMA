/**
 * Nomain IDE - Main Implementation
 * 
 * Entry point and core IDE functionality
 */

#include "nomain_ide.h"
#include "text_editor.h"
#include <string.h>
#include <stdlib.h>

// Global IDE instance
static NomainIDE* g_ide = NULL;

/**
 * Initialize the nomain IDE
 */
NomainIDE* nomain_ide_init(void) {
    NomainIDE* ide = (NomainIDE*)malloc(sizeof(NomainIDE));
    if (!ide) return NULL;
    
    memset(ide, 0, sizeof(NomainIDE));
    
    // Initialize editor
    ide->editor = text_editor_create(IDE_WINDOW_WIDTH, EDITOR_HEIGHT);
    if (!ide->editor) {
        free(ide);
        return NULL;
    }
    
    // Default settings
    ide->show_line_numbers = true;
    ide->show_output = true;
    ide->font_size = 12;
    ide->current_file = NULL;
    ide->modified = false;
    
    g_ide = ide;
    return ide;
}

/**
 * Run the IDE main loop
 */
void nomain_ide_run(NomainIDE* ide) {
    if (!ide) return;
    
    // TODO: Main event loop
    // - Handle keyboard input
    // - Update display
    // - Process compiler output
    // - Handle menu actions
}

/**
 * Cleanup and shutdown IDE
 */
void nomain_ide_shutdown(NomainIDE* ide) {
    if (!ide) return;
    
    if (ide->editor) {
        text_editor_destroy(ide->editor);
    }
    
    if (ide->current_file) {
        free(ide->current_file);
    }
    
    free(ide);
    g_ide = NULL;
}

/**
 * Create a new file
 */
bool nomain_ide_file_new(NomainIDE* ide) {
    if (!ide || !ide->editor) return false;
    
    // TODO: Check for unsaved changes
    
    text_editor_clear(ide->editor);
    
    if (ide->current_file) {
        free(ide->current_file);
        ide->current_file = NULL;
    }
    
    ide->modified = false;
    nomain_ide_update_status(ide, "New file");
    
    return true;
}

/**
 * Open an existing file
 */
bool nomain_ide_file_open(NomainIDE* ide, const char* filename) {
    if (!ide || !filename) return false;
    
    // TODO: Implement file loading from QARMA filesystem
    // For now, just placeholder
    
    nomain_ide_update_status(ide, "File opened");
    return true;
}

/**
 * Save current file
 */
bool nomain_ide_file_save(NomainIDE* ide) {
    if (!ide) return false;
    
    if (!ide->current_file) {
        // Need to prompt for filename
        return false;
    }
    
    // TODO: Implement file saving to QARMA filesystem
    
    ide->modified = false;
    nomain_ide_update_status(ide, "File saved");
    
    return true;
}

/**
 * Save current file with new name
 */
bool nomain_ide_file_save_as(NomainIDE* ide, const char* filename) {
    if (!ide || !filename) return false;
    
    // TODO: Implement file saving
    
    if (ide->current_file) {
        free(ide->current_file);
    }
    ide->current_file = strdup(filename);
    ide->modified = false;
    
    nomain_ide_update_status(ide, "File saved");
    return true;
}

/**
 * Compile current file
 */
bool nomain_ide_compile(NomainIDE* ide) {
    if (!ide) return false;
    
    nomain_ide_update_status(ide, "Compiling...");
    ide->compiling = true;
    
    // TODO: Call nomain compiler
    // - Get text from editor
    // - Pass to compiler
    // - Parse output for errors
    // - Display in output window
    
    ide->compiling = false;
    ide->error_count = 0;
    ide->warning_count = 0;
    
    nomain_ide_update_status(ide, "Compilation successful");
    return true;
}

/**
 * Compile and run current file
 */
bool nomain_ide_compile_and_run(NomainIDE* ide) {
    if (!ide) return false;
    
    if (!nomain_ide_compile(ide)) {
        return false;
    }
    
    if (ide->error_count > 0) {
        nomain_ide_show_error(ide, "Cannot run: compilation errors");
        return false;
    }
    
    // TODO: Execute compiled program
    nomain_ide_update_status(ide, "Running program...");
    
    return true;
}

/**
 * Stop running program
 */
void nomain_ide_stop_program(NomainIDE* ide) {
    if (!ide) return;
    
    // TODO: Kill running process
    nomain_ide_update_status(ide, "Program stopped");
}

/**
 * Toggle output window visibility
 */
void nomain_ide_toggle_output(NomainIDE* ide) {
    if (!ide) return;
    
    ide->show_output = !ide->show_output;
    // TODO: Resize editor window
}

/**
 * Toggle line numbers
 */
void nomain_ide_toggle_line_numbers(NomainIDE* ide) {
    if (!ide || !ide->editor) return;
    
    ide->show_line_numbers = !ide->show_line_numbers;
    ide->editor->show_line_numbers = ide->show_line_numbers;
}

/**
 * Update status bar
 */
void nomain_ide_update_status(NomainIDE* ide, const char* message) {
    if (!ide || !message) return;
    
    // TODO: Display message in status bar
}

/**
 * Display error message
 */
void nomain_ide_show_error(NomainIDE* ide, const char* message) {
    if (!ide || !message) return;
    
    // TODO: Display error dialog or message box
}
