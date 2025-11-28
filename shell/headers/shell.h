#ifndef SHELL_H
#define SHELL_H

#include "kernel_types.h"

// Shell functions
void shell_init(void);
void show_prompt(const char* path);
void process_command(const char* command);
void screen_printf(const char* format, ...);

void shell_run(void);

// Shell state
typedef struct {
    char current_path[256];
    bool initialized;
} shell_state_t;

extern shell_state_t g_shell_state;

#endif // SHELL_H