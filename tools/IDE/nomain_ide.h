/**
 * Nomain IDE - Main Header
 * 
 * Lightweight IDE for the nomain programming language
 */

#ifndef NOMAIN_IDE_H
#define NOMAIN_IDE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct NomainIDE NomainIDE;
typedef struct TextEditor TextEditor;
typedef struct OutputWindow OutputWindow;
typedef struct IDEWindows IDEWindows;

// IDE Configuration
#define IDE_WINDOW_WIDTH    900
#define IDE_WINDOW_HEIGHT   600
#define EDITOR_HEIGHT       400
#define OUTPUT_HEIGHT       180
#define TOOLBAR_HEIGHT      30
#define STATUS_BAR_HEIGHT   20

// Color scheme for nomain syntax
#define COLOR_KEYWORD       0x569CD6  // Blue
#define COLOR_COMMENT       0x6A9955  // Green
#define COLOR_STRING        0xCE9178  // Orange
#define COLOR_NUMBER        0xB5CEA8  // Light green
#define COLOR_OPERATOR      0xD4D4D4  // Light gray
#define COLOR_IDENTIFIER    0xDCDCAA  // Yellow
#define COLOR_BACKGROUND    0x1E1E1E  // Dark gray
#define COLOR_TEXT          0xD4D4D4  // Light gray
#define COLOR_LINE_NUMBER   0x858585  // Gray
#define COLOR_SELECTION     0x264F78  // Blue selection

// Main IDE structure
struct NomainIDE {
    // GUI components (using QARMA window system)
    IDEWindows* windows;            // Window system integration
    TextEditor* editor;             // Main text editor
    OutputWindow* output;           // Compiler output window
    
    // File management
    char* current_file;             // Current file path
    bool modified;                  // File has unsaved changes
    
    // Compiler state
    bool compiling;                 // Compilation in progress
    int error_count;                // Number of errors
    int warning_count;              // Number of warnings
    
    // UI state
    bool show_line_numbers;         // Display line numbers
    bool show_output;               // Output window visible
    int font_size;                  // Editor font size
};

// ============================================================================
// IDE Management
// ============================================================================

/**
 * Initialize the nomain IDE
 * Returns: IDE instance or NULL on failure
 */
NomainIDE* nomain_ide_init(void);

/**
 * Run the IDE main loop
 */
void nomain_ide_run(NomainIDE* ide);

/**
 * Cleanup and shutdown IDE
 */
void nomain_ide_shutdown(NomainIDE* ide);

// ============================================================================
// File Operations
// ============================================================================

/**
 * Create a new file
 */
bool nomain_ide_file_new(NomainIDE* ide);

/**
 * Open an existing file
 */
bool nomain_ide_file_open(NomainIDE* ide, const char* filename);

/**
 * Save current file
 */
bool nomain_ide_file_save(NomainIDE* ide);

/**
 * Save current file with new name
 */
bool nomain_ide_file_save_as(NomainIDE* ide, const char* filename);

// ============================================================================
// Compiler Operations
// ============================================================================

/**
 * Compile current file
 */
bool nomain_ide_compile(NomainIDE* ide);

/**
 * Compile and run current file
 */
bool nomain_ide_compile_and_run(NomainIDE* ide);

/**
 * Stop running program
 */
void nomain_ide_stop_program(NomainIDE* ide);

// ============================================================================
// UI Operations
// ============================================================================

/**
 * Toggle output window visibility
 */
void nomain_ide_toggle_output(NomainIDE* ide);

/**
 * Toggle line numbers
 */
void nomain_ide_toggle_line_numbers(NomainIDE* ide);

/**
 * Update status bar
 */
void nomain_ide_update_status(NomainIDE* ide, const char* message);

/**
 * Display error message
 */
void nomain_ide_show_error(NomainIDE* ide, const char* message);

#endif // NOMAIN_IDE_H
