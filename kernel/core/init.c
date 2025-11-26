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
extern void handle_manager_init(void);
extern void message_system_init(void);
extern void frame_system_init(void);
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
    
    // Initialize handle manager (must be first - everything needs handles)
    handle_manager_init();
    gfx_print("Handle manager initialized.\n");
    
    // Initialize message system (second - communication layer)
    message_system_init();
    gfx_print("Message system initialized.\n");
    
    // Initialize frame system (third - UI containment layer)
    frame_system_init();
    gfx_print("Frame system initialized.\n");
    
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
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'1', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize filesystem subsystem
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'2', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    filesystem_subsystem_init(NULL);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'3', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize VFS and mount RAM disk
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'4', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    vfs_init();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'5', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize filesystem drivers and ATA disk
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'6', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    fs_init();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'7', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize ISO9660 filesystem
    iso9660_init();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'8', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
}

void qarma_init_cpu(void) {
    gfx_print("=== QARMA v1.0 Starting ===\n");
    gfx_print("Keyboard Testing Version\n");
    
    // Initialize IDT and interrupts (includes GDT setup)
    gfx_print("Initializing GDT, IDT and interrupts...\n");
    __asm__ volatile("cli");
    interrupts_system_init();
}

void qarma_init_input(void) {
    // Initialize PS/2 keyboard driver (for fallback)
    keyboard_init();
    keyboard_set_enabled(true);
    
    // Initialize PCI
    pci_init();
    
    // Initialize USB stack and devices (mouse + keyboard)
    usb_mouse_init();
    
    extern int usb_keyboard_init(void);
    usb_keyboard_init();
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
    qarma_window_manager_init();
    
    // Initialize window compositor for draggable windows with title bars
    extern void compositor_init(void);
    extern void console_compositor_init(void);
    compositor_init();
    console_compositor_init();  // Create console window immediately after compositor
    
    // Initialize input event system
    qarma_input_events_init();
    
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
    
    // Register console keyboard handler WITHOUT target filtering
    // This handler must receive ALL keyboard events to detect Ctrl+T even when console is hidden
    // The handler itself checks console visibility and handles Ctrl+T specially
    extern void qarma_console_keyboard_handler(QARMA_INPUT_EVENT* event, void* user_data);
    SERIAL_LOG("[INIT] Registering console keyboard handler...\n");
    qarma_input_event_listen_filtered(
        QARMA_INPUT_EVENT_KEY_DOWN,
        NULL,  // No target filter - receive all keyboard events
        qarma_console_keyboard_handler,
        NULL,
        50  // Normal priority - runs before shell handler (priority 100)
    );
    SERIAL_LOG("[INIT] Console keyboard handler registered (no target filter for Ctrl+T support)\n");
    
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
    uint32_t* fb = (uint32_t*)(uintptr_t)fb_info->virt_addr;
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
    memset((void*)(uintptr_t)fb_info->virt_addr, 0, fb_info->pitch * fb_info->height);
}

void qarma_run_login_screen(void (*on_success)(const char* username)) {
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'1', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    gfx_print("Starting desktop environment...\n");
    SERIAL_LOG("[KERNEL] ===== STARTING DESKTOP ENVIRONMENT =====\n");
    
    // Enable interrupts for GUI
    __asm__ volatile("sti");
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'2', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    SERIAL_LOG("[KERNEL] Interrupts enabled for desktop\n");
    
    if (!fb_info || !fb_info->virt_addr) {
        SERIAL_LOG("[KERNEL] ERROR: No framebuffer available\n");
        gfx_print("ERROR: No framebuffer available!\n");
        while(1) __asm__ volatile("hlt");
    }
    
    // Check if we're in text mode or graphics mode
    bool is_text_mode = (fb_info->virt_addr == 0xB8000);
    
    if (is_text_mode) {
        SERIAL_LOG("[KERNEL] Text mode detected - skipping graphical desktop\n");
        gfx_print("QARMA OS - Text Mode\n");
        gfx_print("===================\n");
        gfx_print("\n");
    } else {
        // Setup framebuffer for graphical desktop
        SERIAL_LOG("[KERNEL] Setting up graphical desktop\n");
        gfx_print("Setting up desktop...\n");
        
        // Clear all windows from window manager
        qarma_window_manager.destroy_all(&qarma_window_manager);
        
        SERIAL_LOG("[KERNEL] Desktop ready\n");
    }
    
    // Skip login screen and go directly to shell
    SERIAL_LOG("[KERNEL] Bypassing login screen\n");
    
    // In text mode, just clear the screen using VGA text mode
    if (is_text_mode) {
        SERIAL_LOG("[KERNEL] Clearing VGA text screen\n");
        extern void vga_text_clear(void);
        vga_text_clear();
        gfx_print("QARMA OS v1.0 - 64-bit Mode\n");
        gfx_print("Ready.\n\n");
        SERIAL_LOG("[KERNEL] VGA text screen cleared\n");
    } else {
        // Graphics mode - clear framebuffer
        SERIAL_LOG("[KERNEL] Clearing graphics framebuffer\n");
    SERIAL_LOG("[KERNEL] FB address: 0x");
    SERIAL_LOG_HEX("", (uint32_t)(fb_info->virt_addr >> 32));
    SERIAL_LOG_HEX("", (uint32_t)(fb_info->virt_addr));
    SERIAL_LOG("\n");
    
    // Check current CR3 value
    uint64_t cr3_val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
    uint64_t pml4_addr = cr3_val & 0xFFFFFFFFFFFFF000ULL;  // Mask off flags
    SERIAL_LOG("[KERNEL] Current CR3/PML4: 0x");
    SERIAL_LOG_HEX("", (uint32_t)(pml4_addr >> 32));
    SERIAL_LOG_HEX("", (uint32_t)pml4_addr);
    SERIAL_LOG("\n");
    
    // Walk page tables for 7GB address to see if it's mapped
    uint64_t test_addr = fb_info->virt_addr;  // Framebuffer virtual address
    SERIAL_LOG("[KERNEL] Walking page tables for VA 0x");
    SERIAL_LOG_HEX("", (uint32_t)(test_addr >> 32));
    SERIAL_LOG_HEX("", (uint32_t)test_addr);
    SERIAL_LOG("\n");
    
    // Extract indices: PML4[addr[47:39]] -> PDPT[addr[38:30]] -> PD[addr[29:21]] -> PT[addr[20:12]]
    uint64_t pml4_idx = (test_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (test_addr >> 30) & 0x1FF;
    uint64_t pd_idx = (test_addr >> 21) & 0x1FF;
    
    SERIAL_LOG("[KERNEL] Indices: PML4[");
    SERIAL_LOG_DEC("", pml4_idx);
    SERIAL_LOG("] PDPT[");
    SERIAL_LOG_DEC("", pdpt_idx);
    SERIAL_LOG("] PD[");
    SERIAL_LOG_DEC("", pd_idx);
    SERIAL_LOG("]\n");
    
    // Read PML4 entry (physical address, need to access via identity mapping or direct)
    // In higher-half kernel, physical addresses < 32MB are identity-mapped
    // But PML4 might be > 32MB. Let's try reading it assuming identity map works
    uint64_t* pml4_table = (uint64_t*)pml4_addr;
    uint64_t pml4_entry = pml4_table[pml4_idx];
    SERIAL_LOG("[KERNEL] PML4[");
    SERIAL_LOG_DEC("", pml4_idx);
    SERIAL_LOG("] = 0x");
    SERIAL_LOG_HEX("", (uint32_t)(pml4_entry >> 32));
    SERIAL_LOG_HEX("", (uint32_t)pml4_entry);
    if (!(pml4_entry & 0x1)) {
        SERIAL_LOG(" (NOT PRESENT!)\n");
    } else {
        SERIAL_LOG(" (present)\n");
        
        uint64_t pdpt_addr = pml4_entry & 0xFFFFFFFFFFFFF000ULL;
        uint64_t* pdpt_table = (uint64_t*)pdpt_addr;
        uint64_t pdpt_entry = pdpt_table[pdpt_idx];
        SERIAL_LOG("[KERNEL] PDPT[");
        SERIAL_LOG_DEC("", pdpt_idx);
        SERIAL_LOG("] = 0x");
        SERIAL_LOG_HEX("", (uint32_t)(pdpt_entry >> 32));
        SERIAL_LOG_HEX("", (uint32_t)pdpt_entry);
        if (!(pdpt_entry & 0x1)) {
            SERIAL_LOG(" (NOT PRESENT!)\n");
        } else {
            SERIAL_LOG(" (present)\n");
            
            uint64_t pd_addr = pdpt_entry & 0xFFFFFFFFFFFFF000ULL;
            uint64_t* pd_table = (uint64_t*)pd_addr;
            uint64_t pd_entry = pd_table[pd_idx];
            SERIAL_LOG("[KERNEL] PD[");
            SERIAL_LOG_DEC("", pd_idx);
            SERIAL_LOG("] = 0x");
            SERIAL_LOG_HEX("", (uint32_t)(pd_entry >> 32));
            SERIAL_LOG_HEX("", (uint32_t)pd_entry);
            if (!(pd_entry & 0x1)) {
                SERIAL_LOG(" (NOT PRESENT!)\n");
            } else if (pd_entry & 0x80) {
                SERIAL_LOG(" (2MB page)\n");
            } else {
                SERIAL_LOG(" (points to PT)\n");
            }
        }
    }
    
    // Test single byte write first
    SERIAL_LOG("[KERNEL] Testing single byte write...\n");
    volatile uint8_t* test_ptr = (volatile uint8_t*)(uintptr_t)fb_info->virt_addr;
    *test_ptr = 0x42;
    SERIAL_LOG("[KERNEL] Single byte write OK\n");
    
    // Test reading it back
    uint8_t read_val = *test_ptr;
    SERIAL_LOG("[KERNEL] Read back: 0x");
    SERIAL_LOG_HEX("", read_val);
    SERIAL_LOG("\n");
    
    // Now try clearing in chunks to see where it fails
    SERIAL_LOG("[KERNEL] Clearing screen in 1KB chunks...\n");
    size_t total_size = fb_info->pitch * fb_info->height;
    size_t chunk_size = 1024;
    uint8_t* fb_addr = (uint8_t*)(uintptr_t)fb_info->virt_addr;
    
    for (size_t offset = 0; offset < total_size; offset += chunk_size) {
        size_t bytes_to_clear = (offset + chunk_size > total_size) ? (total_size - offset) : chunk_size;
        memset(fb_addr + offset, 0, bytes_to_clear);
        
        // Log every 256KB
        if (offset % (256 * 1024) == 0) {
            SERIAL_LOG("[KERNEL] Cleared ");
            SERIAL_LOG_DEC("", offset / 1024);
            SERIAL_LOG(" KB\n");
        }
    }
    
    SERIAL_LOG("[KERNEL] Screen cleared successfully\n");
    
        // Clear screen to desktop background (graphics mode only)
        extern void splash_clear(rgb_color_t bg);
        rgb_color_t desktop_bg = {0x2C, 0x3E, 0x50, 255};  // Dark blue-gray
        splash_clear(desktop_bg);
        SERIAL_LOG("[KERNEL] Screen cleared to desktop background\n");
        
        // Show compositor console window (graphics mode only)
        SERIAL_LOG("[KERNEL] Showing console window\n");
        extern void console_compositor_show(void);
        console_compositor_show();
        
        // Render compositor windows (console)
        extern void compositor_render_all(void);
        compositor_render_all();
        SERIAL_LOG("[KERNEL] Console window rendered and visible\n");
    }
    
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
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'B', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
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
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'A', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    // Parse multiboot info
    qarma_init_memory(mbi);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'B', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    multiboot_parse_info(magic, mbi);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'C', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize graphics
    qarma_init_graphics(mbi);
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'D', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize core subsystems
    qarma_init_core_subsystems();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'E', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize task manager
    extern void task_manager_init(void);
    task_manager_init();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'F', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    SERIAL_LOG("[KERNEL] Task manager initialized\n");
    
    // Initialize filesystems
    qarma_init_filesystems();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'G', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize CPU and interrupts
    qarma_init_cpu();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'H', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize input devices
    qarma_init_input();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'I', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize GUI
    qarma_init_gui();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'J', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Initialize AI persistence
    extern void ai_persistence_init(void);
    ai_persistence_init();
    __asm__ volatile("mov $0x3F8, %%dx\n" "mov $'K', %%al\n" "out %%al, %%dx\n" ::: "rax", "rdx");
    
    // Try to load previous AI learning data
    extern int ai_load_state(void);
    ai_load_state();
    
    // Show boot messages
    //qarma_show_boot_messages();
}
