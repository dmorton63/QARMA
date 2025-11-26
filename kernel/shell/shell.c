#include "shell.h"
#include "graphics/graphics.h"
#include "graphics/framebuffer.h"
#include "keyboard/command.h"
#include "core/string.h"
#include "keyboard/keyboard.h"
#include "core/input/mouse.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_win_handle.h"

shell_state_t g_shell_state = { .current_path = "/", .initialized = false };

void shell_init(void) {
    if (g_shell_state.initialized) {
        return;
    }
    
    strcpy(g_shell_state.current_path, "/");
    g_shell_state.initialized = true;
    
    SERIAL_LOG("[SHELL] Initializing shell\n");
    gfx_print("QARMA Shell Initialized\n");
    gfx_print("Type 'help' for available commands\n\n");
    SERIAL_LOG("[SHELL] Showing prompt\n");
    show_prompt(": ");
    SERIAL_LOG("[SHELL] Init complete\n");
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
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'Z', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    SERIAL_LOG("[SHELL] Entering main loop\n");
    
    // Test if interrupts are enabled
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    if (rflags & (1 << 9)) {
        SERIAL_LOG("[SHELL] Interrupts are ENABLED\n");
    } else {
        SERIAL_LOG("[SHELL] ERROR: Interrupts are DISABLED!\n");
    }
    
    // **IMPORTANT**: Shell is now DEPRECATED - console compositor handles all input
    // This loop only exists for backwards compatibility and should do nothing
    // All keyboard input is routed through qarma_input_events to the console window
    
    while (1) {
        // Just sleep - compositor handles everything
        extern void sleep_ms(uint32_t ms);
        sleep_ms(100);
        
        // DON'T poll keyboard or consume input - compositor handles it
        // DON'T check kb_state->command_ready - it interferes with console
        
        // Yield to scheduler to allow task switches (enables quantum AI observation)
        extern void task_yield(void);
        task_yield();
    }
}