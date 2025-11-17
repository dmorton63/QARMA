#include "vfs.h"
#include "core/string.h"

// Forward declaration for simplefs
extern void simplefs_init(void);
extern void ramdisk_init(void);

#define MAX_FS_DRIVERS 8
static struct fs_driver* fs_drivers[MAX_FS_DRIVERS];
static int fs_driver_count = 0;

static vfs_node_t vfs_root = {
    .name = "/",
    .type = VFS_TYPE_DIR,
    .parent = 0,
    .children = 0,
    .next = 0,
    .size = 0,
    .fs_data = 0,
    .fs = 0,
    .blockdev = 0
};

void vfs_register_fs(struct fs_driver* fs) {
    if (fs_driver_count < MAX_FS_DRIVERS) {
        fs_drivers[fs_driver_count++] = fs;
    }
}


// Find FS driver by name
static struct fs_driver* find_fs_driver(const char* name) {
    for (int i = 0; i < fs_driver_count; ++i) {
        if (strcmp(fs_drivers[i]->name, name) == 0) return fs_drivers[i];
    }
    return 0;
}

// Find node by path (supports nested paths like /ramdisk/file.txt)
static vfs_node_t* vfs_find_node(const char* path) {
    if (!path || strcmp(path, "/") == 0) return &vfs_root;
    
    // Skip leading slash
    if (path[0] == '/') path++;
    
    // Handle nested paths by parsing each component
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    char* component = path_copy;
    char* next_slash;
    vfs_node_t* current = &vfs_root;
    
    while (component && *component) {
        // Find next path separator
        next_slash = component;
        while (*next_slash && *next_slash != '/') next_slash++;
        
        // Null-terminate current component
        if (*next_slash == '/') {
            *next_slash = '\0';
            next_slash++;
        } else {
            next_slash = NULL;
        }
        
        // Search for component in current directory's children
        vfs_node_t* child = current->children;
        current = NULL; // Reset to indicate not found
        
        while (child) {
            if (strcmp(child->name, component) == 0) {
                current = child;
                break;
            }
            child = child->next;
        }
        
        // If component not found, return NULL
        if (!current) return NULL;
        
        // Move to next component
        component = next_slash;
    }
    
    return current;
}

int vfs_mount(const char* devname, const char* fstype, const char* mountpoint) {
    extern void gfx_print(const char*);
    
    gfx_print("[VFS] Mounting device: ");
    if (devname) gfx_print(devname);
    gfx_print(" as ");
    if (fstype) gfx_print(fstype);
    gfx_print(" at ");
    if (mountpoint) gfx_print(mountpoint);
    gfx_print("\\n");
    
    blockdev_t* dev = blockdev_find(devname);
    if (!dev) {
        gfx_print("[VFS] Block device not found\\n");
        return -1;
    }
    
    struct fs_driver* fs = find_fs_driver(fstype);
    if (!fs) {
        gfx_print("[VFS] Filesystem driver not found\\n");
        return -2;
    }
    
    if (fs->probe && !fs->probe(dev)) {
        gfx_print("[VFS] Filesystem probe failed\\n");
        return -3;
    }

    // Create mountpoint node (only supports mounting at root for now)
    vfs_node_t* mp = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    memset(mp, 0, sizeof(vfs_node_t));
    strncpy(mp->name, mountpoint[0] == '/' ? mountpoint + 1 : mountpoint, 63);
    mp->type = VFS_TYPE_DIR;
    mp->parent = &vfs_root;
    mp->fs = fs;
    mp->blockdev = dev;
    mp->next = vfs_root.children;
    vfs_root.children = mp;

    gfx_print("[VFS] Created mount point, calling fs->mount\\n");
    if (fs->mount) {
        int mount_res = fs->mount(dev, mp);
        if (mount_res == 0) {
            gfx_print("[VFS] Filesystem mount successful\\n");
        } else {
            gfx_print("[VFS] Filesystem mount failed\\n");
        }
        return mount_res;
    }
    
    return 0;
}


vfs_node_t* vfs_open(const char* path) {
    extern void gfx_print(const char*);
    
    // Debug: show what path we're trying to open
    gfx_print("[VFS] Attempting to open: ");
    if (path) gfx_print(path);
    gfx_print("\n");
    
    vfs_node_t* node = vfs_find_node(path);
    if (node) {
        gfx_print("[VFS] File found successfully\n");
    } else {
        gfx_print("[VFS] File not found\n");
    }
    
    return node;
}


int vfs_read(vfs_node_t* node, void* buf, size_t size, size_t offset) {
    if (!node || !buf || node->type != VFS_TYPE_FILE) {
        return -1;
    }
    
    // If we have a filesystem driver, use it
    if (node->fs && node->blockdev) {
        // For our simple filesystem, use the direct read function
        if (strcmp(node->fs->name, "simplefs") == 0) {
            // Call simplefs read function directly
            extern int simplefs_read_file(const char* filename, void* buffer, size_t size, size_t offset);
            return simplefs_read_file(node->name, buf, size, offset);
        }
    }
    
    // Fallback: return 0 for unknown filesystems
    return 0;
}

/**
 * Initialize VFS system and mount RAM disk
 */
void vfs_init(void) {
    extern void gfx_print(const char*);
    gfx_print("[VFS] Starting VFS initialization...\n");
    
    // Initialize RAM disk
    gfx_print("[VFS] Calling ramdisk_init()...\n");
    ramdisk_init();
    gfx_print("[VFS] ramdisk_init() completed.\n");
    
    // Initialize simple filesystem driver  
    gfx_print("[VFS] Calling simplefs_init()...\n");
    simplefs_init();
    gfx_print("[VFS] simplefs_init() completed.\n");
    
    // Mount RAM disk at root
    gfx_print("[VFS] Calling vfs_mount()...\n");
    int mount_result = vfs_mount("ram0", "simplefs", "ramdisk");
    gfx_print("[VFS] vfs_mount() completed.\n");
    
    if (mount_result == 0) {
        // VFS mounted successfully - debug output
        gfx_print("[VFS] RAM disk mounted successfully at /ramdisk\n");
    } else {
        gfx_print("[VFS] Failed to mount RAM disk!\n");
    }
    
    gfx_print("[VFS] VFS initialization complete.\n");
}
