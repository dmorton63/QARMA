#include "command.h"
#include "graphics/graphics.h"
#include "core/string.h"
#include "core/core_manager.h"
#include "core/memory/memory_pool.h"
#include "kernel_types.h"
#include "core/io.h"
#include "keyboard/keyboard.h"
#include "core/input/mouse.h"
#include "core/sleep.h"
//#include "drivers/usb/usb_mouse.h"
// Global state
shell_mode_t current_mode = MODE_NORMAL;

// Simple command implementations
void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Available commands:\n");
    gfx_print("  help    - Show this help message\n");
    gfx_print("  echo    - Display text\n");
    gfx_print("  clear   - Clear the screen\n");
    gfx_print("  version - Show system version\n");
    gfx_print("  cores   - Show CPU core allocation map\n");
    gfx_print("  mempool - Show memory pool statistics\n");
    gfx_print("  splash  - Display splash screen from CD-ROM\n");
    gfx_print("  kbd     - Keyboard control (enable/disable/status)\n");
    gfx_print("  pci     - Scan and display PCI devices\n");
    gfx_print("  vmm     - Test virtual memory manager\n");
    gfx_print("  icmp    - Send ICMP echo requests\n");
    gfx_print("  ifconfig - Show network interface information\n");
    gfx_print("  netstat - Show network statistics\n");
    gfx_print("  ifup    - Bring network interface up\n");
    gfx_print("  ifdown  - Bring network interface down\n");
    gfx_print("  ping    - Send ICMP echo request to host\n");
    gfx_print("  pipeline- Test execution pipeline system\n");
    gfx_print("  window  - Create a test window\n");
    gfx_print("  winloop - Run window/mouse update loop\n");
    gfx_print("  reboot  - Restart the system\n");
}

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        gfx_print(argv[i]);
        if (i < argc - 1) gfx_print(" ");
    }
    gfx_print("\n");
}

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_clear_screen();
}

void cmd_cls(int argc, char** argv) {
    cmd_clear(argc, argv);
}

void cmd_version(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("QARMA v1.0.0-alpha\n");
    gfx_print("Built with keyboard support\n");
}

void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Rebooting system...\n");
    // Simple reboot via keyboard controller
    while ((inb(0x64) & 0x02) != 0);
    outb(0x64, 0xFE);
    __asm__ volatile("hlt");
}

void cmd_shutdown(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Shutting down...\n");
    // QEMU shutdown
    outw(0x604, 0x2000);
    __asm__ volatile("hlt");
}

void cmd_exit(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Exit not implemented in kernel mode\n");
}

void cmd_kbd(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: kbd enable|disable|status\n");
        return;
    }

    if (strcmp(argv[1], "enable") == 0) {
        keyboard_set_enabled(true);
        gfx_print("Keyboard processing enabled\n");
    } else if (strcmp(argv[1], "disable") == 0) {
        keyboard_set_enabled(false);
        gfx_print("Keyboard processing disabled\n");
    } else if (strcmp(argv[1], "status") == 0) {
        gfx_print("Keyboard processing is ");
        gfx_print(keyboard_is_enabled() ? "ENABLED\n" : "DISABLED\n");
    } else {
        gfx_print("Unknown kbd command\n");
    }
}

// Forward declaration for PCI scanner
void pci_scan_and_print(void);

void cmd_pci(int argc, char** argv) {
    (void)argc; (void)argv;
    pci_scan_and_print();
}

// External functions for splash screen
extern int iso9660_read_file(const char* path, void* buffer, size_t size, size_t offset);
extern void png_decode_to_framebuffer(const uint8_t* png_data, uint32_t data_size,
                                      uint32_t* framebuffer, uint32_t fb_width, uint32_t fb_height);
extern uint32_t* video_subsystem_get_framebuffer(void);
extern void video_subsystem_get_resolution(uint32_t* width, uint32_t* height);

void cmd_cores(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void core_manager_print_allocation_map(void);
    extern core_manager_stats_t* core_manager_get_stats(void);
    
    // Print allocation map
    core_manager_print_allocation_map();
    
    // Print statistics
    core_manager_stats_t* stats = core_manager_get_stats();
    gfx_print("\n=== Core Manager Statistics ===\n");
    gfx_print("Total cores: ");
    gfx_print_hex(stats->total_cores);
    gfx_print("\nAvailable cores: ");
    gfx_print_hex(stats->available_cores);
    gfx_print("\nReserved cores: ");
    gfx_print_hex(stats->reserved_cores);
    gfx_print("\nAllocated cores: ");
    gfx_print_hex(stats->allocated_cores);
    gfx_print("\n");
}

void cmd_mempool(int argc, char** argv) {
    (void)argc; (void)argv;
    
    gfx_print("\n=== Memory Pool Manager Status ===\n\n");
    
    // Display all subsystem memory stats
    extern void memory_pool_print_all_stats(void);
    memory_pool_print_all_stats();
}

void cmd_vmm(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern uint32_t vmm_alloc_region(uint32_t size);
    extern void vmm_free_region(uint32_t virtual_addr, uint32_t size);
    
    gfx_print("=== Testing Virtual Memory Manager ===\n");
    
    // Test small allocation
    gfx_print("Allocating 4KB region...\n");
    uint32_t region1 = vmm_alloc_region(4096);
    if (region1) {
        gfx_print("Success! Virtual address: ");
        gfx_print_hex(region1);
        gfx_print("\n");
        
        // Try to write to it
        uint32_t* ptr = (uint32_t*)region1;
        *ptr = 0xDEADBEEF;
        
        gfx_print("Wrote 0xDEADBEEF, read back: ");
        gfx_print_hex(*ptr);
        gfx_print("\n");
        
        vmm_free_region(region1, 4096);
        gfx_print("Region freed\n");
    } else {
        gfx_print("Failed to allocate region\n");
    }
    
    gfx_print("\nVMM test complete\n");
}

void cmd_splash(int argc, char** argv) {
    (void)argc; (void)argv;
    
    gfx_print("Loading splash screen from CD-ROM...\n");
    
    // Get framebuffer
    uint32_t* fb = video_subsystem_get_framebuffer();
    uint32_t fb_width, fb_height;
    video_subsystem_get_resolution(&fb_width, &fb_height);
    
    if (!fb) {
        gfx_print("Error: Framebuffer not available\n");
        return;
    }
    
    // Clear framebuffer to black
    for (uint32_t i = 0; i < fb_width * fb_height; i++) {
        fb[i] = 0xFF000000;
    }
    
    // Allocate PNG buffer from VIDEO subsystem pool (2MB)
    uint8_t* png_buffer = (uint8_t*)memory_pool_alloc_large(SUBSYSTEM_VIDEO, 2048 * 1024, 0);
    if (!png_buffer) {
        gfx_print("Failed to allocate PNG buffer\n");
        return;
    }
    
    // Read splash.png from ISO9660
    int bytes_read = iso9660_read_file("/SPLASH.PNG", png_buffer, 2048 * 1024, 0);
    
    if (bytes_read > 0) {
        gfx_print("PNG loaded, decoding...\n");
        png_decode_to_framebuffer((const uint8_t*)png_buffer, bytes_read, fb, fb_width, fb_height);
        gfx_print("Splash screen displayed! Press any key to continue.\n");
    } else {
        gfx_print("Failed to load splash.png from CD-ROM\n");
    }
    
    // Free PNG buffer
    memory_pool_free(SUBSYSTEM_VIDEO, png_buffer);
}

// Simple command table
typedef struct {
    const char* name;
    void (*func)(int argc, char** argv);
} simple_command_t;

static const simple_command_t commands[] = {
    {"help", cmd_help},
    {"echo", cmd_echo},
    {"clear", cmd_clear},
    {"cls", cmd_cls},
    {"version", cmd_version},
    {"reboot", cmd_reboot},
    {"shutdown", cmd_shutdown},
    {"exit", cmd_exit},
    {"kbd", cmd_kbd},
    {"mempool", cmd_mempool},
    {"vmm", cmd_vmm},
    {"pci", cmd_pci},
    {"cores", cmd_cores},
    {"splash", cmd_splash},
    {"ifconfig", cmd_ifconfig},
    {"ifup", cmd_ifup},
    {"ifdown", cmd_ifdown},
    {"ping", cmd_ping},
    {"arp", cmd_arp},
    {"pipeline", cmd_pipeline},
    {"window", cmd_window},
    {"winloop", cmd_winloop},
    // {"mouse", cmd_mouse},
    {NULL, NULL}
};


// void cmd_mouse(int argc, char** argv)
// {
//     (void)argc; (void)argv;
//     show_mouse_info();

// }


// void show_mouse_info(void) {
//     if (!mouse_device) {
//         dispatch_logf("Mouse Device: Not Present");
//         return;
//     }

//     dispatch_logf("Mouse Device: Present");
//     dispatch_logf("Vendor ID: 0x%04X", mouse_device->vendor_id);
//     dispatch_logf("Product ID: 0x%04X", mouse_device->product_id);
//     dispatch_logf("Endpoint: 0x%02X (Interrupt IN)", mouse_device->endpoint_address);
//     dispatch_logf("Polling Interval: %d ms", mouse_device->poll_interval);
//     dispatch_logf("Max Packet Size: %d bytes", mouse_device->max_packet_size);

//     if (mouse_device->last_report_len > 0) {
//         dispatch_logf("Last Report: ");
//         for (int i = 0; i < mouse_device->last_report_len; i++) {
//             dispatch_logf(" 0x%02X", mouse_device->last_report[i]);
//         }
//     } else {
//         dispatch_logf("Last Report: None");
//     }
// }
// Initialize command system
bool command_init(void) {
    gfx_print("Command system initialized\n");
    return true;
}

// Parse input into argc/argv
int parse_input(char* input, char* argv[], int max_args) {
    int argc = 0;
    char* token = input;

    // Simple whitespace parsing
    while (*token && argc < max_args - 1) {
        // Skip whitespace
        while (*token == ' ' || *token == '\t') token++;

        if (*token == '\0') break;

        argv[argc++] = token;

        // Find end of token
        while (*token && *token != ' ' && *token != '\t') token++;

        if (*token) {
            *token = '\0';
            token++;
        }
    }

    argv[argc] = NULL;
    return argc;
}

// Execute command
command_result_t execute_command(const char* input) {
    if (!input || strlen(input) == 0) {
        return CMD_SUCCESS;
    }

    // Copy input to avoid modifying original
    char input_copy[256];
    strcpy(input_copy, input);

    // Parse into argc/argv
    char* argv[16];
    int argc = parse_input(input_copy, argv, 16);

    if (argc == 0) {
        return CMD_SUCCESS;
    }

    // Find and execute command
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].func(argc, argv);
            return CMD_SUCCESS;
        }
    }

    gfx_print("Unknown command: ");
    gfx_print(argv[0]);
    gfx_print("\n");
    return CMD_ERROR_UNKNOWN_COMMAND;
}

// Utility functions
bool is_valid_command(const char* name) {
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(name, commands[i].name) == 0) {
            return true;
        }
    }
    return false;
}

bool check_for_command(const char* cmd) {
    return is_valid_command(cmd);
}

// Network commands
void cmd_ifconfig(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void e1000_print_info(void);
    e1000_print_info();
}

void cmd_ifup(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Interface is already up (E1000 auto-initialized)\n");
}

void cmd_ifdown(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("Interface shutdown not implemented\n");
}

void cmd_ping(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: ping <ip_address>\n");
        gfx_print("Example: ping 10.0.2.2\n");
        return;
    }
    
    // Parse IP address
    const char* ip_str = argv[1];
    uint8_t ip[4];
    int parts = 0;
    int current = 0;
    
    for (const char* p = ip_str; *p && parts < 4; p++) {
        if (*p >= '0' && *p <= '9') {
            current = current * 10 + (*p - '0');
        } else if (*p == '.') {
            ip[parts++] = (uint8_t)current;
            current = 0;
        }
    }
    if (parts < 4) {
        ip[parts] = (uint8_t)current;
    }
    
    gfx_print("Pinging ");
    gfx_print(ip_str);
    gfx_print(" with 32 bytes of data:\n");
    gfx_print("(Note: QEMU user-mode networking doesn't respond to ICMP)\n");
    
    extern void icmp_send_echo(uint32_t dest_ip);
    extern void e1000_check_packets(void);
    
    uint32_t dest = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | 
                    ((uint32_t)ip[2] << 8) | ip[3];
    
    // First attempt - may trigger ARP request
    icmp_send_echo(dest);
    
    // Poll for ARP response
    for (int i = 0; i < 10; i++) {
        e1000_check_packets();
        // Small delay
        for (volatile int j = 0; j < 1000000; j++);
    }
    
    // Second attempt - ARP should be resolved now
    icmp_send_echo(dest);
    
    // Poll for ICMP response
    for (int i = 0; i < 20; i++) {
        e1000_check_packets();
        for (volatile int j = 0; j < 1000000; j++);
    }
    
    gfx_print("\n");
}

void cmd_arp(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void arp_print_cache(void);
    arp_print_cache();
}

void cmd_pipeline(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void pipeline_example_test(void);
    pipeline_example_test();
}

void cmd_window(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void window_test_demo(void);
    window_test_demo();
}

void cmd_winloop(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern void window_update_mouse(void);
    extern mouse_state_t mouse_state;
    extern void mock_mouse_update(void);
    
    SERIAL_LOG("Starting window mode - ESC to exit");
    
    // Initial render
    window_update_mouse();
    
    // Track mouse state for change detection
    int last_x = mouse_state.x;
    int last_y = mouse_state.y;
    bool last_left = mouse_state.left_pressed;
    
    // Simple render loop - interrupts handle everything else
    bool exit_loop = false;
    while (!exit_loop) {
        // Check for ESC key in normal scancode buffer
        if (keyboard_has_scancode()) {
            uint8_t sc = keyboard_get_scancode();
            if (sc == 0x01) {  // ESC pressed
                exit_loop = true;
            }
        }
        
        // Update mock mouse state from key states (updated by interrupts)
        mock_mouse_update();
        
        // Redraw only if mouse changed
        if (mouse_state.x != last_x || mouse_state.y != last_y || 
            mouse_state.left_pressed != last_left) {
            window_update_mouse();
            last_x = mouse_state.x;
            last_y = mouse_state.y;
            last_left = mouse_state.left_pressed;
        }
        
        // Wait for interrupt with interrupts enabled (sti + hlt is atomic)
        __asm__ volatile("sti; hlt");
    }
    
    SERIAL_LOG("Exiting window mode");
}

// Stub functions for compatibility
shell_mode_t get_current_mode(void) { return current_mode; }
void set_current_mode(shell_mode_t mode) { current_mode = mode; }
const char* get_mode_string(shell_mode_t mode) { (void)mode; return "normal"; }
void* alloc_temp_buffer(void) { return NULL; }
void release_temp_buffer(void* buffer) { (void)buffer; }
void cmd_bufstatus(int argc, char** argv) { (void)argc; (void)argv; }
// atoi function is provided by string.c