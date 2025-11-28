/**
 * Nomain IDE - Minimal Test Stub
 * 
 * Basic test implementation to verify window creation
 */

#include "nomain_ide.h"
#include "graphics.h"
#include "qarma_win_handle.h"
#include "qarma_window_manager.h"
#include "qarma_win_factory.h"
#include "qarma_input_events.h"
#include "memory.h"
#include "config.h"
#include "keyboard.h"
#include "nomain_syntax.h"
#include "file_subsystem/file_subsystem/file_subsystem.h"
#include "vfs.h"

// IDE state
static char editor_buffer[4096];  // Increased buffer size
static int cursor_pos = 0;
static int cursor_line = 0;
static int cursor_col = 0;
static bool ide_running = false;
static char current_filename[256] = {0};
static bool file_modified = false;

// Filename prompt state
typedef enum {
    PROMPT_NONE,
    PROMPT_NEW,
    PROMPT_OPEN,
    PROMPT_SAVE
} prompt_mode_t;

static prompt_mode_t prompt_mode = PROMPT_NONE;
static char filename_input[256] = {0};
static int filename_input_pos = 0;

// Forward declarations
static void ide_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data);
static void ide_redraw_editor(void);
static void ide_new_file(void);
static void ide_open_file(const char* filename);
static void ide_save_file(void);
static void ide_save_file_as(const char* filename);

// IDE keyboard event handler
static void ide_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data) {
    static int ide_handler_log = 0;
    if (ide_handler_log < 10) {
        SERIAL_LOG("[IDE_HANDLER] Called\n");
        ide_handler_log++;
    }
    
    // Only process if IDE has focus
    extern void* keyboard_get_focus(void);
    if (keyboard_get_focus() == NULL) {
        if (ide_handler_log < 15) {
            SERIAL_LOG("[IDE_HANDLER] IDE does not have focus\n");
        }
        return;
    }
    
    // Only process KEY_DOWN events
    if (event->type != QARMA_INPUT_EVENT_KEY_DOWN) {
        return;
    }
    
    uint8_t scancode = (uint8_t)event->data.key.scancode;
    char character = (char)event->data.key.character;
    uint32_t modifiers = event->data.key.modifiers;
    bool ctrl_pressed = (modifiers & QARMA_MOD_CTRL) != 0;
    
    SERIAL_LOG("[IDE] Key event: scancode=0x");
    extern void serial_debug_hex(uint32_t value);
    serial_debug_hex(scancode);
    SERIAL_LOG(" modifiers=0x");
    serial_debug_hex(modifiers);
    SERIAL_LOG(" ctrl=");
    SERIAL_LOG(ctrl_pressed ? "YES" : "NO");
    SERIAL_LOG("\n");
    
    // If we're prompting for filename, handle that separately
    if (prompt_mode != PROMPT_NONE) {
        // ESC cancels
        if (scancode == 0x01) {
            prompt_mode = PROMPT_NONE;
            filename_input[0] = '\0';
            filename_input_pos = 0;
            ide_redraw_editor();
            event->handled = true;
            return;
        }
        
        // Enter confirms
        if (scancode == 0x1C) {
            if (filename_input_pos > 0) {
                filename_input[filename_input_pos] = '\0';
                
                // Execute appropriate action based on mode
                if (prompt_mode == PROMPT_NEW) {
                    // Create new file with this name
                    ide_new_file();
                    strcpy(current_filename, filename_input);
                } else if (prompt_mode == PROMPT_OPEN) {
                    // Open the specified file
                    ide_open_file(filename_input);
                } else if (prompt_mode == PROMPT_SAVE) {
                    // Save with the entered filename
                    ide_save_file_as(filename_input);
                }
                
                prompt_mode = PROMPT_NONE;
                filename_input[0] = '\0';
                filename_input_pos = 0;
                ide_redraw_editor();
            }
            event->handled = true;
            return;
        }
        
        // Backspace
        if (scancode == 0x0E && filename_input_pos > 0) {
            filename_input_pos--;
            filename_input[filename_input_pos] = '\0';
            ide_redraw_editor();
            event->handled = true;
            return;
        }
        
        // Regular characters
        if (character != 0 && filename_input_pos < 255) {
            filename_input[filename_input_pos++] = character;
            filename_input[filename_input_pos] = '\0';
            ide_redraw_editor();
            event->handled = true;
            return;
        }
        
        event->handled = true;
        return;
    }
    
    // Handle Ctrl shortcuts
    if (ctrl_pressed) {
        SERIAL_LOG("[IDE] Ctrl detected with scancode=0x");
        serial_debug_hex(scancode);
        SERIAL_LOG("\n");
        
        // Ctrl+N - New file (prompt for filename)
        if (scancode == 0x31) {  // N key
            SERIAL_LOG("[IDE] Ctrl+N pressed - Prompt for new filename\n");
            prompt_mode = PROMPT_NEW;
            filename_input[0] = '\0';
            filename_input_pos = 0;
            ide_redraw_editor();
            event->handled = true;
            return;
        }
        // Ctrl+S - Save file
        if (scancode == 0x1F) {  // S key
            SERIAL_LOG("[IDE] Ctrl+S pressed - Save file\n");
            ide_save_file();
            event->handled = true;
            return;
        }
        // Ctrl+O - Open file (prompt for filename)
        if (scancode == 0x18) {  // O key
            SERIAL_LOG("[IDE] Ctrl+O pressed - Prompt for filename to open\n");
            prompt_mode = PROMPT_OPEN;
            filename_input[0] = '\0';
            filename_input_pos = 0;
            ide_redraw_editor();
            event->handled = true;
            return;
        }
        
        // If Ctrl is pressed but not a recognized combination, don't insert character
        event->handled = true;
        return;
    }
    
    // ESC key to exit
    if (scancode == 0x01) {
        SERIAL_LOG("[IDE] ESC pressed, exiting IDE\n");
        ide_running = false;
        
        // Restore focus to shell
        extern void keyboard_set_focus(void* target);
        keyboard_set_focus(NULL);
        SERIAL_LOG("[IDE] Focus restored to shell\n");
        
        // Clear the screen to remove IDE window
        extern void gfx_clear_screen(void);
        gfx_clear_screen();
        
        // Redisplay the shell prompt
        extern void show_prompt(const char* path);
        show_prompt("/");
        
        event->handled = true;
        return;
    }
    
    // Handle backspace
    if (scancode == 0x0E && cursor_pos > 0) {
        cursor_pos--;
        editor_buffer[cursor_pos] = '\0';
        cursor_col--;
        if (cursor_col < 0) cursor_col = 0;
        file_modified = true;
        ide_redraw_editor();
        event->handled = true;
        return;
    }
    
    // Handle enter
    if (scancode == 0x1C) {
        if (cursor_pos < 4095) {
            editor_buffer[cursor_pos++] = '\n';
            cursor_line++;
            cursor_col = 0;
            file_modified = true;
            ide_redraw_editor();
        }
        event->handled = true;
        return;
    }
    
    // Handle regular characters
    if (character != 0 && cursor_pos < 4095) {
        editor_buffer[cursor_pos++] = character;
        cursor_col++;
        file_modified = true;
        ide_redraw_editor();
        event->handled = true;
    }
}

/**
 * Launch the IDE (test version)
 */
void ide_launch(void) {
    SERIAL_LOG("[IDE] Launching Nomain IDE...\n");
    gfx_print("Starting Nomain IDE...\n");
    
    // Create a simple test window
    QARMA_WIN_HANDLE* ide_window = qarma_win_create(
        QARMA_WIN_TYPE_CUSTOM,
        "Nomain IDE",
        QARMA_FLAG_VISIBLE
    );
    
    if (!ide_window) {
        SERIAL_LOG("[IDE] Failed to create IDE window\n");
        gfx_print("ERROR: Failed to create IDE window\n");
        return;
    }
    
    SERIAL_LOG("[IDE] Window created successfully\n");
    
    // Set window position - use BOTH x,y and position for compatibility
    ide_window->x = 50;
    ide_window->y = 50;
    ide_window->position.x = 50;
    ide_window->position.y = 50;
    
    SERIAL_LOG("[IDE] Window position set to (50, 50)\n");
    SERIAL_LOG("[IDE] Window size: ");
    extern void serial_debug_decimal(uint32_t value);
    serial_debug_decimal(ide_window->size.width);
    SERIAL_LOG(" x ");
    serial_debug_decimal(ide_window->size.height);
    SERIAL_LOG("\n");
    
    // Clear existing pixel buffer to dark background
    int buffer_size = ide_window->size.width * ide_window->size.height;
    if (ide_window->pixel_buffer) {
        for (int i = 0; i < buffer_size; i++) {
            ide_window->pixel_buffer[i] = 0x1E1E1E;  // Dark gray
        }
        SERIAL_LOG("[IDE] Pixel buffer cleared\n");
    } else {
        SERIAL_LOG("[IDE] ERROR: No pixel buffer allocated by window factory\n");
        gfx_print("ERROR: Window has no pixel buffer\n");
        return;
    }
    
    // Draw some test text
    gfx_print("\n");
    gfx_print("╔════════════════════════════════════════╗\n");
    gfx_print("║         Nomain IDE v0.1                ║\n");
    gfx_print("║                                        ║\n");
    gfx_print("║  Lightweight IDE for nomain language  ║\n");
    gfx_print("║                                        ║\n");
    gfx_print("║  Window created successfully!          ║\n");
    gfx_print("║                                        ║\n");
    gfx_print("║  Press ESC to close (not implemented) ║\n");
    gfx_print("║  Use 'shutdown' command to exit       ║\n");
    gfx_print("╚════════════════════════════════════════╝\n");
    gfx_print("\n");
    
    SERIAL_LOG("[IDE] Test window displayed\n");
    gfx_print("IDE window created with red border\n");
    
    // Draw some visible content to the window buffer
    uint32_t* buf = ide_window->pixel_buffer;
    int w = ide_window->size.width;
    int h = ide_window->size.height;
    
    // Draw a VERY visible pattern
    uint32_t red = 0x00FF0000;      // Bright red (BGR format)
    uint32_t green = 0x0000FF00;    // Bright green
    uint32_t blue = 0x000000FF;     // Bright blue
    uint32_t white = 0x00FFFFFF;    // White
    
    // Fill entire window with bright red to make it impossible to miss
    for (int i = 0; i < w * h; i++) {
        buf[i] = red;
    }
    
    // Draw white diagonal lines to confirm it's rendering
    for (int i = 0; i < (w < h ? w : h); i++) {
        if (i < w && i < h) {
            buf[i * w + i] = white;  // Top-left to bottom-right
            buf[i * w + (w - 1 - i)] = white;  // Top-right to bottom-left
        }
    }
    
    // Draw thick green border to be extra obvious
    for (int i = 0; i < 5; i++) {
        for (int x = 0; x < w; x++) {
            if (i < h) buf[i * w + x] = green;
            if (h - 1 - i >= 0 && h - 1 - i < h) buf[(h - 1 - i) * w + x] = green;
        }
        for (int y = 0; y < h; y++) {
            if (i < w) buf[y * w + i] = green;
            if (w - 1 - i >= 0 && w - 1 - i < w) buf[y * w + (w - 1 - i)] = green;
        }
    }
    
    SERIAL_LOG("[IDE] Window in buffer, checking window manager...\n");
    
    // Check if window is in the window manager
    extern QARMA_WINDOW_MANAGER qarma_window_manager;
    SERIAL_LOG("[IDE] Window manager has ");
    extern void serial_debug_decimal(uint32_t value);
    serial_debug_decimal(qarma_window_manager.count);
    SERIAL_LOG(" windows\n");
    
    // Verify our window is visible
    SERIAL_LOG("[IDE] Window flags: ");
    extern void serial_debug_hex(uint32_t value);
    serial_debug_hex(ide_window->flags);
    SERIAL_LOG("\n");
    
    // Switch to graphics mode and draw IDE window
    extern uint32_t* fb_ptr;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    
    SERIAL_LOG("[IDE] Switching to graphics mode\n");
    
    // Clear screen to dark background
    if (fb_ptr) {
        for (uint32_t i = 0; i < fb_width * fb_height; i++) {
            fb_ptr[i] = 0x002A2A2E;  // Dark gray background
        }
    }
    
    // Draw IDE window at (100, 60) with size 600x480
    int win_x = 100;
    int win_y = 60;
    int win_w = 600;
    int win_h = 480;
    
    uint32_t title_bar_color = 0x003A3A3E;
    uint32_t body_color = 0x001E1E1E;
    uint32_t border_color = 0x00505050;
    uint32_t text_color = 0x00FFFFFF;
    
    // Draw window body
    for (int y = win_y; y < win_y + win_h && y < (int)fb_height; y++) {
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = body_color;
        }
    }
    
    // Draw title bar (30px high)
    for (int y = win_y; y < win_y + 30 && y < (int)fb_height; y++) {
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = title_bar_color;
        }
    }
    
    // Draw title text
    extern void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text, rgb_color_t fg, rgb_color_t bg);
    rgb_color_t title_fg = {220, 220, 220, 255};  // Light gray text
    rgb_color_t title_bg = {58, 58, 62, 255};     // Dark gray background
    fb_draw_text_with_bg(win_x + 10, win_y + 8, "Nomain IDE v0.1", title_fg, title_bg);
    
    // Draw border (2px)
    for (int i = 0; i < 2; i++) {
        // Top and bottom
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            if (win_y + i < (int)fb_height) 
                fb_ptr[(win_y + i) * fb_width + x] = border_color;
            if (win_y + win_h - 1 - i < (int)fb_height && win_y + win_h - 1 - i >= 0)
                fb_ptr[(win_y + win_h - 1 - i) * fb_width + x] = border_color;
        }
        // Left and right
        for (int y = win_y; y < win_y + win_h && y < (int)fb_height; y++) {
            if (win_x + i < (int)fb_width)
                fb_ptr[y * fb_width + (win_x + i)] = border_color;
            if (win_x + win_w - 1 - i < (int)fb_width && win_x + win_w - 1 - i >= 0)
                fb_ptr[y * fb_width + (win_x + win_w - 1 - i)] = border_color;
        }
    }
    
    // Draw menu bar
    int menu_bar_height = 24;
    int menu_y = win_y + 30;  // Below title bar
    
    // Menu bar background
    for (int y = menu_y; y < menu_y + menu_bar_height && y < (int)fb_height; y++) {
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = 0x00F0F0F0;  // Light gray menu bar
        }
    }
    
    // Draw menu labels with text
    const char* menus[] = {"File", "Edit", "Build", "View", "Help"};
    int menu_x = win_x + 8;
    
    extern void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text, rgb_color_t fg, rgb_color_t bg);
    
    rgb_color_t menu_fg = {0, 0, 0, 255};        // Black text
    rgb_color_t menu_bg = {240, 240, 240, 255};  // Light gray background
    
    for (int m = 0; m < 5; m++) {
        int text_y = menu_y + 6;  // Center text vertically in menu bar
        
        // Draw the menu text
        fb_draw_text_with_bg(menu_x, text_y, menus[m], menu_fg, menu_bg);
        
        // Calculate width based on text length (8 pixels per char + padding)
        int label_width = 0;
        for (const char* p = menus[m]; *p; p++) label_width += 8;
        label_width += 16;  // Add padding
        
        menu_x += label_width;
    }
    
    // Draw editor area separator line
    int editor_top = menu_y + menu_bar_height;
    for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
        if (editor_top < (int)fb_height) {
            fb_ptr[editor_top * fb_width + x] = 0x00505050;
        }
    }
    
    // Draw status bar at bottom
    int status_bar_height = 24;
    int status_y = win_y + win_h - status_bar_height;
    
    for (int y = status_y; y < win_y + win_h && y < (int)fb_height; y++) {
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = 0x00007ACC;  // Blue status bar
        }
    }
    
    // Draw status bar text
    rgb_color_t status_fg = {255, 255, 255, 255};  // White text
    rgb_color_t status_bg = {0, 122, 204, 255};    // Blue background
    fb_draw_text_with_bg(win_x + 10, status_y + 6, "Ready", status_fg, status_bg);
    fb_draw_text_with_bg(win_x + win_w - 150, status_y + 6, "Line 1, Col 1", status_fg, status_bg);
    
    // Draw separator line above status bar
    for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
        if (status_y - 1 < (int)fb_height) {
            fb_ptr[(status_y - 1) * fb_width + x] = 0x00505050;
        }
    }
    
    SERIAL_LOG("[IDE] IDE window with menu bar rendered\n");
    SERIAL_LOG("[IDE] Menu bar has 5 menus: File, Edit, Build, View, Help\n");
    
    // Draw welcome text in editor area
    int text_y = editor_top + 20;
    int text_x = win_x + 20;
    
    rgb_color_t editor_fg = {106, 153, 85, 255};   // Green comment color
    rgb_color_t editor_bg = {30, 30, 30, 255};     // Dark editor background
    
    fb_draw_text_with_bg(text_x, text_y, "// Welcome to Nomain IDE", editor_fg, editor_bg);
    fb_draw_text_with_bg(text_x, text_y + 16, "// Type to start editing. Press ESC to exit.", editor_fg, editor_bg);
    fb_draw_text_with_bg(text_x, text_y + 32, "//", editor_fg, editor_bg);
    fb_draw_text_with_bg(text_x, text_y + 48, "// Nomain: No 'main' function required!", editor_fg, editor_bg);
    
    // Simple text editor variables
    char editor_buffer[1000] = {0};
    int cursor_pos = 0;
    int cursor_line = 0;
    int cursor_col = 0;
    bool running = true;
    
    // Editor coordinates
    int editor_x = win_x + 20;
    int editor_y = editor_top + 100;  // Below welcome text
    
    SERIAL_LOG("[IDE] Entering editor event loop\n");
    
    // Register keyboard event listener for IDE
    QARMA_INPUT_EVENT_LISTENER* ide_listener = qarma_input_event_listen(
        QARMA_INPUT_EVENT_KEY_DOWN, 
        ide_keyboard_handler, 
        NULL, 
        200  // Higher priority than shell (100) so IDE gets events first
    );
    SERIAL_LOG("[IDE] IDE keyboard handler registered with priority 200\n");
    
    // Set keyboard focus to this IDE window
    extern void keyboard_set_focus(void* target);
    keyboard_set_focus((void*)1);  // Non-NULL value to indicate IDE has focus
    SERIAL_LOG("[IDE] Keyboard focus set to IDE\n");
    
    // Initialize IDE state
    editor_buffer[0] = '\0';
    cursor_pos = 0;
    cursor_line = 0;
    cursor_col = 0;
    ide_running = true;
    
    SERIAL_LOG("[IDE] IDE launched and ready for input\n");
    SERIAL_LOG("[IDE] Press ESC to exit\n");
    
    // No event loop needed! The kernel's main loop will dispatch events to our handler.
    // The IDE stays active until ESC is pressed, then ide_keyboard_handler sets ide_running=false
    // and cleans up. For now, just return - the IDE window stays visible.
    
    // TODO: Later we'll need a way to detect when ide_running becomes false and clean up
}

// Redraw editor area helper function
static void ide_redraw_editor(void) {
    extern uint32_t* fb_ptr;
    extern uint32_t fb_width;
    extern uint32_t fb_height;
    
    // These need to be accessible - using fixed values for now
    int win_x = 100;
    int win_y = 60;
    int win_w = 600;
    int win_h = 480;
    int editor_x = win_x + 20;
    int editor_top = win_y + 90;
    int editor_y = editor_top + 100;
    int status_y = win_y + win_h - 30;
    uint32_t body_color = 0x001E1E1E;
    rgb_color_t editor_bg = {30, 30, 30, 255};
    
    // Clear editor area
    for (int y = editor_y; y < status_y - 10 && y < (int)fb_height; y++) {
        for (int x = editor_x; x < win_x + win_w - 20 && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = body_color;
        }
    }
    
    // Draw the text with syntax highlighting
    extern void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text, rgb_color_t fg, rgb_color_t bg);
    
    int draw_x = editor_x;
    int draw_y = editor_y;
    int cursor_draw_x = editor_x;
    int cursor_draw_y = editor_y;
    
    // Process buffer line by line for syntax highlighting
    int line_start = 0;
    for (int i = 0; i <= cursor_pos; i++) {
        if (i == cursor_pos || editor_buffer[i] == '\n') {
            // Tokenize this line
            int line_len = i - line_start;
            if (line_len > 0) {
                char line[256];
                if (line_len > 255) line_len = 255;
                for (int j = 0; j < line_len; j++) {
                    line[j] = editor_buffer[line_start + j];
                }
                line[line_len] = '\0';
                
                // Get tokens for this line
                int token_count = 0;
                Token* tokens = nomain_syntax_tokenize(line, &token_count);
                
                // Draw each token with its color
                int token_x = draw_x;
                for (int t = 0; t < token_count; t++) {
                    Token* tok = &tokens[t];
                    
                    // Extract token text
                    char token_text[256];
                    for (int j = 0; j < tok->length && j < 255; j++) {
                        token_text[j] = line[tok->start + j];
                    }
                    token_text[tok->length] = '\0';
                    
                    // Convert color to rgb_color_t
                    rgb_color_t fg_color = {
                        (tok->color >> 16) & 0xFF,  // R
                        (tok->color >> 8) & 0xFF,   // G
                        tok->color & 0xFF,          // B
                        255                         // A
                    };
                    
                    // Draw each character of the token
                    for (int c = 0; c < tok->length; c++) {
                        char ch[2] = {token_text[c], '\0'};
                        fb_draw_text_with_bg((uint32_t)token_x, (uint32_t)draw_y, ch, fg_color, editor_bg);
                        token_x += 8;
                    }
                }
                
                // Update cursor position if this is the cursor line
                if (i == cursor_pos) {
                    cursor_draw_x = token_x;
                    cursor_draw_y = draw_y;
                }
                
                nomain_syntax_free_tokens(tokens);
            } else if (i == cursor_pos) {
                // Cursor at start of empty line
                cursor_draw_x = draw_x;
                cursor_draw_y = draw_y;
            }
            
            if (i < cursor_pos && editor_buffer[i] == '\n') {
                draw_y += 16;
                draw_x = editor_x;
                line_start = i + 1;
            }
        }
    }
    
    // Draw cursor
    uint32_t cursor_color = 0x00FFFFFF;
    for (int x = cursor_draw_x; x < cursor_draw_x + 8 && x < (int)fb_width; x++) {
        if (cursor_draw_y + 14 < (int)fb_height) {
            fb_ptr[(cursor_draw_y + 14) * fb_width + x] = cursor_color;
        }
    }
    
    // Update status bar
    for (int y = status_y; y < win_y + win_h && y < (int)fb_height; y++) {
        for (int x = win_x; x < win_x + win_w && x < (int)fb_width; x++) {
            fb_ptr[y * fb_width + x] = 0x00007ACC;
        }
    }
    
    char status_text[50];
    const char* status_prefix = "Line ";
    int idx = 0;
    for (const char* p = status_prefix; *p; p++) status_text[idx++] = *p;
    status_text[idx++] = '0' + ((cursor_line + 1) / 10);
    status_text[idx++] = '0' + ((cursor_line + 1) % 10);
    status_text[idx++] = ',';
    status_text[idx++] = ' ';
    status_text[idx++] = 'C';
    status_text[idx++] = 'o';
    status_text[idx++] = 'l';
    status_text[idx++] = ' ';
    status_text[idx++] = '0' + ((cursor_col + 1) / 10);
    status_text[idx++] = '0' + ((cursor_col + 1) % 10);
    status_text[idx] = '\0';
    
    rgb_color_t status_fg = {255, 255, 255, 255};
    rgb_color_t status_bg = {0, 122, 204, 255};
    extern void fb_draw_text_with_bg(uint32_t x, uint32_t y, const char *text, rgb_color_t fg, rgb_color_t bg);
    
    // Show filename prompt or regular status
    if (prompt_mode != PROMPT_NONE) {
        // Show filename input prompt
        char prompt_text[300];
        int p_idx = 0;
        
        // Show appropriate prompt based on mode
        const char* prompt_msg;
        if (prompt_mode == PROMPT_NEW) {
            prompt_msg = "New file: ";
        } else if (prompt_mode == PROMPT_OPEN) {
            prompt_msg = "Open file: ";
        } else {
            prompt_msg = "Save as: ";
        }
        
        for (const char* p = prompt_msg; *p; p++) {
            prompt_text[p_idx++] = *p;
        }
        for (int i = 0; i < filename_input_pos && p_idx < 290; i++) {
            prompt_text[p_idx++] = filename_input[i];
        }
        // Add cursor
        prompt_text[p_idx++] = '_';
        prompt_text[p_idx] = '\0';
        
        fb_draw_text_with_bg((uint32_t)(win_x + 10), (uint32_t)(status_y + 6), prompt_text, status_fg, status_bg);
        
        const char* help_text = "Enter=Confirm  ESC=Cancel";
        fb_draw_text_with_bg((uint32_t)(win_x + win_w - 200), (uint32_t)(status_y + 6), help_text, status_fg, status_bg);
    } else {
        // Show filename and modified status
        char file_status[128];
        int file_idx = 0;
        if (current_filename[0] != '\0') {
            for (const char* p = current_filename; *p && file_idx < 100; p++) {
                file_status[file_idx++] = *p;
            }
        } else {
            const char* untitled = "untitled.nom";
            for (const char* p = untitled; *p; p++) {
                file_status[file_idx++] = *p;
            }
        }
        if (file_modified) {
            file_status[file_idx++] = ' ';
            file_status[file_idx++] = '*';
        }
        file_status[file_idx] = '\0';
        
        fb_draw_text_with_bg((uint32_t)(win_x + 10), (uint32_t)(status_y + 6), file_status, status_fg, status_bg);
        fb_draw_text_with_bg((uint32_t)(win_x + win_w - 150), (uint32_t)(status_y + 6), status_text, status_fg, status_bg);
    }
}

// ============================================================================
// File Operations
// ============================================================================

static void ide_new_file(void) {
    // Clear buffer
    for (int i = 0; i < 4096; i++) {
        editor_buffer[i] = '\0';
    }
    cursor_pos = 0;
    cursor_line = 0;
    cursor_col = 0;
    current_filename[0] = '\0';
    file_modified = false;
    
    ide_redraw_editor();
    SERIAL_LOG("[IDE] New file created\n");
}

static void ide_open_file(const char* filename) {
    SERIAL_LOG("[IDE] Opening file: ");
    SERIAL_LOG(filename);
    SERIAL_LOG("\n");
    
    // Clear buffer first for safety
    for (int i = 0; i < 4096; i++) {
        editor_buffer[i] = '\0';
    }
    
    // Use VFS to open the file
    extern vfs_node_t* vfs_open(const char* path);
    vfs_node_t* node = vfs_open(filename);
    if (!node) {
        SERIAL_LOG("[IDE] Failed to open file - file not found\n");
        // Show error in editor
        const char* error_msg = "ERROR: File not found\n\nThe file does not exist.\nUse Ctrl+N to create a new file.";
        int i;
        for (i = 0; error_msg[i] != '\0' && i < 4095; i++) {
            editor_buffer[i] = error_msg[i];
        }
        editor_buffer[i] = '\0';
        cursor_pos = 0;
        cursor_line = 0;
        cursor_col = 0;
        ide_redraw_editor();
        return;
    }
    
    // Safety check: verify node is valid before reading
    if (!node->size || node->size > 4095) {
        SERIAL_LOG("[IDE] File size invalid or too large\n");
        const char* error_msg = "ERROR: File is empty or too large";
        int i;
        for (i = 0; error_msg[i] != '\0' && i < 4095; i++) {
            editor_buffer[i] = error_msg[i];
        }
        editor_buffer[i] = '\0';
        cursor_pos = 0;
        ide_redraw_editor();
        return;
    }
    
    // Read file content into buffer using VFS
    extern int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
    int bytes_read = vfs_read(node, editor_buffer, 4095, 0);
    
    if (bytes_read < 0) {
        SERIAL_LOG("[IDE] Failed to read file\n");
        const char* error_msg = "ERROR: Failed to read file";
        int i;
        for (i = 0; error_msg[i] != '\0' && i < 4095; i++) {
            editor_buffer[i] = error_msg[i];
        }
        editor_buffer[i] = '\0';
        cursor_pos = 0;
        ide_redraw_editor();
        return;
    }
    
    editor_buffer[bytes_read] = '\0';  // Null terminate
    
    // Update IDE state
    strcpy(current_filename, filename);
    cursor_pos = bytes_read;  // Put cursor at end of loaded text
    
    // Count lines and calculate cursor position
    cursor_line = 0;
    cursor_col = 0;
    for (int i = 0; i < bytes_read; i++) {
        if (editor_buffer[i] == '\n') {
            cursor_line++;
            cursor_col = 0;
        } else {
            cursor_col++;
        }
    }
    
    file_modified = false;
    
    ide_redraw_editor();
    SERIAL_LOG("[IDE] File opened successfully, bytes: ");
    char bytes_str[16];
    int idx = 0;
    int temp = bytes_read;
    if (temp == 0) {
        bytes_str[idx++] = '0';
    } else {
        while (temp > 0) {
            bytes_str[idx++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    bytes_str[idx] = '\0';
    // Reverse the string
    for (int i = 0; i < idx / 2; i++) {
        char t = bytes_str[i];
        bytes_str[i] = bytes_str[idx - 1 - i];
        bytes_str[idx - 1 - i] = t;
    }
    SERIAL_LOG(bytes_str);
    SERIAL_LOG("\n");
}

static void ide_save_file(void) {
    if (current_filename[0] == '\0') {
        // No filename yet, prompt for one
        SERIAL_LOG("[IDE] No filename, prompting user\n");
        prompt_mode = PROMPT_SAVE;
        filename_input[0] = '\0';
        filename_input_pos = 0;
        ide_redraw_editor();
        return;
    }
    
    // Have a filename, save directly
    // TODO: Add filesystem_file_exists check when implemented
    SERIAL_LOG("[IDE] Saving to existing filename\n");
    ide_save_file_as(current_filename);
}

static void ide_save_file_as(const char* filename) {
    SERIAL_LOG("[IDE] Saving file as: ");
    SERIAL_LOG(filename);
    SERIAL_LOG("\n");
    
    // Calculate buffer length
    size_t length = 0;
    while (editor_buffer[length] != '\0' && length < 4096) {
        length++;
    }
    
    // Try to create the file using VFS
    extern vfs_node_t* vfs_create(const char* path, uint32_t type);
    vfs_node_t* node = vfs_create(filename, VFS_TYPE_FILE);
    if (!node) {
        SERIAL_LOG("[IDE] Note: vfs_create failed, file may exist, trying vfs_open\n");
        // Try opening existing file
        extern vfs_node_t* vfs_open(const char* path);
        node = vfs_open(filename);
    }
    
    if (!node) {
        SERIAL_LOG("[IDE] Failed to create or open file\n");
        return;
    }
    
    // Write buffer to file using VFS
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    int bytes_written = vfs_write(node, editor_buffer, length, 0);
    
    if (bytes_written >= 0 && (size_t)bytes_written == length) {
        strcpy(current_filename, filename);
        file_modified = false;
        ide_redraw_editor();
        SERIAL_LOG("[IDE] File saved successfully, bytes written: ");
        char bytes_str[16];
        int idx = 0;
        int temp = bytes_written;
        if (temp == 0) {
            bytes_str[idx++] = '0';
        } else {
            while (temp > 0) {
                bytes_str[idx++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        bytes_str[idx] = '\0';
        // Reverse
        for (int i = 0; i < idx / 2; i++) {
            char t = bytes_str[i];
            bytes_str[i] = bytes_str[idx - 1 - i];
            bytes_str[idx - 1 - i] = t;
        }
        SERIAL_LOG(bytes_str);
        SERIAL_LOG("\n");
    } else {
        SERIAL_LOG("[IDE] Failed to write file, result=");
        char result_str[16];
        int idx = 0;
        int temp = bytes_written;
        if (temp < 0) {
            result_str[idx++] = '-';
            temp = -temp;
        }
        if (temp == 0) {
            result_str[idx++] = '0';
        } else {
            while (temp > 0) {
                result_str[idx++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        result_str[idx] = '\0';
        SERIAL_LOG(result_str);
        SERIAL_LOG("\n");
    }
}
