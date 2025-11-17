#ifndef COMMAND_H
#define COMMAND_H

#include "kernel_types.h" 
#include "config.h"

// Command system constants
#define MAX_ARGS 16
#define MAX_COMMAND_NAME_LEN 32
#define TEMP_BUFFER_SIZE 4096
#define MAX_TEMP_BUFFERS 8

// Shell modes
typedef enum {
    MODE_NORMAL,
    MODE_VERBOSE,
    MODE_DEBUG,
    MODE_SAFE,
    MODE_HEX,
    MODE_COLOR
} shell_mode_t;

// Command function pointer type
typedef void (*command_func_t)(int argc, char** argv);

// Command entry structure
typedef struct {
    const char* name;
    command_func_t function;
    const char* description;
    const char* usage;
} command_entry_t;

// Command result codes
typedef enum {
    CMD_SUCCESS = 1,
    CMD_ERROR_INVALID_ARGS,
    CMD_ERROR_FILE_NOT_FOUND,
    CMD_ERROR_PERMISSION_DENIED,
    CMD_ERROR_NO_MEMORY,
    CMD_ERROR_UNKNOWN_COMMAND,
    CMD_ERROR_GENERAL
} command_result_t;

//void cmd_mouse(int argc, char** argv);

//void show_mouse_info(void);

// Function prototypes
bool command_init(void);
// static bool get_volume_bpb_for_command(uint8_t vol_index, const BPB **bpb_out, const char *cmd_name);
// static void release_volume_bpb_for_command(uint8_t vol_index);
// static uint32_t get_root_dir_start_sector(const BPB *bpb);
// static uint32_t get_root_dir_sectors(const BPB *bpb);
// static uint32_t get_bytes_per_sector(const BPB *bpb);
command_result_t execute_command(const char *input);
int parse_input(char* input, char* argv[], int max_args);
bool is_valid_command(const char* name);

bool check_for_command(const char *cmd);

// Buffer management
void* alloc_temp_buffer(void);
void release_temp_buffer(void* buffer);
void cmd_bufstatus(int argc, char** argv);

// Command implementations
void cmd_help(int argc, char** argv);
void cmd_echo(int argc, char** argv);
void cmd_cls(int argc, char** argv);
// void cmd_pwd(int argc, char** argv);
// void cmd_ls(int argc, char** argv);
// void cmd_dir(int argc, char** argv);
// void cmd_mkdir(int argc, char** argv);
// void cmd_mount(int argc, char** argv);
// void cmd_readit(int argc, char** argv);
// void cmd_bpbinfo(int argc, char** argv);
//void cmd_diskdebug(int argc, char **argv);
//void cmd_setmode(int argc, char **argv);
//void cmd_debug(int argc, char** argv);
//void cmd_usb(int argc, char** argv);
//void cmd_pci(int argc, char** argv);
void cmd_shutdown(int argc, char** argv);
void cmd_reboot(int argc, char** argv);
void cmd_version(int argc, char** argv);
void cmd_clear(int argc, char** argv);
void cmd_exit(int argc, char** argv);

// Network commands
void cmd_ifconfig(int argc, char** argv);
void cmd_ifup(int argc, char** argv);
void cmd_ifdown(int argc, char** argv);
void cmd_ping(int argc, char** argv);
void cmd_arp(int argc, char** argv);

// Pipeline commands
void cmd_pipeline(int argc, char** argv);

// Window commands
void cmd_window(int argc, char** argv);
void cmd_winloop(int argc, char** argv);

int atoi(const char* str);
// Utility functions
const char* get_mode_string(shell_mode_t mode);
shell_mode_t get_current_mode(void);
void set_current_mode(shell_mode_t mode);

// Global state
extern shell_mode_t current_mode;

#endif // COMMAND_H