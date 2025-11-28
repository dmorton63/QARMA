/*
 * QARMA - Filesystem Commands
 * 
 * Commands for file and directory operations
 */

#include "command.h"
#include "vfs.h"
#include "string.h"
#include "config.h"

extern void gfx_print(const char* str);
extern void gfx_print_hex(uint32_t value);

// Current working directory
static char g_current_dir[256] = "/";

// Build absolute path from current directory and input (if relative)
static void build_abs_path(const char* input, char* out, size_t outsz) {
    if (!input || !*input) {
        // No input provided, use current directory
        strncpy(out, g_current_dir, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    if (input[0] == '/') {
        // Already absolute
        strncpy(out, input, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    // Relative to current directory
    if (strcmp(g_current_dir, "/") == 0) {
        out[0] = '/';
        strncpy(out + 1, input, outsz - 2);
        out[outsz - 1] = '\0';
    } else {
        strncpy(out, g_current_dir, outsz - 1);
        out[outsz - 1] = '\0';
        size_t len = strlen(out);
        if (len < outsz - 1) {
            out[len] = '/';
            len++;
        }
        if (len < outsz) {
            strncpy(out + len, input, outsz - len - 1);
            out[outsz - 1] = '\0';
        }
    }
}

// List files and directories
void cmd_ls(int argc, char** argv) {
    char abs_path[256];
    if (argc > 1) {
        build_abs_path(argv[1], abs_path, sizeof(abs_path));
    } else {
        strncpy(abs_path, g_current_dir, sizeof(abs_path) - 1);
        abs_path[sizeof(abs_path) - 1] = '\0';
    }
    
    vfs_node_t* node = vfs_open(abs_path);
    if (!node) {
        gfx_print("ls: ");
        gfx_print(abs_path);
        gfx_print(": No such directory\n");
        return;
    }
    
    if (node->type != VFS_TYPE_DIR) {
        gfx_print("ls: ");
        gfx_print(abs_path);
        gfx_print(": Not a directory\n");
        return;
    }
    
    gfx_print("Contents of ");
    gfx_print(abs_path);
    gfx_print(":\n");
    
    vfs_node_t* child = node->children;
    if (!child) {
        gfx_print("  (empty)\n");
        return;
    }
    
    // Count children
    int count = 0;
    for (vfs_node_t* it = child; it; it = it->next) {
        count++;
    }

    // Collect into an array for sorting
    vfs_node_t* entries[count];
    int idx = 0;
    for (vfs_node_t* it = child; it; it = it->next) {
        entries[idx++] = it;
    }

    // Simple insertion sort by name (alphabetical)
    for (int i = 1; i < count; i++) {
        vfs_node_t* key = entries[i];
        int j = i - 1;
        while (j >= 0 && strcmp(entries[j]->name, key->name) > 0) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }

    // Print dot entries first
    gfx_print("  .\n");
    gfx_print("  ..\n");

    // Then print sorted entries
    for (int i = 0; i < count; i++) {
        gfx_print("  ");
        gfx_print(entries[i]->name);
        if (entries[i]->type == VFS_TYPE_DIR) {
            gfx_print("/");
        }
        gfx_print("\n");
    }
}

// Change directory
void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        // cd with no args goes to root
        strncpy(g_current_dir, "/", sizeof(g_current_dir) - 1);
        gfx_print("/\n");
        return;
    }
    
    const char* target = argv[1];
    
    // Handle special cases
    if (strcmp(target, ".") == 0) {
        // Current directory - do nothing
        return;
    }
    
    if (strcmp(target, "..") == 0) {
        // Go to parent directory
        if (strcmp(g_current_dir, "/") == 0) {
            gfx_print("Already at root\n");
            return;
        }
        
        // Find last slash
        int last_slash = -1;
        for (int i = 0; g_current_dir[i]; i++) {
            if (g_current_dir[i] == '/') {
                last_slash = i;
            }
        }
        
        if (last_slash <= 0) {
            strncpy(g_current_dir, "/", sizeof(g_current_dir) - 1);
        } else {
            g_current_dir[last_slash] = '\0';
        }
        
        gfx_print(g_current_dir);
        gfx_print("\n");
        return;
    }
    
    // Build new path
    char new_path[256];
    if (target[0] == '/') {
        // Absolute path
        strncpy(new_path, target, sizeof(new_path) - 1);
    } else {
        // Relative path
        if (strcmp(g_current_dir, "/") == 0) {
            new_path[0] = '/';
            strncpy(new_path + 1, target, sizeof(new_path) - 2);
        } else {
            strncpy(new_path, g_current_dir, sizeof(new_path) - 1);
            int len = strlen(new_path);
            if (len < (int)sizeof(new_path) - 1) {
                new_path[len] = '/';
                strncpy(new_path + len + 1, target, sizeof(new_path) - len - 2);
            }
        }
    }
    new_path[sizeof(new_path) - 1] = '\0';
    
    // Check if directory exists
    vfs_node_t* node = vfs_open(new_path);
    if (!node) {
        gfx_print("cd: ");
        gfx_print(new_path);
        gfx_print(": No such directory\n");
        return;
    }
    
    if (node->type != VFS_TYPE_DIR) {
        gfx_print("cd: ");
        gfx_print(new_path);
        gfx_print(": Not a directory\n");
        return;
    }
    
    // Change directory
    strncpy(g_current_dir, new_path, sizeof(g_current_dir) - 1);
    g_current_dir[sizeof(g_current_dir) - 1] = '\0';
    
    SERIAL_LOG("[CD] Changed to: ");
    SERIAL_LOG(g_current_dir);
    SERIAL_LOG("\n");
}

// Print working directory
void cmd_pwd(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    gfx_print(g_current_dir);
    gfx_print("\n");
}

// Create directory
void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: mkdir <directory>\n");
        return;
    }
    
    gfx_print("mkdir: Directory creation not yet implemented\n");
    gfx_print("Note: VFS layer needs directory creation support\n");
}

// Remove directory
void cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: rmdir <directory>\n");
        return;
    }
    
    gfx_print("rmdir: Directory removal not yet implemented\n");
}

// Display file contents
void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: cat <filename>\n");
        return;
    }
    
    char abs_path[256];
    build_abs_path(argv[1], abs_path, sizeof(abs_path));
    
    gfx_print("Attempting to read: ");
    gfx_print(abs_path);
    gfx_print("\n");
    
    vfs_node_t* node = vfs_open(abs_path);
    if (!node) {
        gfx_print("Error: File not found\n");
        return;
    }
    
    if (node->type != VFS_TYPE_FILE) {
        gfx_print("Error: Not a file\n");
        return;
    }
    
    // Read file content
    char buffer[512];
    int bytes_read = vfs_read(node, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        gfx_print(buffer);
        gfx_print("\n");
    } else {
        gfx_print("Error: Could not read file\n");
    }
}

// Remove file
void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        gfx_print("Usage: rm <filename>\n");
        return;
    }
    
    gfx_print("rm: File deletion not yet implemented\n");
}

// Copy file
void cmd_cp(int argc, char** argv) {
    if (argc < 3) {
        gfx_print("Usage: cp <source> <destination>\n");
        return;
    }
    
    gfx_print("cp: File copying not yet implemented\n");
}

// Move/rename file
void cmd_mv(int argc, char** argv) {
    if (argc < 3) {
        gfx_print("Usage: mv <source> <destination>\n");
        return;
    }
    
    gfx_print("mv: File moving not yet implemented\n");
}

// Show disk information
void cmd_disk(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    gfx_print("=== Disk Information ===\n");
    gfx_print("\n");
    gfx_print("ATA Primary Master (hda):\n");
    gfx_print("  Status: Detected\n");
    gfx_print("  Size: 10 MB\n");
    gfx_print("  Filesystem: FAT16\n");
    gfx_print("  Mount point: /disk\n");
    gfx_print("\n");
    gfx_print("RAM Disk (ram0):\n");
    gfx_print("  Size: 128 KB\n");
    gfx_print("  Filesystem: SimpleFS\n");
    gfx_print("  Mount point: /ramdisk\n");
    gfx_print("\n");
    gfx_print("CD-ROM:\n");
    gfx_print("  Filesystem: ISO9660\n");
    gfx_print("  Mount point: /cdrom\n");
}

// Directory listing (alias for ls)
void cmd_dir(int argc, char** argv) {
    cmd_ls(argc, argv);
}
