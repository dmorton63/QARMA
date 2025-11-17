/**
 * QARMA Kernel - Main Entry Point (Simplified for Keyboard Testing)
 * 
 * This is the main kernel entry point focused on getting keyboard input working.
 */

#include "multiboot.h"
#include "string.h"
#include "config.h"
#include "keyboard/keyboard.h"
#include "shell/shell.h"
#include "graphics/graphics.h"
#include "graphics/subsystem/video_subsystem.h"
#include "core/scheduler/subsystem_registry.h"
#include "graphics/framebuffer.h"
#include "kernel.h"
#include "graphics/popup.h"
#include "graphics/message_box.h"
#include "qarma_win_handle/qarma_win_handle.h"
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_input_events.h"
#include "qarma_win_handle/panic.h"
#include "core/memory.h"
#include "core/memory/memory_pool.h"
#include "core/input/mouse.h"
#include "core/pci.h"
#include "core/scheduler/task_manager.h"
#include "core/scheduler/scheduler_demo.h"
#include "fs/file_subsystem/file_subsystem.h"
#include "fs/vfs.h"
#include "fs/iso9660.h"
#include "graphics/png_decoder.h"
#include "drivers/block/cdrom.h"
#include "core/blockdev.h"
#include "core/memory/heap.h"
#include "drivers/usb/usb_mouse.h"
#include "keyboard/command.h"



// #include "core/memory/vmm/vmm.h"
// #include "core/memory/heap.h"
// #include "core/overlay/overlay.h"
//#include "../scheduler/qarma_window_manager.h"

//#include "../scheduler/qarma_win_handle.h"
#include "splash_app/qarma_splash_app.h"  // Contains splash_app and its functions
#include "qarma_win_handle/login_screen.h"  // Login screen
#include "qarma_win_handle/main_window.h"   // Main desktop window
#include "graphics/png_decoder.h"  // For PNG loading functions
#include "gui/gui.h"  // GUI library for rendering
#include "gui/boot_messages.h"  // Boot messages window

extern QARMA_WIN_HANDLE global_win_handler;
extern QARMA_APP_HANDLE splash_app;


// Function declarations from other modules
void gdt_init(void);
void idt_init(void);  
void interrupts_system_init(void);
void multiboot_parse_info(uint32_t magic, multiboot_info_t* mbi);
uint32_t get_ticks(void);

// Basic types
typedef unsigned int size_t;

// Global verbosity level  
verbosity_level_t g_verbosity = VERBOSITY_VERBOSE;

// I/O port functions (temporary - should move to a dedicated header)
uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Serial debug functions for keyboard and other subsystems
void serial_debug(const char* msg) {
    const char* ptr = msg;
    while (*ptr) {
        // Write to COM1 (0x3F8)
        while ((inb(0x3F8 + 5) & 0x20) == 0);
        outb(0x3F8, *ptr);
        ptr++;
    }
}

void serial_debug_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[9] = "00000000";
    for (int i = 7; i >= 0; i--) {
        buffer[7-i] = hex_chars[(value >> (i * 4)) & 0xF];
    }
    serial_debug(buffer);
}

void serial_debug_decimal(uint32_t value) {
    char buffer[11]; // Max for 32-bit: 4294967295 + null terminator
    char* ptr = buffer + 10;
    *ptr = '\0';
    
    do {
        *--ptr = '0' + (value % 10);
        value /= 10;
    } while (value > 0);
    
    serial_debug(ptr);
}

/**
 * Main kernel loop for keyboard testing
 */
// int kernel_main_loop(void) {
//     gfx_print("\n=== QARMA Interactive Shell ===\n");
//     gfx_print("Keyboard input enabled. Type 'help' for commands.\n\n");
    
//     // Initialize shell interface
//     shell_init();
    
//     // Enable interrupts for keyboard input
//     __asm__ volatile("sti");
    
//     // Interactive loop
//     while (1) {
//         // CPU halt to reduce CPU usage while waiting for input
//         __asm__ volatile("hlt");
//     }
// }


int kernel_splash_test()
{
    // Initialize splash app
    splash_app.init(&splash_app);

    uint32_t last_tick = get_ticks();
    QARMA_TICK_CONTEXT ctx = {
        .tick_count = 0,
        .delta_time = 0.0f,
        .uptime_seconds = 0.0f
    };

    while (true) {
        uint32_t current_tick = get_ticks();
        if (current_tick > last_tick) {
            uint32_t ticks_elapsed = current_tick - last_tick;
            last_tick = current_tick;

            ctx.tick_count += ticks_elapsed;
            ctx.delta_time = (float)ticks_elapsed / (float)QARMA_TICK_RATE;
            ctx.uptime_seconds += ctx.delta_time;

            // Update splash app
            splash_app.update(&splash_app, &ctx);

            // Update all windows via manager
            qarma_window_manager.update_all(&qarma_window_manager, &ctx);

            // Render all windows
            qarma_window_manager.render_all(&qarma_window_manager);

            // Exit when splash window is gone
            if (splash_app.main_window == NULL) {
                break;
            }
        }

        sleep_ms(1);  // Let interrupts breathe
    }
    
    splash_app.shutdown(&splash_app);
    return 0;
}

// Callback for successful login
static void on_login_success(const char* username) {
    SERIAL_LOG("[KERNEL] User logged in: \n");
    gfx_print("Login successful! Welcome, ");
    gfx_print((char*)username);
    gfx_print("\n");
}

/**
 * Main kernel entry point - simplified for keyboard testing
 */
int kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // Early debug output using VGA text mode
    volatile char* vga_buffer = (volatile char*)0xB8000;
    const char* msg = "BOOT: kernel_main started     ";
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_buffer[80*2 + i * 2] = msg[i];      // Second line
        vga_buffer[80*2 + i * 2 + 1] = 0x07;   // White on black
    }

    memory_init();   // Parse multiboot info first to set verbosity level
    
    // Update VGA output
    const char* msg2 = "BOOT: memory_init complete    ";
    for (int i = 0; msg2[i] != '\0'; i++) {
        vga_buffer[160*2 + i * 2] = msg2[i];    // Third line
        vga_buffer[160*2 + i * 2 + 1] = 0x07;
    }
    
    multiboot_parse_info(magic, mbi);
    
    const char* msg3 = "BOOT: multiboot parsed        ";
    for (int i = 0; msg3[i] != '\0'; i++) {
        vga_buffer[240*2 + i * 2] = msg3[i];    // Fourth line
        vga_buffer[240*2 + i * 2 + 1] = 0x07;
    }
    
   // Physical memory manager (frame allocator, memory map)

    // Initialize basic graphics firstma
    const char* msg4 = "BOOT: starting graphics init  ";
    for (int i = 0; msg4[i] != '\0'; i++) {
        vga_buffer[320*2 + i * 2] = msg4[i];    // Fifth line
        vga_buffer[320*2 + i * 2 + 1] = 0x07;
    }
    
    graphics_init(mbi);
    framebuffer_init();
    
    // Initialize subsystem registry
    subsystem_registry_init();
    gfx_print("Subsystem registry initialized.\n");
    
    // Initialize parallel processing engine (needed for core management)
    parallel_engine_init();
    gfx_print("Parallel processing engine initialized.\n");
    
    // Initialize core allocation manager
    extern void core_manager_init(void);
    core_manager_init();
    gfx_print("Core allocation manager initialized.\n");
    
    // Initialize memory pool manager
    extern void memory_pool_init(void);
    memory_pool_init();
    gfx_print("Memory pool manager initialized.\n");
    
    // Initialize execution pipeline system
    extern void pipeline_system_init(void);
    pipeline_system_init();
    gfx_print("Execution pipeline system initialized.\n");
    
    // Initialize video subsystem
    video_subsystem_init(NULL);
    gfx_print("Video subsystem initialized.\n");
    
    // TEST PNG LOADING - moved to early boot
    SERIAL_LOG("===EARLY PNG TEST START===\n");
    gfx_print("===EARLY PNG TEST START===\n");
    png_image_t* early_splash = load_splash_image();
    if (early_splash) {
        SERIAL_LOG("SUCCESS: PNG image loaded and decoded!\n");
        gfx_print("SUCCESS: PNG image loaded and decoded!\n");
        gfx_print("Image dimensions: ");
        // Simple number printing
        uint32_t w = early_splash->width;
        uint32_t h = early_splash->height;
        char buf[20];
        int i = 0;
        if (w == 0) buf[i++] = '0';
        else {
            int digits = 0;
            uint32_t temp = w;
            while (temp > 0) { temp /= 10; digits++; }
            for (int j = digits - 1; j >= 0; j--) {
                temp = w;
                for (int k = 0; k < j; k++) temp /= 10;
                buf[i++] = '0' + (temp % 10);
            }
        }
        buf[i++] = 'x';
        if (h == 0) buf[i++] = '0';
        else {
            int digits = 0;
            uint32_t temp = h;
            while (temp > 0) { temp /= 10; digits++; }
            for (int j = digits - 1; j >= 0; j--) {
                temp = h;
                for (int k = 0; k < j; k++) temp /= 10;
                buf[i++] = '0' + (temp % 10);
            }
        }
        buf[i] = '\0';
        //gfx_print(buf);
        //gfx_print("\n");
        
        // Show memory pool stats while PNG allocation is active
        gfx_print("\n");
        memory_pool_print_all_stats();
        gfx_print("\n");
        
        // Display title showing PNG loaded
        video_subsystem_splash_title("PNG CHECKERBOARD LOADED!", 
                                    (rgb_color_t){255, 255, 0, 255},  // Yellow text
                                    (rgb_color_t){255, 0, 255, 255}); // Magenta bg
        
        png_free(early_splash);
        SERIAL_LOG("PNG test complete - image freed\n");
        gfx_print("PNG test complete - image freed\n");
    } else {
        SERIAL_LOG("FAILED: Could not load PNG image\n");
        gfx_print("FAILED: Could not load PNG image\n");
    }
    SERIAL_LOG("===EARLY PNG TEST END===\n");
    gfx_print("===EARLY PNG TEST END===\n");
    
    // Initialize filesystem subsystem
    SERIAL_LOG("[KERNEL] About to init filesystem subsystem\n");
    filesystem_subsystem_init(NULL);
    SERIAL_LOG("[KERNEL] Filesystem subsystem initialized\n");
    gfx_print("Filesystem subsystem initialized.\n");
    
    SERIAL_LOG("[KERNEL] About to initialize VFS\n");
    gfx_print("DEBUG: About to initialize VFS...\n");
    
    // Initialize VFS and mount RAM disk
    vfs_init();
    SERIAL_LOG("[KERNEL] VFS init completed\n");
    gfx_print("DEBUG: VFS init completed successfully.\n");
    gfx_print("VFS initialized and RAM disk mounted.\n");
    
    // Initialize ISO9660 filesystem (CD-ROM driver will be initialized after PCI)
    SERIAL_LOG("[KERNEL] ===== INITIALIZING ISO9660 FILESYSTEM =====\n");
    iso9660_init();
    SERIAL_LOG("[KERNEL] ISO9660 init completed\n");
    
    gfx_print("=== QARMA v1.0 Starting ===\n");
    gfx_print("Keyboard Testing Version\n");
    
    // Initialize GDT
    gfx_print("Initializing GDT...\n");
    gdt_init();
    
    // Initialize IDT and interrupts
    gfx_print("Initializing IDT and interrupts...\n");
    //idt_init();
    __asm__ volatile("cli");
    interrupts_system_init();
    
    // Initialize keyboard driver
    gfx_print("Initializing keyboard driver...\n");
    //draw_splash("QARMA Keyboard Test");
    keyboard_init();
    // Ensure higher-level keyboard processing is enabled by default so
    // the interactive shell and commands are available. Use the `kbd`
    // command at runtime to toggle processing if needed.
    keyboard_set_enabled(true);
    pci_init();
    gfx_print("Initializing mouse driver...\n");
    usb_mouse_init();
    gfx_print("Mouse driver initialized.\n");
    
    // Initialize QARMA window manager
    gfx_print("Initializing window manager...\n");
    qarma_window_manager_init();
    gfx_print("Window manager initialized.\n");
    
    // Initialize input event system
    gfx_print("Initializing input event system...\n");
    qarma_input_events_init();
    gfx_print("Input event system initialized.\n");
    
    // Enable interrupts before login screen (needed for keyboard input)
    __asm__ volatile("sti");
    SERIAL_LOG("[KERNEL] Interrupts enabled for login screen\n");
    
    // ========================================================================
    // BOOT MESSAGES WINDOW - Show boot sequence to user
    // ========================================================================
    SERIAL_LOG("[KERNEL] ===== CREATING BOOT MESSAGES WINDOW =====\n");
    
    extern BootMessagesWindow* boot_messages_create(int x, int y, int width, int height);
    extern void boot_messages_add(BootMessagesWindow* bmw, const char* message);
    extern void boot_messages_render(BootMessagesWindow* bmw);
    extern void boot_messages_destroy(BootMessagesWindow* bmw);
    
    // Create boot messages window (centered on screen)
    extern FramebufferInfo* fb_info;
    int win_w = 600;
    int win_h = 400;
    int win_x = (fb_info->width - win_w) / 2;
    int win_y = (fb_info->height - win_h) / 2;
    
    BootMessagesWindow* boot_msg_win = boot_messages_create(win_x, win_y, win_w, win_h);
    if (!boot_msg_win) {
        SERIAL_LOG("[KERNEL] Failed to create boot messages window\n");
    } else {
        SERIAL_LOG("[KERNEL] Boot messages window created\n");
        
        // Add boot messages showing what we've done so far
        boot_messages_add(boot_msg_win, "QARMA Boot Sequence");
        boot_messages_add(boot_msg_win, "======================================");
        boot_messages_add(boot_msg_win, "");
        boot_messages_add(boot_msg_win, "[OK] Multiboot information parsed");
        boot_messages_add(boot_msg_win, "[OK] Memory manager initialized");
        boot_messages_add(boot_msg_win, "[OK] Heap allocator ready");
        boot_messages_add(boot_msg_win, "[OK] Framebuffer detected");
        boot_messages_add(boot_msg_win, "[OK] Graphics subsystem initialized");
        boot_messages_add(boot_msg_win, "[OK] Video subsystem ready");
        boot_messages_add(boot_msg_win, "[OK] PNG decoder functional");
        boot_messages_add(boot_msg_win, "[OK] Filesystem subsystem initialized");
        boot_messages_add(boot_msg_win, "[OK] VFS mounted");
        boot_messages_add(boot_msg_win, "[OK] ISO9660 filesystem ready");
        boot_messages_add(boot_msg_win, "[OK] GDT initialized");
        boot_messages_add(boot_msg_win, "[OK] IDT and interrupts configured");
        boot_messages_add(boot_msg_win, "[OK] Keyboard driver loaded");
        boot_messages_add(boot_msg_win, "[OK] PCI subsystem initialized");
        boot_messages_add(boot_msg_win, "[OK] USB mouse driver initialized");
        boot_messages_add(boot_msg_win, "[OK] QARMA window manager started");
        boot_messages_add(boot_msg_win, "[OK] Input event system ready");
        boot_messages_add(boot_msg_win, "");
        boot_messages_add(boot_msg_win, "System initialization complete!");
        boot_messages_add(boot_msg_win, "");
        boot_messages_add(boot_msg_win, "Press TAB to focus close button,");
        boot_messages_add(boot_msg_win, "then ENTER to continue to login.");
        
        // Render the window
        boot_messages_render(boot_msg_win);
        
        // Blit to framebuffer
        uint32_t* fb = (uint32_t*)fb_info->address;
        if (boot_msg_win->main_window && boot_msg_win->main_window->pixel_buffer) {
            uint32_t* win_buffer = boot_msg_win->main_window->pixel_buffer;
            int src_w = boot_msg_win->main_window->size.width;
            int src_h = boot_msg_win->main_window->size.height;
            int src_x = boot_msg_win->main_window->x;
            int src_y = boot_msg_win->main_window->y;
            
            for (int y = 0; y < src_h; y++) {
                for (int x = 0; x < src_w; x++) {
                    int fb_x = src_x + x;
                    int fb_y = src_y + y;
                    if (fb_x >= 0 && fb_x < (int)fb_info->width && 
                        fb_y >= 0 && fb_y < (int)fb_info->height) {
                        fb[fb_y * fb_info->width + fb_x] = win_buffer[y * src_w + x];
                    }
                }
            }
        }
        
        SERIAL_LOG("[KERNEL] Boot messages window rendered\n");
        
        // Wait for user to acknowledge boot complete (TAB then ENTER)
        keyboard_enable_window_mode(true);
        keyboard_set_enabled(false);
        
        SERIAL_LOG("[KERNEL] Waiting for user to close boot messages\n");
        
        bool boot_msg_closed = false;
        while (!boot_msg_closed) {
            key_event_t key_event;
            if (keyboard_get_window_key_event(&key_event)) {
                if (!key_event.released) {
                    // Handle close (Enter when focused)
                    if (key_event.scancode == KEY_ENTER && boot_msg_win->close_button_ctrl.focused) {
                        SERIAL_LOG("[KERNEL] Proceeding to login\n");
                        boot_msg_closed = true;
                        break;
                    }
                    
                    // Handle tab (focus close button)
                    if (key_event.scancode == KEY_TAB) {
                        extern void close_button_set_focus(CloseButton* cb, bool focused);
                        close_button_set_focus(&boot_msg_win->close_button_ctrl, 
                                             !boot_msg_win->close_button_ctrl.focused);
                        boot_messages_render(boot_msg_win);
                        
                        // Re-blit to framebuffer
                        if (boot_msg_win->main_window && boot_msg_win->main_window->pixel_buffer) {
                            uint32_t* win_buffer = boot_msg_win->main_window->pixel_buffer;
                            int src_w = boot_msg_win->main_window->size.width;
                            int src_h = boot_msg_win->main_window->size.height;
                            int src_x = boot_msg_win->main_window->x;
                            int src_y = boot_msg_win->main_window->y;
                            
                            for (int y = 0; y < src_h; y++) {
                                for (int x = 0; x < src_w; x++) {
                                    int fb_x = src_x + x;
                                    int fb_y = src_y + y;
                                    if (fb_x >= 0 && fb_x < (int)fb_info->width && 
                                        fb_y >= 0 && fb_y < (int)fb_info->height) {
                                        fb[fb_y * fb_info->width + fb_x] = win_buffer[y * src_w + x];
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            sleep_ms(16); // ~60fps
        }
        
        // Destroy boot messages window
        boot_messages_destroy(boot_msg_win);
        SERIAL_LOG("[KERNEL] Boot messages window closed\n");
        
        // Clear screen before showing desktop
        memset((void*)fb_info->address, 0, fb_info->pitch * fb_info->height);
    }
    
    // ========================================================================
    // DESKTOP ENVIRONMENT - New Clean Flow
    // ========================================================================
    gfx_print("Starting desktop environment...\n");
    SERIAL_LOG("[KERNEL] ===== STARTING DESKTOP ENVIRONMENT =====\n");
    
    // Enable interrupts for GUI
    __asm__ volatile("sti");
    SERIAL_LOG("[KERNEL] Interrupts enabled for desktop\n");
    
    extern FramebufferInfo* fb_info;
    if (!fb_info || !fb_info->address) {
        SERIAL_LOG("[KERNEL] ERROR: No framebuffer available\n");
        gfx_print("ERROR: No framebuffer available!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Step 1: Create main desktop window (persistent, full-screen with gradient)
    SERIAL_LOG("[KERNEL] Creating main desktop window\n");
    gfx_print("Creating main desktop window...\n");
    
    extern MainWindow* main_window_create(void);
    extern void main_window_render(MainWindow* mw);
    extern bool main_window_should_exit(MainWindow* mw);
    extern void main_window_destroy(MainWindow* mw);
    
    MainWindow* main_win = main_window_create();
    if (!main_win) {
        SERIAL_LOG("[KERNEL] FATAL: Failed to create main window\n");
        gfx_print("FATAL: Failed to create main window\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Render main window once to show gradient
    main_window_render(main_win);
    
    // Blit main window to framebuffer
    uint32_t* fb = (uint32_t*)fb_info->address;
    int fb_w = fb_info->width;
    int fb_h = fb_info->height;
    
    if (main_win->win && main_win->win->pixel_buffer) {
        uint32_t* win_buffer = main_win->win->pixel_buffer;
        int win_w = main_win->win->size.width;
        int win_h = main_win->win->size.height;
        
        for (int y = 0; y < win_h && y < fb_h; y++) {
            for (int x = 0; x < win_w && x < fb_w; x++) {
                fb[y * fb_w + x] = win_buffer[y * win_w + x];
            }
        }
    }
    
    SERIAL_LOG("[KERNEL] Main window created and rendered\n");
    gfx_print("Main window ready.\n");
    
    // Step 2: Show login screen overlay
    SERIAL_LOG("[KERNEL] Creating login overlay\n");
    gfx_print("Showing login screen...\n");
    
    extern LoginScreen* login_screen_create(void);
    extern void login_screen_set_callback(LoginScreen* login, void (*callback)(const char*));
    extern void login_screen_update(LoginScreen* login);
    extern void login_screen_render(LoginScreen* login);
    extern void login_screen_handle_event(LoginScreen* login, QARMA_INPUT_EVENT* event);
    extern void login_screen_destroy(LoginScreen* login);
    extern void keyboard_enable_window_mode(bool enable);
    extern void keyboard_set_enabled(bool enabled);
    extern bool keyboard_has_event(void);
    extern key_event_t keyboard_poll_event(void);
    extern char scancode_to_ascii(uint8_t scancode, bool shift, bool caps);
    
    LoginScreen* login = login_screen_create();
    if (!login) {
        SERIAL_LOG("[KERNEL] FATAL: Failed to create login screen\n");
        gfx_print("FATAL: Failed to create login screen\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Set callback for successful login
    login_screen_set_callback(login, on_login_success);
    
    // Enable keyboard for login
    keyboard_enable_window_mode(true);
    keyboard_set_enabled(false);
    
    SERIAL_LOG("[KERNEL] Entering login loop\n");
    
    // Blit main window gradient to framebuffer once (it doesn't change)
    if (main_win->win && main_win->win->pixel_buffer) {
        uint32_t* win_buffer = main_win->win->pixel_buffer;
        int win_w = main_win->win->size.width;
        int win_h = main_win->win->size.height;
        
        for (int y = 0; y < win_h && y < fb_h; y++) {
            for (int x = 0; x < win_w && x < fb_w; x++) {
                fb[y * fb_w + x] = win_buffer[y * win_w + x];
            }
        }
    }
    
    // Login loop - overlay on main window
    while (login->main_window != NULL) {
        // Poll keyboard events
        while (keyboard_has_event()) {
            key_event_t key_event = keyboard_poll_event();
            
            if (!key_event.released) {
                QARMA_INPUT_EVENT input_event = {0};
                input_event.type = QARMA_INPUT_EVENT_KEY_DOWN;
                input_event.timestamp = get_ticks();
                input_event.data.key.scancode = key_event.scancode;
                input_event.data.key.keycode = key_event.scancode;
                input_event.data.key.modifiers = key_event.modifiers;
                
                bool shift = (key_event.modifiers & 0x01) != 0;
                input_event.data.key.character = scancode_to_ascii(key_event.scancode, shift, false);
                
                login_screen_handle_event(login, &input_event);
                
                // Send KEY_PRESS for printable characters
                if (input_event.data.key.character >= 32 && input_event.data.key.character <= 126) {
                    QARMA_INPUT_EVENT char_event = input_event;
                    char_event.type = QARMA_INPUT_EVENT_KEY_PRESS;
                    login_screen_handle_event(login, &char_event);
                }
            }
        }
        
        // Update and render login
        login_screen_update(login);
        login_screen_render(login);
        
        // Blit login window on top of the gradient background
        if (login->main_window && login->main_window->pixel_buffer) {
            uint32_t* login_buffer = login->main_window->pixel_buffer;
            int login_x = login->main_window->x;
            int login_y = login->main_window->y;
            int login_w = login->main_window->size.width;
            int login_h = login->main_window->size.height;
            
            for (int y = 0; y < login_h; y++) {
                for (int x = 0; x < login_w; x++) {
                    int fb_x = login_x + x;
                    int fb_y = login_y + y;
                    if (fb_x >= 0 && fb_x < fb_w && fb_y >= 0 && fb_y < fb_h) {
                        fb[fb_y * fb_w + fb_x] = login_buffer[y * login_w + x];
                    }
                }
            }
        }
        
        sleep_ms(16);  // ~60fps
    }
    
    SERIAL_LOG("[KERNEL] Login successful, destroying login screen\n");
    gfx_print("Login successful!\n");
    
    // Destroy login screen (frees memory)
    login_screen_destroy(login);
    login = NULL;
    
    SERIAL_LOG("[KERNEL] Login screen destroyed\n");
    
    // Restore main window to framebuffer
    if (main_win->win && main_win->win->pixel_buffer) {
        uint32_t* win_buffer = main_win->win->pixel_buffer;
        int win_w = main_win->win->size.width;
        int win_h = main_win->win->size.height;
        
        for (int y = 0; y < win_h && y < fb_h; y++) {
            for (int x = 0; x < win_w && x < fb_w; x++) {
                fb[y * fb_w + x] = win_buffer[y * win_w + x];
            }
        }
    }
    
    // Step 3: Main window loop (desktop environment)
    SERIAL_LOG("[KERNEL] Entering main desktop loop\n");
    gfx_print("Desktop ready. Press Tab to focus close button, Enter to shutdown.\n");
    
    extern void main_window_update(MainWindow* mw);
    extern void main_window_handle_event(MainWindow* mw, QARMA_INPUT_EVENT* event);
    
    while (!main_window_should_exit(main_win)) {
        // Poll keyboard events
        while (keyboard_has_event()) {
            key_event_t key_event = keyboard_poll_event();
            
            if (!key_event.released) {
                QARMA_INPUT_EVENT input_event = {0};
                input_event.type = QARMA_INPUT_EVENT_KEY_DOWN;
                input_event.timestamp = get_ticks();
                input_event.data.key.scancode = key_event.scancode;
                input_event.data.key.keycode = key_event.scancode;
                input_event.data.key.modifiers = key_event.modifiers;
                
                main_window_handle_event(main_win, &input_event);
            }
        }
        
        // Update and render main window
        main_window_update(main_win);
        main_window_render(main_win);
        
        // Blit main window to framebuffer
        if (main_win->win && main_win->win->pixel_buffer) {
            uint32_t* win_buffer = main_win->win->pixel_buffer;
            int win_w = main_win->win->size.width;
            int win_h = main_win->win->size.height;
            
            for (int y = 0; y < win_h && y < fb_h; y++) {
                for (int x = 0; x < win_w && x < fb_w; x++) {
                    fb[y * fb_w + x] = win_buffer[y * win_w + x];
                }
            }
        }
        
        sleep_ms(16);  // ~60fps
    }
    
    SERIAL_LOG("[KERNEL] Main window close requested - shutting down\n");
    gfx_print("Shutting down system...\n");
    
    // Cleanup
    main_window_destroy(main_win);
    keyboard_set_enabled(false);
    keyboard_enable_window_mode(false);
    
    // Proper system shutdown sequence (same as cmd_shutdown)
    SERIAL_LOG("[KERNEL] Initiating ACPI shutdown\n");
    gfx_print("Shutting down...\n");
    
    cmd_shutdown(0, NULL);
    // QEMU/ACPI shutdown - sends shutdown signal to QEMU
    //outw(0x604, 0x2000);
    
    // If that doesn't work (not in QEMU), disable interrupts and halt
    //__asm__ volatile("cli");
    //SERIAL_LOG("[KERNEL] System halted\n");
    //gfx_print("System halted.\n");
    
    // Infinite halt loop - nothing should ever execute after this
    while(1) {
        __asm__ volatile("hlt");
    }
}

/**
 * Early kernel initialization - called by boot stub
 */
void kernel_early_init(void) {
    // Initialize critical systems first
    gdt_init();
    
    // Basic graphics initialization for early logging
    //graphics_init();
    
    gfx_print("Early kernel initialization complete.\n");
}

/**
 * Handle critical kernel panics
 */
void kernel_panic(const char* message) {
    // Disable interrupts
    __asm__ volatile("cli");
    
    gfx_print("\n*** KERNEL PANIC ***\n");
    gfx_print("Error: ");
    gfx_print(message);
    gfx_print("\nSystem halted.\n");
    
    // // Halt the system
    // while (1) {
    //     __asm__ volatile("hlt");
    // }
    panic(message);
}

const char* splash[] = {
    "╔══════════════════════════════════════╗",
    "║         Welcome to QARMA        ║",
    "║        The Ritual Has Begun         ║",
    "╚══════════════════════════════════════╝",
};

void draw_splash(const char* title) {
    volatile uint16_t* fb = (uint16_t*)0xB8000;
    uint8_t attr = (BLUE << 4) | WHITE;

    for (int i = 0; i < 80 * 25; i++) {
        fb[i] = (attr << 8) | ' ';
    }

    for (int i = 0; title[i]; i++) {
        fb[40 - (strlen(title) / 2) + i] = (attr << 8) | title[i];
    }
}

