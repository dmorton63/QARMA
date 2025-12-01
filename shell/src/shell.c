#include "shell.h"
#include "config.h"
#include "graphics.h"
#include "framebuffer.h"
#include "command.h"
#include "string.h"
#include "keyboard.h"
#include "input/mouse.h"
#include "qarma_window_manager.h"
#include "qarma_win_handle.h"

shell_state_t g_shell_state = { .current_path = "/", .initialized = false };

void shell_init(void) {
    SERIAL_LOG("[SHELL] shell_init() entry\n");
    if (g_shell_state.initialized) {
        SERIAL_LOG("[SHELL] Already initialized, returning\n");
        return;
    }
    
    SERIAL_LOG("[SHELL] Setting path\n");
    strcpy(g_shell_state.current_path, "/");
    g_shell_state.initialized = true;
    
    SERIAL_LOG("[SHELL] Initializing shell\n");
    // Skip gfx_print for now - might be causing issues
    // SERIAL_LOG("[SHELL] About to call gfx_print #1\n");
    // gfx_print("QARMA Shell Initialized\n");
    // SERIAL_LOG("[SHELL] About to call gfx_print #2\n");
    // gfx_print("Type 'help' for available commands\n\n");
    // SERIAL_LOG("[SHELL] Showing prompt\n");
    // SERIAL_LOG("[SHELL] About to call show_prompt\n");
    // show_prompt(": ");
    SERIAL_LOG("[SHELL] Init complete\n");
}

void show_prompt(const char* path) {
    SERIAL_LOG("[SHELL] show_prompt() called - skipping display (console window will handle)\n");
    // Don't use sprintf or gfx_print - both cause crashes
    // The console window will display the prompt when ready
    // For now, just log to serial
}

void process_command(const char* command) {
    if (!command || strlen(command) == 0) {
        return;
    }
    
    // Execute the command using the command system
    execute_command(command);
}

static void shell_tiny_vformat(char* out, size_t outsz, const char* fmt, va_list ap) {
    size_t pos = 0; const char* p = fmt ? fmt : "";
    while (*p && pos + 1 < outsz) {
        if (*p != '%') { out[pos++] = *p++; continue; }
        p++;
        if (*p == '%') { out[pos++] = '%'; p++; continue; }
        if (*p == 's') { const char* s = va_arg(ap, const char*); if (!s) s = "(null)"; while (*s && pos + 1 < outsz) out[pos++] = *s++; p++; }
        else if (*p == 'c') { int c = va_arg(ap, int); out[pos++] = (char)c; p++; }
        else if (*p == 'u' || *p == 'd') { int is_signed = (*p == 'd'); long v = is_signed ? va_arg(ap, int) : (long)va_arg(ap, unsigned int); if (is_signed && v < 0) { if (pos + 1 < outsz) out[pos++] = '-'; v = -v; } char buf[32]; int bp=0; unsigned long uv=(unsigned long)v; if (uv==0) buf[bp++]='0'; while (uv && bp < (int)sizeof(buf)) { buf[bp++] = (char)('0'+(uv%10)); uv/=10; } for (int i=bp-1;i>=0 && pos + 1 < outsz;--i) out[pos++]=buf[i]; p++; }
        else if (*p == 'x' || *p=='X') { unsigned int v = va_arg(ap, unsigned int); const char* hex = (*p=='X')?"0123456789ABCDEF":"0123456789abcdef"; char buf[8]; int bp=0; if (v==0) buf[bp++]='0'; while (v && bp < (int)sizeof(buf)) { buf[bp++]=hex[v&0xF]; v>>=4; } for (int i=bp-1;i>=0 && pos + 1 < outsz;--i) out[pos++]=buf[i]; p++; }
        else { out[pos++]='%'; if (*p && pos + 1 < outsz) out[pos++]=*p; if (*p) p++; }
    }
    out[pos] = '\0';
}

void screen_printf(const char* format, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, format);
    shell_tiny_vformat(buf, sizeof(buf), format, ap);
    va_end(ap);
    // Print to screen
    gfx_print(buf);
    // And mirror to serial
    serial_logf("[SHELL] %s\n", buf);
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
        extern void task_sleep(uint32_t milliseconds);
        task_sleep(100);
        
        // DON'T poll keyboard or consume input - compositor handles it
        // DON'T check kb_state->command_ready - it interferes with console
        
        // Yield to scheduler to allow task switches (enables quantum AI observation)
        extern void task_yield(void);
        task_yield();
    }
}