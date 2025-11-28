#include "vfs.h"
#include "string.h"
#include "config.h"

// VirtIO 9P hooks
extern bool virtio_9p_is_mounted(void);
extern int virtio_9p_open(const char* path, int mode);
extern int virtio_9p_read(int fid, void* buffer, uint32_t count, uint64_t offset);
extern int virtio_9p_write(int fid, const void* buffer, uint32_t count, uint64_t offset);
extern void virtio_9p_close(int fid);
extern bool virtio_9p_readdir(const char* path, void (*callback)(const char* name, bool is_dir));

typedef struct {
    uint32_t magic; // '9PFD'
    int fid;
} vfs_9p_file_t;
#ifdef CONFIG_9P_DEBUG
#define VFS9P_LOG(msg) SERIAL_LOG(msg)
#else
#define VFS9P_LOG(msg) ((void)0)
#endif

static vfs_node_t* g_vfs9p_parent = NULL;
static void vfs9p_add_child_cb(const char* name, bool is_dir) {
    if (!g_vfs9p_parent || !name) return;
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (!node) return;
    memset(node, 0, sizeof(vfs_node_t));
    strncpy(node->name, name, 63);
    node->type = is_dir ? VFS_TYPE_DIR : VFS_TYPE_FILE;
    node->parent = g_vfs9p_parent;
    // push front
    node->next = g_vfs9p_parent->children;
    g_vfs9p_parent->children = node;
}

#define VFS_9P_MAGIC 0x39504644

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
    
    // Special case: host 9P mount passthrough
    if (path && virtio_9p_is_mounted()) {
        if (strncmp(path, "/host/", 6) == 0) {
            const char* rel = path + 5; // points to "/..." start
            if (*rel == '/') rel++;
            if (*rel == '\0') {
                // Opening the mount directory itself
                vfs_node_t* dir = (vfs_node_t*)malloc(sizeof(vfs_node_t));
                if (!dir) return NULL;
                memset(dir, 0, sizeof(vfs_node_t));
                strncpy(dir->name, "host", 63);
                dir->type = VFS_TYPE_DIR;
                // Populate children via 9P readdir
                g_vfs9p_parent = dir;
                dir->children = NULL;
                (void)virtio_9p_readdir("/", vfs9p_add_child_cb);
                g_vfs9p_parent = NULL;
                return dir;
            }
            // Try to treat as directory first: build a node via readdir
            vfs_node_t* dir = (vfs_node_t*)malloc(sizeof(vfs_node_t));
            if (dir) {
                memset(dir, 0, sizeof(vfs_node_t));
                dir->type = VFS_TYPE_DIR;
                // name from last component
                const char* lastc = rel; for (const char* p = rel; *p; ++p) if (*p=='/') lastc = p+1;
                strncpy(dir->name, lastc, 63);
                g_vfs9p_parent = dir;
                dir->children = NULL;
                bool ok = virtio_9p_readdir(rel, vfs9p_add_child_cb);
                g_vfs9p_parent = NULL;
                if (ok) {
                    return dir;
                }
                free(dir);
            }
            // Fall back to opening as file
            int fid = virtio_9p_open(rel, 0x00);
            if (fid >= 0) {
                vfs_node_t* file = (vfs_node_t*)malloc(sizeof(vfs_node_t));
                if (!file) {
                    virtio_9p_close(fid);
                    return NULL;
                }
                memset(file, 0, sizeof(vfs_node_t));
                // Set name from last path component
                const char* last = rel;
                for (const char* p = rel; *p; ++p) if (*p == '/') last = p + 1;
                strncpy(file->name, last, 63);
                file->type = VFS_TYPE_FILE;
                vfs_9p_file_t* meta = (vfs_9p_file_t*)malloc(sizeof(vfs_9p_file_t));
                if (!meta) {
                    virtio_9p_close(fid);
                    free(file);
                    return NULL;
                }
                meta->magic = VFS_9P_MAGIC;
                meta->fid = fid;
                file->fs_data = meta;
                VFS9P_LOG("[VFS] 9P passthrough open OK\n");
                return file;
            } else {
                VFS9P_LOG("[VFS] 9P passthrough open FAILED\n");
                return NULL;
            }
        }
    }

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

    // 9P passthrough
    if (node->fs_data) {
        vfs_9p_file_t* meta = (vfs_9p_file_t*)node->fs_data;
        if (meta->magic == VFS_9P_MAGIC) {
            int r = virtio_9p_read(meta->fid, buf, (uint32_t)size, (uint64_t)offset);
            return r;
        }
    }
    
    // If we have a filesystem driver, use it
    if (node->fs && node->blockdev) {
        // For our simple filesystem, use the direct read function
        // Both simplefs and fat16 use the same underlying storage format
        if (strcmp(node->fs->name, "simplefs") == 0 || strcmp(node->fs->name, "fat16") == 0) {
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
    SERIAL_LOG("[VFS] init: begin\n");
    
    // Initialize RAM disk
    gfx_print("[VFS] Calling ramdisk_init()...\n");
    ramdisk_init();
    gfx_print("[VFS] ramdisk_init() completed.\n");
    
    // Initialize simple filesystem driver  
    gfx_print("[VFS] Calling simplefs_init()...\n");
    simplefs_init();
    gfx_print("[VFS] simplefs_init() completed.\n");
    
    // Initialize VirtIO 9P driver for host filesystem access
    extern bool virtio_9p_init(void);
    extern bool virtio_9p_mount(const char* mount_tag, const char* mount_point);
    
    gfx_print("[VFS] Initializing VirtIO 9P driver...\n");
    VFS9P_LOG("[VFS] 9P: calling virtio_9p_init()\n");
    if (virtio_9p_init()) {
        gfx_print("[VFS] Mounting host shared filesystem...\n");
        VFS9P_LOG("[VFS] 9P: init OK, mounting...\n");
        if (virtio_9p_mount("hostshare", "/host")) {
            gfx_print("[VFS] Host filesystem available at /host\n");
            VFS9P_LOG("[VFS] 9P: mount OK at /host\n");
            // Ensure '/host' appears under root immediately
            // Check if 'host' already exists
            vfs_node_t* child = vfs_root.children;
            vfs_node_t* host_node = NULL;
            while (child) {
                if (strcmp(child->name, "host") == 0) { host_node = child; break; }
                child = child->next;
            }
            if (!host_node) {
                host_node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
                if (host_node) {
                    memset(host_node, 0, sizeof(vfs_node_t));
                    strncpy(host_node->name, "host", 63);
                    host_node->type = VFS_TYPE_DIR;
                    host_node->parent = &vfs_root;
                    // insert at head
                    host_node->next = vfs_root.children;
                    vfs_root.children = host_node;
                    // Populate initial listing via 9P
                    g_vfs9p_parent = host_node;
                    host_node->children = NULL;
                    (void)virtio_9p_readdir("/", vfs9p_add_child_cb);
                    g_vfs9p_parent = NULL;
                }
            }
        } else {
            gfx_print("[VFS] Failed to mount host filesystem\n");
            VFS9P_LOG("[VFS] 9P: mount FAILED\n");
        }
    } else {
        gfx_print("[VFS] No VirtIO 9P device found (host sharing not available)\n");
        VFS9P_LOG("[VFS] 9P: init FAILED (device not found)\n");
    }
    
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
    SERIAL_LOG("[VFS] init: end\n");
}

/**
 * Create a new file or directory
 */
vfs_node_t* vfs_create(const char* path, uint32_t type) {
    extern void* heap_alloc(size_t size);
    extern void gfx_print(const char*);
    
    if (!path) return NULL;
    
    // Check if file already exists
    vfs_node_t* existing = vfs_find_node(path);
    if (existing) {
        SERIAL_LOG("[VFS] File already exists, returning existing node\n");
        return existing;
    }
    
    // Parse the path to get parent directory and filename
    char parent_path[256];
    char filename[64];
    const char* last_slash = NULL;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    vfs_node_t* parent;
    if (last_slash) {
        // Extract parent path and filename
        int parent_len = last_slash - path;
        if (parent_len == 0) {
            parent = &vfs_root;
        } else {
            strncpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            parent = vfs_find_node(parent_path);
        }
        strncpy(filename, last_slash + 1, 63);
        filename[63] = '\0';
    } else {
        parent = &vfs_root;
        strncpy(filename, path, 63);
        filename[63] = '\0';
    }
    
    if (!parent) {
        SERIAL_LOG("[VFS] Parent directory not found\n");
        return NULL;
    }
    
    // Allocate new node
    vfs_node_t* node = (vfs_node_t*)heap_alloc(sizeof(vfs_node_t));
    if (!node) {
        SERIAL_LOG("[VFS] Failed to allocate node\n");
        return NULL;
    }
    
    // Initialize node
    strncpy(node->name, filename, 63);
    node->name[63] = '\0';
    node->type = type;
    node->parent = parent;
    node->children = NULL;
    node->next = NULL;
    node->size = 0;
    node->fs_data = NULL;
    node->fs = parent->fs;
    node->blockdev = parent->blockdev;
    
    // Add to parent's children
    if (!parent->children) {
        parent->children = node;
    } else {
        vfs_node_t* sibling = parent->children;
        while (sibling->next) sibling = sibling->next;
        sibling->next = node;
    }
    
    SERIAL_LOG("[VFS] Created node: ");
    SERIAL_LOG(filename);
    SERIAL_LOG("\n");
    
    return node;
}

/**
 * Write to a file
 */
int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset) {
    extern int simplefs_write_file(const char* filename, const void* buffer, size_t size, size_t offset);
    
    if (!node || !buf || node->type != VFS_TYPE_FILE) {
        SERIAL_LOG("[VFS] Invalid write parameters\n");
        return -1;
    }
    
    // 9P passthrough
    if (node->fs_data) {
        vfs_9p_file_t* meta = (vfs_9p_file_t*)node->fs_data;
        if (meta->magic == VFS_9P_MAGIC) {
            int w = virtio_9p_write(meta->fid, buf, (uint32_t)size, (uint64_t)offset);
            return w;
        }
    }

    // If we have a filesystem driver, use it
    if (node->fs && node->blockdev) {
        // For now, both simplefs and fat16 use the same underlying storage format
        if (strcmp(node->fs->name, "simplefs") == 0 || strcmp(node->fs->name, "fat16") == 0) {
            // Call simplefs write function
            int result = simplefs_write_file(node->name, buf, size, offset);
            if (result > 0) {
                // Update node size if needed
                if (offset + result > node->size) {
                    node->size = offset + result;
                }
            }
            return result;
        }
    }
    
    SERIAL_LOG("[VFS] No filesystem driver for write: ");
    SERIAL_LOG(node->fs ? node->fs->name : "NULL");
    SERIAL_LOG("\n");
    return -1;
}
