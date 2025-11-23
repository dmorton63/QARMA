#pragma once

#include "multiboot.h"

/**
 * QARMA System Initialization Module
 * 
 * This module handles all system initialization tasks that were previously
 * in kernel.c, reducing the kernel's footprint and improving organization.
 */

// Initialize all core subsystems
void qarma_init_core_subsystems(void);

// Initialize memory management
void qarma_init_memory(multiboot_info_t* mbi);

// Initialize graphics and video
void qarma_init_graphics(multiboot_info_t* mbi);

// Initialize filesystems
void qarma_init_filesystems(void);

// Initialize CPU and interrupts
void qarma_init_cpu(void);

// Initialize input devices (keyboard, mouse)
void qarma_init_input(void);

// Initialize window manager and GUI
void qarma_init_gui(void);

// Display boot messages window
void qarma_show_boot_messages(void);

// Run the login screen
void qarma_run_login_screen(void (*on_success)(const char* username));

// Run the desktop loop
void qarma_run_desktop(void);

// Full system initialization (calls all of the above)
void qarma_init_all(uint32_t magic, multiboot_info_t* mbi);
