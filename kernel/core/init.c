/**
 * QARMA System Initialization Module
 * 
 * Handles all system initialization tasks, reducing kernel.c footprint.
 */

#include "core/init.h"
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
#include "qarma_win_handle/qarma_window_manager.h"
#include "qarma_win_handle/qarma_input_events.h"
#include "core/memory.h"
#include "core/memory/memory_pool.h"
#include "core/memory/dma_allocator.h"
#include "core/input/mouse.h"
#include "core/pci.h"
#include "fs/file_subsystem/file_subsystem.h"
#include "fs/vfs.h"
#include "fs/iso9660.h"
#include "graphics/png_decoder.h"
#include "core/memory/heap.h"
#include "drivers/usb/usb_mouse.h"
#include "keyboard/command.h"
#include "qarma_win_handle/login_screen.h"

// Debug flag: Set to 1 to enable mouse event logging, 0 to disable
#define DEBUG_MOUSE_EVENTS 1
#include "qarma_win_handle/main_window.h"
#include "gui/boot_messages.h"

// External function declarations
extern void gdt_init(void);
extern void idt_init(void);
extern void interrupts_system_init(void);
extern void multiboot_parse_info(uint32_t magic, multiboot_info_t* mbi);
extern uint32_t get_ticks(void);
extern void parallel_engine_init(void);
extern void core_manager_init(void);
extern void memory_pool_init(void);
extern void pipeline_system_init(void);
extern void fs_init(void);
extern void pci_init(void);
extern int usb_mouse_init(void);
extern void qarma_window_manager_init(void);
extern void qarma_input_events_init(void);
extern void show_prompt(const char* path);
extern bool keyboard_has_event(void);
extern key_event_t keyboard_poll_event(void);
extern char scancode_to_ascii(uint8_t scancode, bool shift, bool caps);
extern png_image_t* load_splash_image(void);
extern FramebufferInfo* fb_info;
extern void sleep_ms(uint32_t ms);
extern bool keyboard_get_window_key_event(key_event_t* out);
extern void login_screen_handle_event(LoginScreen* login, QARMA_INPUT_EVENT* event);
extern void login_screen_update(LoginScreen* login);
extern void login_screen_render(LoginScreen* login);

void qarma_init_memory(multiboot_info_t* mbi) {
    memory_init();
}

void qarma_init_graphics(multiboot_info_t* mbi) {
    graphics_init(mbi);
    framebuffer_init();
}

void qarma_init_core_subsystems(void) {
    // Initialize subsystem registry
    subsystem_registry_init();
    gfx_print("Subsystem registry initialized.\n");
    
    // Initialize parallel processing engine
    parallel_engine_init();
    gfx_print("Parallel processing engine initialized.\n");
    
    // Initialize core allocation manager
    core_manager_init();
    gfx_print("Core allocation manager initialized.\n");
    
    // Initialize memory pool manager
    memory_pool_init();
    gfx_print("Memory pool manager initialized.\n");
    
    // Initialize DMA allocator (for hardware DMA operations)
    dma_allocator_init();
    gfx_print("DMA allocator initialized.\n");
    
    // Initialize execution pipeline system
    pipeline_system_init();
    gfx_print("Execution pipeline system initialized.\n");
    
    // Initialize video subsystem
    video_subsystem_init(NULL);
    gfx_print("Video subsystem initialized.\n");
}

void qarma_init_filesystems(void) {
    // TEST PNG LOADING
    SERIAL_LOG("===EARLY PNG TEST START===\n");
    gfx_print("===EARLY PNG TEST START===\n");
    png_image_t* early_splash = load_splash_image();
    if (early_splash) {
        SERIAL_LOG("SUCCESS: PNG image loaded and decoded!\n");
        gfx_print("SUCCESS: PNG image loaded and decoded!\n");
        
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
    
    // Initialize filesystem drivers and ATA disk
    SERIAL_LOG("[KERNEL] Initializing filesystem drivers...\n");
    fs_init();
    SERIAL_LOG("[KERNEL] Filesystem drivers initialized\n");
    
    // Initialize ISO9660 filesystem
    SERIAL_LOG("[KERNEL] ===== INITIALIZING ISO9660 FILESYSTEM =====\n");
    iso9660_init();
    SERIAL_LOG("[KERNEL] ISO9660 init completed\n");
}

void qarma_init_cpu(void) {
    gfx_print("=== QARMA v1.0 Starting ===\n");
    gfx_print("Keyboard Testing Version\n");
    
    // Initialize GDT
    gfx_print("Initializing GDT...\n");
    gdt_init();
    
    // Initialize IDT and interrupts
    gfx_print("Initializing IDT and interrupts...\n");
    __asm__ volatile("cli");
    interrupts_system_init();
}

void qarma_init_input(void) {
    // Initialize PS/2 keyboard driver (for fallback)
    gfx_print("Initializing PS/2 keyboard driver...\n");
    keyboard_init();
    keyboard_set_enabled(true);
    
    // Initialize PCI
    pci_init();
    
    // Initialize USB stack and devices (mouse + keyboard)
    gfx_print("Initializing USB mouse driver...\n");
    usb_mouse_init();
    gfx_print("USB mouse driver initialized.\n");
    
    gfx_print("Initializing USB keyboard driver...\n");
    extern int usb_keyboard_init(void);
    usb_keyboard_init();
    gfx_print("USB keyboard driver initialized.\n");
}

// Debug mouse event handler (can be quickly disabled via DEBUG_MOUSE_EVENTS flag)
#if DEBUG_MOUSE_EVENTS
static int mouse_event_log_count = 0;
static const int MAX_MOUSE_EVENT_LOGS = 100;

static void debug_mouse_event_handler(QARMA_INPUT_EVENT* event, void* user_data) {
    (void)user_data;  // Unused
    
    if (mouse_event_log_count >= MAX_MOUSE_EVENT_LOGS) {
        return;  // Stop logging after limit
    }
    
    switch (event->type) {
        case QARMA_INPUT_EVENT_MOUSE_MOVE:
            SERIAL_LOG("[MOUSE] MOVE: pos=(");
            SERIAL_LOG_DEC("", event->data.mouse.x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", event->data.mouse.y);
            SERIAL_LOG(") delta=(");
            SERIAL_LOG_DEC("", event->data.mouse.delta_x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", event->data.mouse.delta_y);
            SERIAL_LOG(") buttons=0x");
            SERIAL_LOG_HEX("", event->data.mouse.buttons);
            SERIAL_LOG("\n");
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_DOWN:
            SERIAL_LOG("[MOUSE] DOWN: button=");
            SERIAL_LOG_DEC("", event->data.mouse.button);
            SERIAL_LOG(" pos=(");
            SERIAL_LOG_DEC("", event->data.mouse.x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", event->data.mouse.y);
            SERIAL_LOG(")\n");
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_UP:
            SERIAL_LOG("[MOUSE] UP: button=");
            SERIAL_LOG_DEC("", event->data.mouse.button);
            SERIAL_LOG(" pos=(");
            SERIAL_LOG_DEC("", event->data.mouse.x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", event->data.mouse.y);
            SERIAL_LOG(")\n");
            break;
            
        case QARMA_INPUT_EVENT_MOUSE_SCROLL:
            SERIAL_LOG("[MOUSE] SCROLL: delta=");
            SERIAL_LOG_DEC("", event->data.mouse.scroll_delta);
            SERIAL_LOG(" pos=(");
            SERIAL_LOG_DEC("", event->data.mouse.x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", event->data.mouse.y);
            SERIAL_LOG(")\n");
            break;
            
        default:
            return;  // Not a mouse event, ignore
    }
    
    mouse_event_log_count++;
    
    // Log mouse state every 10th event
    if (mouse_event_log_count % 10 == 0) {
        extern mouse_state_t mouse_state;
        SERIAL_LOG("[MOUSE_STATE] Event #");
        SERIAL_LOG_DEC("", mouse_event_log_count);
        SERIAL_LOG(": pos=(");
        SERIAL_LOG_DEC("", mouse_state.x);
        SERIAL_LOG(",");
        SERIAL_LOG_DEC("", mouse_state.y);
        SERIAL_LOG(") buttons=");
        if (mouse_state.left_pressed) SERIAL_LOG("L");
        if (mouse_state.middle_pressed) SERIAL_LOG("M");
        if (mouse_state.right_pressed) SERIAL_LOG("R");
        if (!mouse_state.left_pressed && !mouse_state.middle_pressed && !mouse_state.right_pressed) {
            SERIAL_LOG("none");
        }
        SERIAL_LOG("\n");
    }
    
    if (mouse_event_log_count == MAX_MOUSE_EVENT_LOGS) {
        SERIAL_LOG("[MOUSE] Debug logging limit reached (100 events), disabling further logs.\n");
    }
}
#endif

void qarma_init_gui(void) {
    // Initialize QARMA window manager
    gfx_print("Initializing window manager...\n");
    qarma_window_manager_init();
    gfx_print("Window manager initialized.\n");
    
    // Initialize window compositor for draggable windows with title bars
    gfx_print("Initializing window compositor...\n");
    extern void compositor_init(void);
    extern void console_compositor_init(void);
    compositor_init();
    console_compositor_init();  // Create console window immediately after compositor
    gfx_print("Window compositor and console initialized.\n");
    
    // Initialize input event system
    gfx_print("Initializing input event system...\n");
    qarma_input_events_init();
    gfx_print("Input event system initialized.\n");
    
    // Register window manager's mouse event handler
    extern void qarma_window_manager_handle_mouse_event(QARMA_INPUT_EVENT* event);
    SERIAL_LOG("[INIT] Registering window manager mouse event handler...\n");
    qarma_input_event_listen(
        0,  // Listen to all events (will filter for mouse internally)
        (QARMA_INPUT_EVENT_HANDLER)qarma_window_manager_handle_mouse_event,
        NULL,
        50  // Medium priority (before debug handler)
    );
    SERIAL_LOG("[INIT] Window manager mouse handler registered\n");
    
    // Register console keyboard handler with proper target filtering
    // This handler only receives events when console window has keyboard focus
    extern void qarma_console_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data);
    extern struct compositor_window_t* console_compositor_get_window(void);
    SERIAL_LOG("[INIT] Registering console keyboard handler...\n");
    void* console_window = console_compositor_get_window();
    qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_KEY_DOWN,
        console_window,  // Only handle events targeted at console window
        qarma_console_keyboard_handler,
        NULL,
        50  // Normal priority - filtering handles routing
    );
    SERIAL_LOG("[INIT] Console keyboard handler registered with target filter\n");
    
    // Register shell keyboard handler now that event system is ready
    extern void keyboard_register_shell_handler(void);
    keyboard_register_shell_handler();
    gfx_print("Shell keyboard handler registered.\n");
    
    #if DEBUG_MOUSE_EVENTS
    // Register debug mouse event handler (logs first 20 events)
    SERIAL_LOG("[INIT] Registering debug mouse event handler...\n");
    qarma_input_event_listen(
        0,  // Listen to all event types
        debug_mouse_event_handler,
        NULL,
        100  // High priority (runs after window manager handler)
    );
    SERIAL_LOG("[INIT] Debug mouse handler registered (will log first 100 events with state every 10th)\n");
    #endif
}

void qarma_show_boot_messages(void) {
    // Enable interrupts for keyboard input
    __asm__ volatile("sti");
    SERIAL_LOG("[KERNEL] Interrupts enabled for boot messages\n");
    
    SERIAL_LOG("[KERNEL] ===== SKIPPING BOOT MESSAGES WINDOW =====\n");
    // Skip boot messages and go straight to desktop
    // Clear screen and backing store before continuing (for double buffering)
    extern void splash_clear(rgb_color_t bg);
    rgb_color_t desktop_bg = {0x2C, 0x3E, 0x50, 255};  // Dark blue-gray
    splash_clear(desktop_bg);
    SERIAL_LOG("[KERNEL] Screen and backing store cleared, continuing to login\n");
    return;
    
    SERIAL_LOG("[KERNEL] ===== CREATING BOOT MESSAGES WINDOW =====\n");
    
    // Create boot messages window (centered on screen)
    int win_w = 600;
    int win_h = 400;
    int win_x = (fb_info->width - win_w) / 2;
    int win_y = (fb_info->height - win_h) / 2;
    
    BootMessagesWindow* boot_msg_win = boot_messages_create(win_x, win_y, win_w, win_h);
    if (!boot_msg_win) {
        SERIAL_LOG("[KERNEL] Failed to create boot messages window\n");
        return;
    }
    
    SERIAL_LOG("[KERNEL] Boot messages window created\n");
    
    // Add boot messages
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
    
    // Wait for user to acknowledge boot complete
    keyboard_enable_window_mode(true);
    keyboard_set_enabled(false);
    
    SERIAL_LOG("[KERNEL] Waiting for user to close boot messages (auto-close in 10 frames)\n");
    
    bool boot_msg_closed = false;
    uint32_t wait_count = 0;
    const uint32_t auto_close_delay = 10;
    
    while (!boot_msg_closed) {
        // Auto-close after delay
        if (wait_count++ >= auto_close_delay) {
            SERIAL_LOG("[KERNEL] Auto-closing boot window\n");
            boot_msg_closed = true;
            break;
        }
        
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

void qarma_run_login_screen(void (*on_success)(const char* username)) {
    gfx_print("Starting desktop environment...\n");
    SERIAL_LOG("[KERNEL] ===== STARTING DESKTOP ENVIRONMENT =====\n");
    
    // Enable interrupts for GUI
    __asm__ volatile("sti");
    SERIAL_LOG("[KERNEL] Interrupts enabled for desktop\n");
    
    if (!fb_info || !fb_info->address) {
        SERIAL_LOG("[KERNEL] ERROR: No framebuffer available\n");
        gfx_print("ERROR: No framebuffer available!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Setup framebuffer for desktop
    SERIAL_LOG("[KERNEL] Setting up desktop\n");
    gfx_print("Setting up desktop...\n");
    
    // Clear all windows from window manager
    qarma_window_manager.destroy_all(&qarma_window_manager);
    
    uint32_t* fb = (uint32_t*)fb_info->address;
    int fb_w = fb_info->width;
    int fb_h = fb_info->height;
    
    SERIAL_LOG("[KERNEL] Desktop ready\n");
    
    // Skip login screen and go directly to shell
    SERIAL_LOG("[KERNEL] Bypassing login screen\n");
    
    // Clear the screen completely (remove all boot text)
    memset((void*)fb_info->address, 0, fb_info->pitch * fb_info->height);
    SERIAL_LOG("[KERNEL] Screen cleared\n");
    
    // Clear screen to desktop background
    extern void splash_clear(rgb_color_t bg);
    rgb_color_t desktop_bg = {0x2C, 0x3E, 0x50, 255};  // Dark blue-gray
    splash_clear(desktop_bg);
    SERIAL_LOG("[KERNEL] Screen cleared to desktop background\n");
    
    // Show compositor console window
    SERIAL_LOG("[KERNEL] Showing console window\n");
    extern void console_compositor_show(void);
    console_compositor_show();
    
    // Render compositor windows (console)
    extern void compositor_render_all(void);
    compositor_render_all();
    SERIAL_LOG("[KERNEL] Console window rendered and visible\n");
    
    // Enable keyboard
    keyboard_set_enabled(true);
    SERIAL_LOG("[KERNEL] Keyboard enabled\n");
    
    // Clear any stale keyboard events from buffer
    extern bool keyboard_has_event(void);
    extern key_event_t keyboard_poll_event(void);
    while (keyboard_has_event()) {
        keyboard_poll_event();  // Discard
    }
    SERIAL_LOG("[KERNEL] Keyboard buffer cleared\n");
    
    // Console compositor handles all input/output
    SERIAL_LOG("[KERNEL] Console ready for input\n");
    
    // Call success callback to mark login complete
    if (on_success) {
        on_success("admin");
    }
    
    // Run shell with desktop visible in background
    extern void shell_run(void);
    SERIAL_LOG("[KERNEL] Starting shell with desktop\n");
    shell_run();
    
    // Shell exited (should never happen normally)
    SERIAL_LOG("[KERNEL] Shell exited\n");
    SERIAL_LOG("[KERNEL] System shutting down...\n");
    
    // Clean shutdown message
    gfx_print("\n\nShell terminated.\n");
    gfx_print("Shutting down...\n");
    
    // Use QEMU's ISA debug exit device to exit cleanly
    // Port 0x501 with value 0 = success (exit code 0)
    outb(0x501, 0x00);
    
    // If that doesn't work (not in QEMU or device not present), just halt
    while(1) {
        __asm__ volatile("hlt");
    }
    
    // OLD LOGIN CODE (disabled)
    #if 0
    // Show login screen overlay
    SERIAL_LOG("[KERNEL] Creating login overlay\n");
    gfx_print("Showing login screen...\n");
    
    LoginScreen* login = login_screen_create();
    if (!login) {
        SERIAL_LOG("[KERNEL] FATAL: Failed to create login screen\n");
        gfx_print("FATAL: Failed to create login screen\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Set callback for successful login
    login_screen_set_callback(login, on_success);
    
    // Enable keyboard for login
    keyboard_enable_window_mode(true);
    keyboard_set_enabled(true);
    
    SERIAL_LOG("[KERNEL] Entering login loop\n");
    
    // Clear framebuffer with black for login screen
    for (int i = 0; i < fb_w * fb_h; i++) {
        fb[i] = 0x000000;
    }
    
    // Login loop
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
        
        // Blit login window on top of background
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
    
    // Destroy login screen
    login_screen_destroy(login);
    login = NULL;
    
    SERIAL_LOG("[KERNEL] Login screen destroyed\n");
    
    // Re-enable console input/output after login
    keyboard_enable_window_mode(false);
    keyboard_set_enabled(true);
    
    SERIAL_LOG("[KERNEL] Console input/output re-enabled\n");
    #endif
}

void qarma_run_desktop(void) {
    SERIAL_LOG("[KERNEL] Entering desktop loop\n");
    
    // Initialize compositor and windows immediately
    SERIAL_LOG("[KERNEL] Initializing compositor and windows\n");
    extern void window_test_demo(void);
    window_test_demo();
    
    SERIAL_LOG("[KERNEL] Desktop initialized with compositor\n");
    
    // Don't show_prompt() - it writes directly to framebuffer and covers windows
    // Console window handles its own prompt
    
    bool should_exit = false;
    
    // Main desktop loop - compositor handles all rendering
    while (!should_exit) {
        sleep_ms(16);  // ~60fps
    }
    
    SERIAL_LOG("[KERNEL] Desktop exit requested - shutting down\n");
    gfx_print("Shutting down system...\n");
    keyboard_set_enabled(false);
    keyboard_enable_window_mode(false);
    
    // Save AI state before shutdown
    extern int ai_save_state(void);
    gfx_print("Saving AI learning data...\n");
    ai_save_state();
    
    // Proper system shutdown
    SERIAL_LOG("[KERNEL] Initiating ACPI shutdown\n");
    gfx_print("Shutting down...\n");
    
    extern void cmd_shutdown(int argc, char** argv);
    cmd_shutdown(0, NULL);
    
    // Infinite halt loop
    while(1) {
        __asm__ volatile("hlt");
    }
}

void qarma_init_all(uint32_t magic, multiboot_info_t* mbi) {
    // Parse multiboot info
    qarma_init_memory(mbi);
    multiboot_parse_info(magic, mbi);
    
    // Initialize graphics
    qarma_init_graphics(mbi);
    
    // Initialize core subsystems
    qarma_init_core_subsystems();
    
    // Initialize task manager
    extern void task_manager_init(void);
    task_manager_init();
    SERIAL_LOG("[KERNEL] Task manager initialized\n");
    
    // Initialize filesystems
    qarma_init_filesystems();
    
    // Initialize CPU and interrupts
    qarma_init_cpu();
    
    // Initialize input devices
    qarma_init_input();
    
    // Initialize GUI
    qarma_init_gui();
    
    // Initialize AI persistence
    extern void ai_persistence_init(void);
    ai_persistence_init();
    
    // Try to load previous AI learning data
    extern int ai_load_state(void);
    ai_load_state();
    
    // Show boot messages
    //qarma_show_boot_messages();
}
