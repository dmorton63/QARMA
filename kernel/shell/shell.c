#include "shell.h"
#include "graphics/graphics.h"
#include "keyboard/command.h"
#include "core/string.h"
#include "keyboard/keyboard.h"

shell_state_t g_shell_state = { .current_path = "/", .initialized = false };

void shell_init(void) {
    if (g_shell_state.initialized) {
        return;
    }
    
    strcpy(g_shell_state.current_path, "/");
    g_shell_state.initialized = true;
    
    gfx_print("QARMA Shell Initialized\n");
    gfx_print("Type 'help' for available commands\n\n");
    show_prompt(": ");
}

void show_prompt(const char* path) {

    char prompt[256];
        sprintf(prompt, "[Qarma OS]%s", path);
        gfx_print(prompt);
}

void process_command(const char* command) {
    if (!command || strlen(command) == 0) {
        return;
    }
    
    // Execute the command using the command system
    execute_command(command);
}

void screen_printf(const char* format, ...) {
    // Simple implementation - just pass to gfx_print for now
    // In a full implementation, this would handle printf-style formatting
    gfx_print(format);
}

// Printf-style function for commands

// Simple printf implementation declaration (implemented in shell.c)
extern void gfx_printf(const char* format, ...);
// Screen manipulation functions are provided by graphics subsystem

void screen_put_char(char c) {
    gfx_putchar(c);
}

void shell_run(void) {
    while (1) {
    struct keyboard_state* kb_state = get_keyboard_state();        
        if (kb_state->command_ready) {
            // Null-terminate the buffer
            kb_state->input_buffer[kb_state->buffer_tail] = '\0';

            // Echo the command
            gfx_print("\n");
            gfx_print("Command received: ");
            gfx_print(kb_state->input_buffer);
            gfx_print("\n");

            // Process the command
            process_command(kb_state->input_buffer);

            // Reset buffer
            kb_state->buffer_head = 0;
            kb_state->buffer_tail = 0;
            kb_state->buffer_count = 0;
            kb_state->command_ready = false;

            // Show prompt again
            show_prompt(g_shell_state.current_path);
        }

        //__asm__ volatile("hlt");
    }
}