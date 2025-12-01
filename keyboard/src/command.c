#include "command.h"
#include "graphics.h"
#include "string.h"
#include "core_manager.h"
#include "memory/memory_pool.h"
#include "kernel_types.h"
#include "io.h"
#include "keyboard.h"
#include "input/mouse.h"
#include "sleep.h"
#include "scheduler/task_manager.h"
// VFS for admin/dev diagnostics and AI state paths
#include "vfs.h"
#include "udp.h"
#include "network_subsystem.h"
#include "port_manager.h"
#include "tcp.h"
#include "netlog.h"
#include "log_timestamp.h"
// Console dump API
#include "console_compositor.h"
// E1000 loopback helpers
#include "net/net/e1000.h"
//#include "usb/usb_mouse.h"

// --- Serial tee support for command output ---
// When enabled, mirror gfx_* prints to SERIAL_LOG so output is captured in QEMU serial.
static bool g_serial_tee = false;
static bool g_serial_tee_ts = false;
extern bool g_log_use_datetime;

// Keep real function pointers so we can macro-wrap the gfx_* calls locally
extern void gfx_print(const char*);
extern void gfx_print_decimal(uint32_t);
extern void gfx_print_hex(uint32_t);
static void (*gfx_print_real)(const char*) = gfx_print;
static void (*gfx_print_decimal_real)(uint32_t) = gfx_print_decimal;
static void (*gfx_print_hex_real)(uint32_t) = gfx_print_hex;
extern void serial_debug_hex(uint32_t);

// Provide a safe itoa for decimal mirroring
static void cmd_itoa_u32(uint32_t v, char* buf, int base) {
    static const char* digits = "0123456789ABCDEF";
    char tmp[32]; int i = 0;
    if (base < 2 || base > 16) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (v == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (v > 0) { tmp[i++] = digits[v % (uint32_t)base]; v /= (uint32_t)base; }
    int j = 0; while (i > 0) { buf[j++] = tmp[--i]; } buf[j] = '\0';
}

// Detect if a string is just a printf-style placeholder (optionally prefixed by
// a single '>' or ':' and optionally suffixed by a single '\n'). Examples:
// "%s", ">%d", ":%x\n". Used to avoid mirroring accidental raw format tokens.
static bool is_placeholder_only(const char* s) {
    if (!s) return false;
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    if (!*s) return false;
    const char* p = s;
    if (*p == '>' || *p == ':') {
        p++;
        while (*p == ' ' || *p == '\t') p++; // allow space after prefix like "> %s"
    }
    if (*p != '%') return false;
    p++;
    char c = *p;
    if (!(c == 's' || c == 'd' || c == 'u' || c == 'x' || c == 'X' || c == 'c')) return false;
    p++;
    if (*p == '\n') p++;
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    return *p == '\0';
}

#define GFX_PRINT_TEE(str) do { \
    const char* __s = (str); \
    gfx_print_real(__s); \
    if (g_serial_tee) { \
        if (g_serial_tee_ts) { PRINT_TIMESTAMP(); } \
        /* Filter out accidental raw format tokens that sometimes leak */ \
        if (is_placeholder_only(__s)) { \
            /* skip */ \
        } else { \
            SERIAL_LOG((char*)(__s)); \
        } \
    } \
} while(0)

#undef gfx_print
#define gfx_print(str) GFX_PRINT_TEE(str)

#undef gfx_print_decimal
#define gfx_print_decimal(val) do { \
    uint32_t __v = (uint32_t)(val); \
    gfx_print_decimal_real(__v); \
    if (g_serial_tee) { char __buf[16]; cmd_itoa_u32(__v, __buf, 10); SERIAL_LOG(__buf); } \
} while(0)

#undef gfx_print_hex
#define gfx_print_hex(val) do { \
    uint32_t __v = (uint32_t)(val); \
    gfx_print_hex_real(__v); \
    if (g_serial_tee) { serial_debug_hex(__v); } \
} while(0)

// Filesystem commands (implemented in fs_commands.c)
extern void cmd_ls(int argc, char** argv);
extern void cmd_cd(int argc, char** argv);
extern void cmd_pwd(int argc, char** argv);
extern void cmd_mkdir(int argc, char** argv);
extern void cmd_rmdir(int argc, char** argv);
extern void cmd_cat(int argc, char** argv);
extern void cmd_rm(int argc, char** argv);
extern void cmd_cp(int argc, char** argv);
extern void cmd_mv(int argc, char** argv);
extern void cmd_disk(int argc, char** argv);
extern void cmd_dir(int argc, char** argv);

// Task scheduler test
void cmd_tasktest(int argc, char** argv);

// IDE
extern void ide_launch(void);

// 9P test
void cmd_9ptest(int argc, char** argv);

// Global state
shell_mode_t current_mode = MODE_NORMAL;

// IDE command
void cmd_ide(int argc, char** argv) {
    (void)argc; (void)argv;
    ide_launch();
}

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
    gfx_print("  mouse_status [driver|cursor_on|cursor_off] - Mouse info\n");
    gfx_print("  pci     - Scan and display PCI devices\n");
    gfx_print("  vmm     - Test virtual memory manager\n");
    gfx_print("  icmp    - Send ICMP echo requests\n");
    gfx_print("  ifconfig - Show network interface information\n");
    gfx_print("  netstat - Show network statistics\n");
    gfx_print("  ifup [stage1|full|safe] - Bring NIC up\n");
    gfx_print("  ifdown  - Bring network interface down\n");
    gfx_print("  ping    - Send ICMP echo request to host\n");
    gfx_print("  e1000_diag - Dump E1000 registers and rings\n");
    gfx_print("  e1000_loopback off|mac|phy|status - Toggle NIC loopback\n");
    gfx_print("  txtest  - Send a TX broadcast test frame\n");
    gfx_print("  lbtest  - One-shot MAC loopback test and diag\n");
    gfx_print("  phy_dump- Dump PHY autoneg/link registers\\n");
    gfx_print("  pipeline- Test execution pipeline system\n");
    gfx_print("  window  - Create a test window\n");
    gfx_print("  winloop - Run window/mouse update loop\n");
    gfx_print("  quantum [on|off|status|full] - Quantum control and examples\n");
    gfx_print("  aisave  - Save AI learning data to disk\n");
    gfx_print("  aiload  - Load AI learning data from disk\n");
    gfx_print("  aistats - Show AI and quantum statistics\n");
    gfx_print("  reboot  - Restart the system\n");
    gfx_print("  serialtee on|off|status - Mirror output to serial\n");
    gfx_print("           ts on|off|status - Toggle timestamp prefix\n");
    gfx_print("           date on|off|status - Use RTC date/time instead of ticks\n");
    gfx_print("  saveconsole [path] - Save console to /host/console_dump.txt by default\n");

    // Filesystem commands
    gfx_print("\nFilesystem commands:\n");
    gfx_print("  ls [path]   - List directory contents (sorted)\n");
    gfx_print("  dir [path]  - Alias for ls\n");
    gfx_print("  cd <dir>    - Change directory\n");
    gfx_print("  pwd         - Print working directory\n");
    gfx_print("  cat <file>  - Display file contents\n");
    gfx_print("  disk        - Show disk information\n");
    gfx_print("  mkdir <dir> - Create directory (not implemented)\n");
    gfx_print("  rmdir <dir> - Remove directory (not implemented)\n");
    gfx_print("  rm <file>   - Remove file (not implemented)\n");
    gfx_print("  cp <s> <d>  - Copy file (not implemented)\n");
    gfx_print("  mv <s> <d>  - Move/Rename file (not implemented)\n");
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

void cmd_mouse_status(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // Disable mouse polling during status read to prevent race conditions
    extern void usb_mouse_set_polling_enabled(bool enabled);
    usb_mouse_set_polling_enabled(false);
    
    extern mouse_state_t mouse_state;
    
    gfx_print("=== Mouse Status ===\n");
    
    // Check for subcommands
    if (argc > 1) {
        if (strcmp(argv[1], "driver") == 0) {
            extern uint32_t usb_mouse_get_report_count(void);
            uint32_t reports = usb_mouse_get_report_count();
            gfx_print("USB Mouse Reports Received: ");
            char buf[16];
            itoa(reports, buf, 10);
            gfx_print(buf);
            gfx_print("\n");
            gfx_print("Check serial output for detailed transfer logs\n");
            usb_mouse_set_polling_enabled(true);
            return;
        } else if (strcmp(argv[1], "cursor_off") == 0) {
            extern void compositor_set_cursor_enabled(bool enabled);
            compositor_set_cursor_enabled(false);
            gfx_print("Cursor rendering disabled\n");
            usb_mouse_set_polling_enabled(true);
            return;
        } else if (strcmp(argv[1], "cursor_on") == 0) {
            extern void compositor_set_cursor_enabled(bool enabled);
            compositor_set_cursor_enabled(true);
            gfx_print("Cursor rendering enabled\n");
            usb_mouse_set_polling_enabled(true);
            return;
        }
    }
    
    gfx_print("Position: (");
    gfx_print("Position: (");
    char buf[16];
    itoa(mouse_state.x, buf, 10);
    gfx_print(buf);
    gfx_print(", ");
    itoa(mouse_state.y, buf, 10);
    gfx_print(buf);
    gfx_print(")\n");
    
    gfx_print("Delta: (");
    itoa(mouse_state.dx, buf, 10);
    gfx_print(buf);
    gfx_print(", ");
    itoa(mouse_state.dy, buf, 10);
    gfx_print(buf);
    gfx_print(")\n");
    
    gfx_print("Buttons: ");
    if (mouse_state.left_pressed) gfx_print("LEFT ");
    if (mouse_state.middle_pressed) gfx_print("MIDDLE ");
    if (mouse_state.right_pressed) gfx_print("RIGHT ");
    if (!mouse_state.left_pressed && !mouse_state.middle_pressed && !mouse_state.right_pressed) {
        gfx_print("none");
    }
    gfx_print("\n");
    
    gfx_print("Scroll: ");
    if (mouse_state.scroll_up) gfx_print("UP ");
    if (mouse_state.scroll_down) gfx_print("DOWN ");
    if (!mouse_state.scroll_up && !mouse_state.scroll_down) {
        gfx_print("none");
    }
    gfx_print("\n");
    
    // Re-enable mouse polling
    usb_mouse_set_polling_enabled(true);
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

// Removed: cmd_vmm demo. Use memory pool and allocator-specific diagnostics instead.

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

// Forward declarations for commands defined later in this file
void cmd_aiquicktest(int argc, char** argv);
#ifdef CONFIG_DEV_COMMANDS
void cmd_dev_diag(int argc, char** argv);
#endif
void cmd_admin(int argc, char** argv);
void cmd_udp_echo(int argc, char** argv);
void cmd_port(int argc, char** argv);
void cmd_tcp_connect(int argc, char** argv);
void cmd_http_get(int argc, char** argv);
void cmd_netpoll(int argc, char** argv);
void cmd_arping(int argc, char** argv);
void cmd_serialtee(int argc, char** argv);
void cmd_saveconsole(int argc, char** argv);
void cmd_e1000_diag(int argc, char** argv);
void cmd_e1000_loopback(int argc, char** argv);
void cmd_txtest(int argc, char** argv);
void cmd_lbtest(int argc, char** argv);
void cmd_phy_dump(int argc, char** argv);

// Admin gating for dev commands
static bool g_admin_mode = false;

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
    {"mouse_status", cmd_mouse_status},
    {"mempool", cmd_mempool},
    {"pci", cmd_pci},
    {"cores", cmd_cores},
    {"splash", cmd_splash},
    {"ifconfig", cmd_ifconfig},
    {"ifup", cmd_ifup},
    {"ifdown", cmd_ifdown},
    {"ping", cmd_ping},
    {"arp", cmd_arp},
    {"arping", cmd_arping},
    {"udp_echo", cmd_udp_echo},
    {"port", cmd_port},
    {"tcp_connect", cmd_tcp_connect},
    {"http_get", cmd_http_get},
    {"netpoll", cmd_netpoll},
    {"pipeline", cmd_pipeline},
    {"window", cmd_window},
    {"winloop", cmd_winloop},
    {"serialtee", cmd_serialtee},
    {"hostwrite", cmd_hostwrite},
    {"saveconsole", cmd_saveconsole},
    {"e1000_diag", cmd_e1000_diag},
    {"e1000_loopback", cmd_e1000_loopback},
    {"txtest", cmd_txtest},
    {"lbtest", cmd_lbtest},
    {"phy_dump", cmd_phy_dump},
    {"netlog", cmd_netlog},
    // Filesystem commands
    {"ls", cmd_ls},
    {"dir", cmd_dir},
    {"cd", cmd_cd},
    {"pwd", cmd_pwd},
    {"mkdir", cmd_mkdir},
    {"rmdir", cmd_rmdir},
    {"cat", cmd_cat},
    {"rm", cmd_rm},
    {"cp", cmd_cp},
    {"mv", cmd_mv},
    {"disk", cmd_disk},
    {"admin", cmd_admin},
    // AI commands
    {"aisave", cmd_aisave},
    {"aiload", cmd_aiload},
    {"aistats", cmd_aistats},
    {"ai_quicktest", cmd_aiquicktest},
    {"quantum", cmd_quantum},
    {"tasktest", cmd_tasktest},
    {"ide", cmd_ide},
    {"9ptest", cmd_9ptest},
#ifdef CONFIG_DEV_COMMANDS
    {"dev_diag", cmd_dev_diag},
#endif
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
    extern net_device_t* network_get_device(uint32_t index);
    uint32_t i = 0;
    bool any = false;
    while (1) {
        net_device_t* dev = network_get_device(i);
        if (!dev) break;
        any = true;
        gfx_print(dev->name);
        gfx_print(": ");
        gfx_print(dev->state ? "UP" : "DOWN");
        gfx_print("\n  MTU: ");
        gfx_print_decimal(dev->mtu);
        gfx_print("\n  IP: ");
        gfx_print_decimal(dev->ip_address.addr[0]); gfx_print(".");
        gfx_print_decimal(dev->ip_address.addr[1]); gfx_print(".");
        gfx_print_decimal(dev->ip_address.addr[2]); gfx_print(".");
        gfx_print_decimal(dev->ip_address.addr[3]); gfx_print("\n");
        i++;
    }
    if (!any) {
        gfx_print("No network devices\n");
    }
}

void cmd_ifup(int argc, char** argv) {
    extern void e1000_set_init_mode(int mode);
    extern void e1000_init(void);
    extern void e1000_print_info(void);
    extern void e1000_print_diag(void);
    if (argc > 1 && argv && argv[1]) {
        const char* mode = argv[1];
        if (mode[0]=='f') { // full
            gfx_print("ifup: Full init (map MMIO, bring up)\n");
            e1000_set_init_mode(2);
            e1000_init();
            e1000_print_info();
            return;
        } else if (mode[0]=='d') { // diag
            gfx_print("ifup: Diagnostics dump\n");
            e1000_print_diag();
            return;
        } else if (mode[0]=='s') { // safe
            gfx_print("ifup: Safe mode (no MMIO, stub only)\n");
            e1000_set_init_mode(0);
            e1000_init();
            e1000_print_info();
            return;
        }
    }
    // Default: Stage1 logging only
    gfx_print("ifup: Stage1 (log BAR, no MMIO)\n");
    e1000_set_init_mode(1);
    e1000_init();
    e1000_print_info();
}
// Explicit E1000 diagnostic command alias (same as ifup diag)
void cmd_e1000_diag(int argc, char** argv) {
    (void)argc; (void)argv;
    extern void e1000_print_diag(void);
    gfx_print("e1000: Diagnostics dump\n");
    e1000_print_diag();
}

// e1000_loopback off|mac|phy|status
void cmd_e1000_loopback(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: e1000_loopback off|mac|phy|status\n");
        return;
    }
    if (strcmp(argv[1], "status") == 0) {
        int m = e1000_get_loopback_mode();
        if (m < 0) { gfx_print("Loopback: unavailable\n"); return; }
        gfx_print("Loopback mode: ");
        if (m == 0) gfx_print("off\n");
        else if (m == 2) gfx_print("mac\n");
        else if (m == 3) gfx_print("phy\n");
        else { gfx_print("unknown\n"); }
        return;
    }
    if (strcmp(argv[1], "off") == 0) {
        e1000_config_loopback(E1000_LB_OFF);
        gfx_print("Loopback: OFF\n");
    } else if (strcmp(argv[1], "mac") == 0) {
        e1000_config_loopback(E1000_LB_MAC);
        gfx_print("Loopback: MAC\n");
    } else if (strcmp(argv[1], "phy") == 0) {
        e1000_config_loopback(E1000_LB_PHY);
        gfx_print("Loopback: PHY\n");
    } else {
        gfx_print("Usage: e1000_loopback off|mac|phy|status\n");
    }
}

// txtest: send a simple broadcast frame via E1000
void cmd_txtest(int argc, char** argv) {
    (void)argc; (void)argv;
    extern void e1000_send_test_broadcast(void);
    e1000_send_test_broadcast();
    gfx_print("txtest: sent (check netlog)\n");
}

void cmd_phy_dump(int argc, char** argv) {
    (void)argc; (void)argv;
    extern void e1000_phy_dump(void);
    e1000_phy_dump();
}

// lbtest: enable MAC loopback, send test frame, poll briefly, dump diag, restore off
void cmd_lbtest(int argc, char** argv) {
    (void)argc; (void)argv;
    gfx_print("lbtest: enabling MAC loopback, sending, polling...\n");
    e1000_config_loopback(E1000_LB_MAC);
    // Send
    extern void e1000_send_test_broadcast(void);
    e1000_send_test_broadcast();
    // Poll receive path for a short duration
    extern void e1000_check_packets(void);
    for (int i = 0; i < 200; ++i) { // ~short spin
        e1000_check_packets();
        for (volatile int j = 0; j < 200000; ++j) { }
    }
    // Dump quick diagnostics
    extern void e1000_print_diag(void);
    e1000_print_diag();
    // Restore normal mode
    e1000_config_loopback(E1000_LB_OFF);
    gfx_print("lbtest: done\n");
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

// arping <ip>: send ARP request, poll RX briefly, then show ARP cache
void cmd_arping(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: arping <ip>\n");
        gfx_print("Example: arping 192.168.100.1\n");
        return;
    }

    // Parse dotted-quad
    const char* ip_str = argv[1];
    uint8_t ipb[4] = {0};
    int parts = 0; int current = 0;
    for (const char* p = ip_str; *p; ++p) {
        if (*p >= '0' && *p <= '9') {
            current = current * 10 + (*p - '0');
            if (current > 255) { gfx_print("Invalid IP\n"); return; }
        } else if (*p == '.') {
            if (parts >= 4) { gfx_print("Invalid IP\n"); return; }
            ipb[parts++] = (uint8_t)current; current = 0;
        } else { gfx_print("Invalid IP\n"); return; }
    }
    if (parts < 3) { gfx_print("Invalid IP\n"); return; }
    ipb[3] = (uint8_t)current;

    ipv4_addr_t tip = { .addr = { ipb[0], ipb[1], ipb[2], ipb[3] } };

    net_device_t* dev = network_get_default_device();
    if (!dev) { gfx_print("No network device\n"); return; }
    if (dev->state != NET_DEV_RUNNING) { gfx_print("Interface is down\n"); return; }

    extern int arp_send_request(net_device_t* dev, ipv4_addr_t* target_ip);
    extern void e1000_check_packets(void);
    extern void arp_print_cache(void);

    gfx_print("Sending ARP request for "); gfx_print(ip_str); gfx_print("...\n");
    (void)arp_send_request(dev, &tip);

    // Poll RX briefly to process reply
    for (int i = 0; i < 50; ++i) {
        e1000_check_packets();
        for (volatile int spin = 0; spin < 200000; ++spin) { }
    }

    gfx_print("\nARP cache:\n");
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

void cmd_winloop(int argc, char** argv);

// UDP echo client: udp_echo <port> <message>
void cmd_udp_echo(int argc, char** argv) {
    if (argc < 3) {
        gfx_print("Usage: udp_echo <port> <message>\n");
        gfx_print("Example: udp_echo 9000 hello\n");
        return;
    }

    // Parse port
    uint32_t port = 0;
    for (const char* p = argv[1]; *p; ++p) { if (*p < '0' || *p > '9') { gfx_print("Invalid port\n"); return; } port = port*10 + (uint32_t)(*p - '0'); }
    if (port == 0 || port > 65535) { gfx_print("Port out of range\n"); return; }

    net_device_t* dev = network_get_default_device();
    if (!dev) { gfx_print("No network device\n"); return; }

    // Default remote host: use device gateway (bridged LAN)
    ipv4_addr_t host = dev->gateway;
    if (host.addr[0] == 0 && host.addr[1] == 0 && host.addr[2] == 0 && host.addr[3] == 0) {
        // Fallback to a common host IP if gateway not set
        host.addr[0] = 172; host.addr[1] = 17; host.addr[2] = 131; host.addr[3] = 240;
    }

    const char* msg = argv[2];
    uint32_t len = (uint32_t)strlen(msg);

    int res = udp_send(dev, &host, /*src_port*/ (uint16_t)port, /*dest_port*/ (uint16_t)port, (const uint8_t*)msg, len);
    if (res < 0) {
        gfx_print("udp_echo: send failed (ARP unresolved?)\n");
        gfx_print("Try again after ARP resolves or run 'arp'\n");
        netlog_write("udp_echo: send failed\n");
        return;
    }
    gfx_print("udp_echo: sent "); gfx_print_decimal(len); gfx_print(" bytes to host:\n");
    netlog_write_hex("udp_echo: sent bytes=", (uint32_t)len); netlog_write("\n");
}

// Port manager control: port [allow|block|list] udp|tcp in|out <port>
void cmd_port(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: port [allow|block|list|save|load] udp|tcp [in|out] [port]\n");
        return;
    }
    if (strcmp(argv[1], "save") == 0) { extern int port_manager_save(void); int r=port_manager_save(); if(r==0) gfx_print("Saved\n"); else gfx_print("Save failed\n"); return; }
    if (strcmp(argv[1], "load") == 0) { extern int port_manager_load(void); int r=port_manager_load(); if(r==0) gfx_print("Loaded\n"); else gfx_print("Load failed\n"); return; }
    pm_protocol_t proto = (strcmp(argv[2], "udp")==0)? PM_PROTO_UDP : PM_PROTO_TCP;
    pm_direction_t dir = PM_DIR_INBOUND;
    if (argc >= 4) { dir = (strcmp(argv[3], "out")==0)? PM_DIR_OUTBOUND : PM_DIR_INBOUND; }
    if (strcmp(argv[1], "list") == 0) { port_list(proto, dir); return; }
    if (argc < 5) { gfx_print("Missing port\n"); return; }
    // Parse port
    uint32_t port = 0; for (const char* p = argv[4]; *p; ++p) { if (*p<'0'||*p>'9'){ gfx_print("Invalid port\n"); return;} port=port*10+(*p-'0'); }
    if (port>65535) { gfx_print("Port out of range\n"); return; }
    if (strcmp(argv[1], "allow")==0) { port_allow(proto, (uint16_t)port, dir); gfx_print("Port allowed\n"); }
    else if (strcmp(argv[1], "block")==0) { port_block(proto, (uint16_t)port, dir); gfx_print("Port blocked\n"); }
    else { gfx_print("Unknown action\n"); }
}

// Minimal TCP connect: tcp_connect <port>
void cmd_tcp_connect(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: tcp_connect <port>\n");
        return;
    }
    // One connection, default host = device gateway (bridged)
    uint32_t port = 0; for (const char* p = argv[1]; *p; ++p) { if (*p<'0'||*p>'9'){ gfx_print("Invalid port\n"); return;} port=port*10+(*p-'0'); }
    if (port==0 || port>65535) { gfx_print("Port out of range\n"); return; }
    net_device_t* dev = network_get_default_device(); if (!dev) { gfx_print("No network device\n"); return; }
    ipv4_addr_t host = dev->gateway;
    if (host.addr[0] == 0 && host.addr[1] == 0 && host.addr[2] == 0 && host.addr[3] == 0) { host = (ipv4_addr_t){ .addr = {172,17,131,240} }; }
    // Use an ephemeral local port for the client connection
    static uint16_t eph = 40000;
    if (eph < 49152) eph = 49152; // ensure in ephemeral range
    uint16_t local_port = eph++;
    uint16_t remote_port = (uint16_t)port;
    int r = tcp_connect(dev, &host, local_port, remote_port);
    if (r==0) { gfx_print("tcp_connect: SYN sent\n"); netlog_write_hex("tcp_connect: SYN sent port=", (uint32_t)remote_port); netlog_write("\n"); }
    else { gfx_print("tcp_connect: failed\n"); netlog_write_hex("tcp_connect: failed port=", (uint32_t)remote_port); netlog_write("\n"); }
}

// HTTP GET: http_get <port> <path>
void cmd_http_get(int argc, char** argv) {
    if (argc < 3) {
        gfx_print("Usage: http_get <port> <path>\n");
        return;
    }
    uint32_t port = 0; for (const char* p = argv[1]; *p; ++p) { if (*p<'0'||*p>'9'){ gfx_print("Invalid port\n"); return;} port=port*10+(*p-'0'); }
    if (port==0 || port>65535) { gfx_print("Port out of range\n"); return; }
    const char* path = argv[2];
    net_device_t* dev = network_get_default_device(); if (!dev) { gfx_print("No network device\n"); return; }
    ipv4_addr_t host = dev->gateway;
    if (host.addr[0] == 0 && host.addr[1] == 0 && host.addr[2] == 0 && host.addr[3] == 0) { host = (ipv4_addr_t){ .addr = {172,17,131,240} }; }
    // Use ephemeral local port to avoid inbound default-deny issues
    static uint16_t eph = 50000;
    if (eph < 49152) eph = 49152;
    uint16_t local_port = eph++;
    uint16_t remote_port = (uint16_t)port;
    extern void e1000_set_polling_enabled(bool enabled);
    extern void e1000_check_packets(void);
    // Enable polling temporarily to process ARP/TCP responses
    e1000_set_polling_enabled(true);
    netlog_write_hex("http_get: connect port=", (uint32_t)remote_port); netlog_write("\n");
    if (tcp_connect(dev, &host, local_port, remote_port) != 0) {
        gfx_print("tcp_connect pending (ARP). Waiting...\n");
        netlog_write("http_get: connect pending (ARP)\n");
        for (int i = 0; i < 300; ++i) { // brief wait to receive ARP reply
            e1000_check_packets();
            for (volatile int j = 0; j < 500000; ++j) { }
        }
        if (tcp_connect(dev, &host, local_port, remote_port) != 0) {
            e1000_set_polling_enabled(false);
            gfx_print("tcp_connect failed\n");
            netlog_write("http_get: connect failed\n");
            return;
        }
    }
    char req[256];
    int n = snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %u.%u.%u.%u\r\nConnection: close\r\n\r\n", path,
                     host.addr[0], host.addr[1], host.addr[2], host.addr[3]);
    if (n <= 0) { gfx_print("format error\n"); netlog_write("http_get: format error\n"); return; }
    if (tcp_send(dev, &host, local_port, remote_port, (const uint8_t*)req, (uint32_t)n) != 0) { gfx_print("tcp_send failed\n"); netlog_write("http_get: send failed\n"); return; }
    gfx_print("HTTP request sent; polling for response...\n");
    netlog_write("http_get: request sent; polling...\n");
    for (int i = 0; i < 600; ++i) {
        e1000_check_packets();
        for (volatile int j = 0; j < 500000; ++j) { }
    }
    // Restore polling default (OFF) to avoid input lag
    e1000_set_polling_enabled(false);
    // Optionally log completion marker (response size would need TCP RX accounting)
    netlog_write("http_get: done polling\n");
}

// netpoll on|off : toggle E1000 RX polling to reduce input lag
void cmd_netpoll(int argc, char** argv) {
    if (argc < 2) { gfx_print("Usage: netpoll on|off\n"); return; }
    extern void e1000_set_polling_enabled(bool enabled);
    if (strcmp(argv[1], "on") == 0) { e1000_set_polling_enabled(true); gfx_print("Net polling: ON\n"); }
    else if (strcmp(argv[1], "off") == 0) { e1000_set_polling_enabled(false); gfx_print("Net polling: OFF\n"); }
    else { gfx_print("Usage: netpoll on|off\n"); }
}

// serialtee on|off|status : mirror command output to serial log
void cmd_serialtee(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: serialtee on|off|status | serialtee ts on|off|status | serialtee date on|off|status\n");
        return;
    }
    if (strcmp(argv[1], "on") == 0) {
        g_serial_tee = true;
        gfx_print("Serial tee: ON\n");
    } else if (strcmp(argv[1], "off") == 0) {
        g_serial_tee = false;
        gfx_print("Serial tee: OFF\n");
    } else if (strcmp(argv[1], "status") == 0) {
        gfx_print("Serial tee status: ");
        gfx_print(g_serial_tee ? "ON\n" : "OFF\n");
    } else if (strcmp(argv[1], "ts") == 0) {
        if (argc < 3) { gfx_print("Usage: serialtee ts on|off|status\n"); return; }
        if (strcmp(argv[2], "on") == 0) { g_serial_tee_ts = true; gfx_print("Serial tee timestamps: ON\n"); }
        else if (strcmp(argv[2], "off") == 0) { g_serial_tee_ts = false; gfx_print("Serial tee timestamps: OFF\n"); }
        else if (strcmp(argv[2], "status") == 0) { gfx_print("Serial tee timestamps: "); gfx_print(g_serial_tee_ts ? "ON\n" : "OFF\n"); }
        else { gfx_print("Usage: serialtee ts on|off|status\n"); }
    } else if (strcmp(argv[1], "date") == 0) {
        if (argc < 3) { gfx_print("Usage: serialtee date on|off|status\n"); return; }
        if (strcmp(argv[2], "on") == 0) { g_log_use_datetime = true; gfx_print("Serial tee date/time: ON\n"); }
        else if (strcmp(argv[2], "off") == 0) { g_log_use_datetime = false; gfx_print("Serial tee date/time: OFF\n"); }
        else if (strcmp(argv[2], "status") == 0) { gfx_print("Serial tee date/time: "); gfx_print(g_log_use_datetime ? "ON\n" : "OFF\n"); }
        else { gfx_print("Usage: serialtee date on|off|status\n"); }
    } else {
        gfx_print("Usage: serialtee on|off|status | serialtee ts on|off|status | serialtee date on|off|status\n");
    }
}

// Host write test: hostwrite <path> <text>
// Uses 9P write-intent open to create/append on /host share
void cmd_hostwrite(int argc, char** argv) {
    if (argc < 3) { gfx_print("Usage: hostwrite <path> <text>\n"); return; }
    extern bool virtio_9p_is_mounted(void);
    extern vfs_node_t* vfs_open_for_write(const char* path);
    extern vfs_node_t* vfs_create(const char* path, uint32_t type);
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    const char* path = argv[1];
    const char* text = argv[2];
    // Require /host prefix to ensure 9P backend
    if (strncmp(path, "/host/", 6) != 0) { gfx_print("Path must start with /host/\n"); return; }
    if (!virtio_9p_is_mounted()) { gfx_print("hostwrite: 9P not mounted (/host unavailable)\n"); return; }
    vfs_node_t* node = vfs_open_for_write(path);
    if (!node) {
        // Attempt create then reopen for write (server may allow create via write intent or separate create)
        node = vfs_create(path, VFS_TYPE_FILE);
        if (!node) {
            gfx_print("hostwrite: create failed\n"); return;
        }
        // Try to re-open for write to acquire 9P fid with write mode
        vfs_node_t* wnode = vfs_open_for_write(path);
        if (wnode) node = wnode; // prefer write-intent node if available
    }
    int r = vfs_write(node, text, strlen(text), 0);
    if (r == (int)strlen(text)) { gfx_print("hostwrite: wrote OK\n"); }
    else { gfx_print("hostwrite: write failed\n"); }
}

// saveconsole [path]
void cmd_saveconsole(int argc, char** argv) {
    const char* path = "/host/console_dump.txt";
    if (argc >= 2 && argv[1] && argv[1][0]) path = argv[1];

    // If targeting host path, ensure 9P is available
    if (strncmp(path, "/host/", 6) == 0) {
        extern bool virtio_9p_is_mounted(void);
        if (!virtio_9p_is_mounted()) {
            gfx_print("saveconsole: /host not mounted (enable 9P)\n");
            return;
        }
    }

    int res = console_compositor_dump_to_file(path);
    if (res >= 0) {
        gfx_print("saveconsole: wrote "); gfx_print_decimal((uint32_t)res); gfx_print(" line(s) to "); gfx_print(path); gfx_print("\n");
    } else {
        if (res == -1) { gfx_print("saveconsole: invalid state or path\n"); }
        else if (res == -2) { gfx_print("saveconsole: /host unavailable (9P not mounted)\n"); }
        else if (res == -3) { gfx_print("saveconsole: create failed\n"); }
        else if (res == -4) { gfx_print("saveconsole: write failed (lines)\n"); }
        else if (res == -5) { gfx_print("saveconsole: write failed (newline)\n"); }
        else if (res == -6) { gfx_print("saveconsole: write failed (pending)\n"); }
        else if (res == -7) { gfx_print("saveconsole: write failed (pending newline)\n"); }
        else { gfx_print("saveconsole: error\n"); }
    }
}

// netlog status|upgrade : inspect or force migrate backend
void cmd_netlog(int argc, char** argv) {
    if (argc < 2) { gfx_print("Usage: netlog status|upgrade\n"); return; }
    if (strcmp(argv[1], "status") == 0) { netlog_status(); return; }
    if (strcmp(argv[1], "upgrade") == 0) { netlog_force_upgrade(); netlog_status(); return; }
    gfx_print("Usage: netlog status|upgrade\n");
}

// AI Commands
void cmd_aisave(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern int ai_save_state(void);
    ai_save_state();
}

void cmd_aiload(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern int ai_load_state(void);
    int result = ai_load_state();
    
    if (result != 0) {
        gfx_print("No saved AI state found or load failed\n");
    }
}

void cmd_aistats(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // Show persistence stats
    extern void ai_persistence_print_stats(void);
    ai_persistence_print_stats();
    
    // Show quantum AI stats
    extern void quantum_ai_print_stats(void);
    quantum_ai_print_stats();
    
    // Show command predictor stats
    extern void command_cache_print_stats(void);
    command_cache_print_stats();
}

// Quick save/load parity test for AI persistence
void cmd_aiquicktest(int argc, char** argv) {
    if (!g_admin_mode) { gfx_print("Access denied. Use 'admin login <password>'\n"); return; }
    (void)argc; (void)argv;

    gfx_print("Running AI quick test (cache save/load) ...\n");

    // Seed predictor cache
    extern void command_cache_clear(void);
    extern bool command_cache_result(const char* command, const char* result);
    // Predictor stats and API (declared at file scope to avoid transient editor warnings)
    extern void command_predictor_get_stats(void* stats_out);
    typedef struct { uint32_t total_predictions, cache_hits, cache_misses, cache_size; float hit_rate; } predictor_stats_t;

    command_cache_clear();
    command_cache_result("echo test1", "ok1");
    command_cache_result("echo test2", "ok2");
    command_cache_result("echo test3", "ok3");

    predictor_stats_t stats_before;
    command_predictor_get_stats(&stats_before);
    gfx_print("  Cache size before save: ");
    gfx_print_decimal(stats_before.cache_size);
    gfx_print("\n");

    extern int ai_save_state(void);
    if (ai_save_state() != 0) {
        gfx_print("  Save failed\n");
        return;
    }

    // Clear and reload
    command_cache_clear();
    predictor_stats_t stats_cleared;
    command_predictor_get_stats(&stats_cleared);
    gfx_print("  Cache size after clear: ");
    gfx_print_decimal(stats_cleared.cache_size);
    gfx_print("\n");

    extern int ai_load_state(void);
    if (ai_load_state() != 0) {
        gfx_print("  Load failed\n");
        return;
    }

    predictor_stats_t stats_after;
    command_predictor_get_stats(&stats_after);
    gfx_print("  Cache size after load: ");
    gfx_print_decimal(stats_after.cache_size);
    gfx_print("\n");

    if (stats_after.cache_size == stats_before.cache_size) {
        gfx_print("AI quick test: PASS\n");
    } else {
        gfx_print("AI quick test: FAIL (sizes differ)\n");
    }
}

#ifdef CONFIG_DEV_COMMANDS
// Developer diagnostics: write a snapshot to /ramdisk/dev_diag.log
void cmd_dev_diag(int argc, char** argv) {
    if (!g_admin_mode) { gfx_print("Access denied. Use 'admin login <password>'\n"); return; }
    (void)argc; (void)argv;
    extern vfs_node_t* vfs_create(const char* path, uint32_t type);
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    

    vfs_node_t* node = vfs_create("/ramdisk/dev_diag.log", VFS_TYPE_FILE);
    if (!node) { gfx_print("dev_diag: failed to create log file\n"); return; }

    // Collect minimal diagnostics
    char buf[256];
    size_t off = 0;
    const char* hdr = "=== QARMA Diagnostics ===\n";
    vfs_write(node, hdr, strlen(hdr), off); off += strlen(hdr);

    extern const char* quantum_get_status(void);
    const char* qstat = quantum_get_status();
    int n = snprintf(buf, sizeof(buf), "Quantum: %s\n", qstat);
    vfs_write(node, buf, (size_t)n, off); off += (size_t)n;

    // Core manager stats
    extern core_manager_stats_t* core_manager_get_stats(void);
    core_manager_stats_t* cstats = core_manager_get_stats();
    if (cstats) {
        n = snprintf(buf, sizeof(buf),
                     "Cores: total=%u available=%u reserved=%u allocated=%u\n",
                     cstats->total_cores, cstats->available_cores,
                     cstats->reserved_cores, cstats->allocated_cores);
        vfs_write(node, buf, (size_t)n, off); off += (size_t)n;
    }

    // Memory pool summary
    extern memory_pool_stats_t* memory_pool_get_stats(void);
    memory_pool_stats_t* mp = memory_pool_get_stats();
    if (mp) {
        n = snprintf(buf, sizeof(buf), "Memory: total=%u used=%u free=%u\n",
                     mp->total_bytes, mp->used_bytes, mp->free_bytes);
        vfs_write(node, buf, (size_t)n, off); off += (size_t)n;
    }

    // AI persistence counters (printed via aistats, but include summary)
    extern void ai_persistence_print_stats(void);
    const char* hint = "See 'aistats' for detailed AI persistence metrics\n";
    vfs_write(node, hint, strlen(hint), off); off += strlen(hint);

    gfx_print("dev_diag: wrote /ramdisk/dev_diag.log\n");
}
#endif

// Admin command to manage dev access
void cmd_admin(int argc, char** argv) {
    extern vfs_node_t* vfs_open(const char* path);
    extern vfs_node_t* vfs_create(const char* path, uint32_t type);
    extern int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset);
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);

    // Prefer ramdisk to avoid host mount/write issues
    const char* ram_path = "/ramdisk/admin.bin";

    if (argc < 2) {
        gfx_print("Usage: admin [login <password>|set <password>|status]\n");
        return;
    }

    if (strcmp(argv[1], "status") == 0) {
        gfx_print("Admin mode: "); gfx_print(g_admin_mode ? "ENABLED\n" : "DISABLED\n");
        return;
    }

    if (strcmp(argv[1], "login") == 0 && argc >= 3) {
        // Try ramdisk first for stability
        vfs_node_t* node = vfs_open(ram_path);
        const char* used = ram_path;
        if (!node) { gfx_print("No admin password set. Use 'admin set <password>' first.\n"); return; }
        char stored[128];
        int r = vfs_read(node, stored, sizeof(stored)-1, 0);
        if (r <= 0) { gfx_print("Failed to read admin password\n"); return; }
        stored[r] = '\0';
        if (strcmp(stored, argv[2]) == 0) {
            g_admin_mode = true;
            gfx_print("Admin login successful\n");
        } else {
            gfx_print("Admin login failed\n");
        }
        return;
    }

    if (strcmp(argv[1], "set") == 0 && argc >= 3) {
        // Write to ramdisk to ensure reliability
        vfs_node_t* node = vfs_create(ram_path, VFS_TYPE_FILE);
        const char* used = ram_path;
        if (!node) { gfx_print("Failed to create admin password file\n"); return; }
        int w = vfs_write(node, argv[2], strlen(argv[2]), 0);
        if (w == (int)strlen(argv[2])) {
            gfx_print("Admin password set at "); gfx_print(used); gfx_print("\n");
        } else {
            gfx_print("Failed to write admin password\n");
        }
        return;
    }

    gfx_print("Invalid admin command. Use 'admin login <password>' or 'admin set <password>'\n");
}

// Background task functions for testing quantum AI
static int bg_task_1(void *data) {
    int id = (int)(uintptr_t)data;
    for (int i = 0; i < 50; i++) {
        extern void task_yield(void);
        task_yield();
        if (i % 10 == 0) {
            SERIAL_LOG("[BG_TASK");
            SERIAL_LOG_HEX("] iteration ", id);
            SERIAL_LOG_HEX(": ", i);
            SERIAL_LOG("\\n");
        }
    }
    return 0;
}

static int bg_task_2(void *data) {
    int id = (int)(uintptr_t)data;
    for (int i = 0; i < 30; i++) {
        extern void task_yield(void);
        task_yield();
        if (i % 10 == 0) {
            SERIAL_LOG("[BG_TASK");
            SERIAL_LOG_HEX("] iteration ", id);
            SERIAL_LOG_HEX(": ", i);
            SERIAL_LOG("\\n");
        }
    }
    return 0;
}

void cmd_tasktest(int argc, char** argv) {
    (void)argc; (void)argv;
    
    gfx_print("Quantum AI requires actual task switching to observe.\\n");
    gfx_print("Current limitation: Shell doesn't run as schedulable task.\\n");
    gfx_print("\\n");
    gfx_print("To test quantum AI properly, run 'quantum' command demos\\n");
    gfx_print("which create quantum registers with task scheduling.\\n");
    gfx_print("\\n");
    gfx_print("The integration is complete - quantum AI will observe\\n");
    gfx_print("task switches when proper multitasking is active.\\n");
    gfx_print("\\n");
    gfx_print("Implementation includes:\\n");
    gfx_print("  - Task execution tracking\\n");
    gfx_print("  - Workload profiling\\n");
    gfx_print("  - AI-driven task selection\\n");
    gfx_print("  - Statistics and quality metrics\\n");
}

void cmd_quantum(int argc, char** argv) {
    // Quantum toggle commands
    if (argc > 1) {
        extern void quantum_enable(void);
        extern void quantum_disable(void);
        extern const char* quantum_get_status(void);
        
        if (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "enable") == 0) {
            quantum_enable();
            gfx_print("Quantum processing: ENABLED\n");
            gfx_print("All tasks will now use quantum-inspired optimization\n");
            return;
        } else if (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "disable") == 0) {
            quantum_disable();
            gfx_print("Quantum processing: DISABLED\n");
            gfx_print("Tasks will use classical scheduling\n");
            return;
        } else if (strcmp(argv[1], "status") == 0) {
            gfx_print("Quantum processing status: ");
            gfx_print(quantum_get_status());
            gfx_print("\n");
            return;
        }
    }
    
    // Clear screen for better visibility
    extern void framebuffer_clear(void);
    framebuffer_clear();
    
    if (argc > 1 && strcmp(argv[1], "full") == 0) {
        gfx_print("Running all quantum register examples...\n");
        gfx_print("(This may take a while - output in serial log)\n");
        gfx_print("Press any key to continue...\n");
        
        // Wait for keypress
        extern bool keyboard_has_scancode(void);
        extern uint8_t keyboard_get_scancode(void);
        while (!keyboard_has_scancode()) {
            __asm__ volatile("hlt");
        }
        keyboard_get_scancode(); // consume key
        
        framebuffer_clear();
        
        extern void quantum_register_run_examples(void);
        quantum_register_run_examples();
        
        gfx_print("\nQuantum examples complete! Press any key...\n");
        while (!keyboard_has_scancode()) {
            __asm__ volatile("hlt");
        }
        keyboard_get_scancode();
        framebuffer_clear();
    } else {
        // Quick demo - just run a couple examples
        gfx_print("=== QARMA Quantum Demo ===\n");
        gfx_print("(Use 'quantum full' for all 12 examples)\n\n");
        
        extern void quantum_ai_init(void);
        extern void example_simple_parallel(void);
        extern void example_ai_recommended(void);
        extern void quantum_ai_print_stats(void);
        
        quantum_ai_init();
        
        gfx_print("1. Simple parallel processing...\n");
        example_simple_parallel();
        
        gfx_print("\n2. AI-recommended strategy...\n");
        example_ai_recommended();
        
        gfx_print("\n--- AI Learning Statistics ---\n");
        quantum_ai_print_stats();
        
        gfx_print("\n=== Demo Complete ===\n");
        gfx_print("The AI learned from these examples!\n");
        gfx_print("Try: aistats, aisave, or 'quantum full'\n");
    }
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
        
        // Wait for interrupt with interrupts enabled (sti+hlt is atomic)
        __asm__ volatile("sti; hlt");
    }
    
    SERIAL_LOG("Exiting window mode");
}

// 9P filesystem test
void cmd_9ptest(int argc, char** argv) {
    (void)argc; (void)argv;
    
    extern int virtio_9p_open(const char* path, int mode);
    extern int virtio_9p_read(int fid, void* buffer, uint32_t count, uint64_t offset);
    extern void virtio_9p_close(int fid);
    extern bool virtio_9p_is_mounted(void);
    
    if (!virtio_9p_is_mounted()) {
        gfx_print("9P filesystem not mounted\n");
        return;
    }
    
    gfx_print("Testing 9P filesystem...\n");
    gfx_print("Opening /host/test.txt\n");
    
    // P9_OREAD = 0x00
    int fid = virtio_9p_open("/test.txt", 0x00);
    if (fid < 0) {
        gfx_print("Failed to open file\n");
        return;
    }
    
    gfx_print("Reading file...\n");
    char buffer[512];
    int bytes_read = virtio_9p_read(fid, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = 0;
        gfx_print("File contents:\n");
        gfx_print(buffer);
        gfx_print("\n");
    } else {
        gfx_print("Failed to read file\n");
    }
    
    virtio_9p_close(fid);
    gfx_print("Test complete\n");
}

// Stub functions for compatibility
shell_mode_t get_current_mode(void) { return current_mode; }
void set_current_mode(shell_mode_t mode) { current_mode = mode; }
const char* get_mode_string(shell_mode_t mode) { (void)mode; return "normal"; }
void* alloc_temp_buffer(void) { return NULL; }
void release_temp_buffer(void* buffer) { (void)buffer; }
void cmd_bufstatus(int argc, char** argv) { (void)argc; (void)argv; }
// atoi function is provided by string.c