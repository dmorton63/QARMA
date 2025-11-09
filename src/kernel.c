#include "kernel.h"
#include "cpu_core_manager.h"
#include "keyboard_subsystem.h"
#include "video_subsystem.h"
#include "window_subsystem.h"  // Add window subsystem
#include "priority_scheduler.h"
#include "wintest.h"
#include "mesa_bootstrap.h"  // For mesa_shutdown_requested()
#include "log.h"             // For LOG macro
#include <stdio.h>
#include <stdbool.h>  // For system_running
#include <unistd.h>   // For sleep() or usleep() if needed

int main(void) {
    printf("WELCOME TO QARMA OS Version 1.1\n");
    LOG("Initializing core manager...\n");
    core_manager_init();

    // Initialize subsystems and managers
    LOG("Initializing subsystems...\n");
    keyboard_subsystem_init();  // Core assignment handled in subsystem
    video_subsystem_init();     // Core assignment handled in subsystem  
    window_subsystem_init();    // Core assignment handled in subsystem

    // Note: Window is created automatically by video_subsystem_init() now

    // Assign default handlers for all cores
    LOG("Loading dispatch table...\n");
    load_dispatch_table();
    test_dispatch_table(2);

    LOG("Starting main event loop...\n");
    printf("Click the [Shutdown OS] button or press ESC to exit.\n");
    
    bool system_running = true;
    int loop_count = 0;
    
    while (system_running) {
        dispatch_by_priority();  // ← This is the hook
        usleep(100000);  // 100ms delay
        
        // Check for shutdown via mouse/keyboard
        if (mesa_shutdown_requested()) {
            printf("Shutdown requested via GUI, shutting down system.\n");
            dispatch_log("Shutdown requested via GUI, shutting down system.");
            system_running = false;
            break;
        }
        
        // Only check keyboard exhaustion after some time to let the system initialize
        loop_count++;
        if (loop_count > 100 && keyboard_input_exhausted()) {
            // Allow keyboard exhaustion to shutdown only after 10 seconds of running
            LOG("No keyboard activity detected after startup period.\n");
            LOG("Use mouse or ESC key to interact with the system.\n");
            // Don't shutdown automatically - wait for user interaction
            // system_running = false;
        }
        
        // Add a heartbeat every 5 seconds to show the system is alive
        if (loop_count % 50 == 0) {
            LOG("QARMA OS running... (loop %d)\n", loop_count);
        }
    }

    printf("QARMA OS shutting down...\n");
    return 0;
}