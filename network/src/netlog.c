#include "netlog.h"
#include "vfs.h"
#include "string.h"
#include "config.h"
#include "kernel_types.h"

static vfs_node_t* g_netlog_node = NULL;
static size_t g_netlog_off = 0;
static bool g_netlog_failed = false;

// Correct host mount path is /host (virtio 9P), not /shared_files inside guest.
static const char* host_path = "/host/net.log";      // host share if mounted
static const char* fallback_path = "/ramdisk/net.log"; // guaranteed after ramdisk mount

bool netlog_init(void) {
    if (g_netlog_failed) return false;
    if (g_netlog_node) return true;
    extern vfs_node_t* vfs_create(const char* path, uint32_t type);
    extern vfs_node_t* vfs_open(const char* path);
    // Ensure VFS is ready: require /ramdisk to exist and have an fs driver
    vfs_node_t* ramdisk = vfs_open("/ramdisk");
    if (!ramdisk || !ramdisk->fs) {
        // VFS not initialized yet; defer initialization without failing
        return false;
    }
    // Prefer fallback first to ensure we always have a log early in boot, under ramdisk
    g_netlog_node = vfs_create(fallback_path, VFS_TYPE_FILE);
    if (!g_netlog_node) {
        g_netlog_failed = true;
        return false;
    }
    // If host share is mounted, upgrade to host log (append, do not overwrite)
    if (vfs_open("/host") != NULL) {
        // Try open-for-write first (uses 9P write path); if missing create
        extern vfs_node_t* vfs_open_for_write(const char* path);
        vfs_node_t* host_node = vfs_open_for_write(host_path);
        if (!host_node) {
            host_node = vfs_create(host_path, VFS_TYPE_FILE);
        }
        if (host_node) {
            g_netlog_node = host_node;
            // Append to end of existing host file (no overwrite)
            g_netlog_off = host_node->size;
        }
    }
    const char* hdr = "=== QARMA Network Log ===\n";
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    int w = vfs_write(g_netlog_node, hdr, strlen(hdr), g_netlog_off);
    if (w > 0) {
        g_netlog_off += w;
    }
    return true;
}

void netlog_write(const char* msg) {
    if (!msg) return;
    if (!g_netlog_node) {
        if (!netlog_init()) return;
    }
    // Attempt host upgrade if not yet and host now mounted
    if (g_netlog_node && g_netlog_node->parent && strcmp(g_netlog_node->parent->name, "ramdisk") == 0) {
        extern vfs_node_t* vfs_open(const char* path);
        if (vfs_open("/host") != NULL) {
            extern vfs_node_t* vfs_open_for_write(const char* path);
            extern vfs_node_t* vfs_create(const char* path, uint32_t type);
            vfs_node_t* host_node = vfs_open_for_write(host_path);
            if (!host_node) host_node = vfs_create(host_path, VFS_TYPE_FILE);
            if (host_node && host_node != g_netlog_node) {
                g_netlog_node = host_node;
                // Append migration marker at EOF
                g_netlog_off = host_node->size;
                const char* hdr = "=== QARMA Network Log (migrated to host) ===\n";
                extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
                int w2 = vfs_write(g_netlog_node, hdr, strlen(hdr), g_netlog_off);
                if (w2 > 0) g_netlog_off += w2;
            }
        }
    }
    extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    size_t len = strlen(msg);
    if (len == 0) return;
    int w3 = vfs_write(g_netlog_node, msg, len, g_netlog_off);
    if (w3 > 0) g_netlog_off += w3;
}

static char hexbuf[16];
static const char* to_hex(uint32_t v) {
    static const char* digits = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        hexbuf[i] = digits[v & 0xF];
        v >>= 4;
    }
    hexbuf[8] = '\0';
    return hexbuf;
}

void netlog_write_hex(const char* prefix, uint32_t val) {
    if (prefix) netlog_write(prefix);
    netlog_write("0x");
    netlog_write(to_hex(val));
}

void netlog_flush(void) {
    // Currently noop; VFS writes are immediate.
}

// Report backend status (printed to serial)
void netlog_status(void) {
    extern void gfx_print(const char*);
    if (g_netlog_failed) { gfx_print("netlog: failed init\n"); return; }
    if (!g_netlog_node) { gfx_print("netlog: not initialized\n"); return; }
    gfx_print("netlog: backend=");
    if (g_netlog_node->parent) gfx_print(g_netlog_node->parent->name); else gfx_print("(none)");
    gfx_print(" path="); gfx_print(g_netlog_node->name); gfx_print(" off=");
    extern void gfx_print_hex(uint32_t);
    gfx_print_hex((uint32_t)g_netlog_off);
    gfx_print("\n");
}

// Force host migration attempt
void netlog_force_upgrade(void) {
    extern void gfx_print(const char*);
    if (!g_netlog_node) { if (!netlog_init()) { gfx_print("netlog: init failed for force upgrade\n"); return; } }
    if (g_netlog_node && g_netlog_node->parent && strcmp(g_netlog_node->parent->name, "ramdisk") == 0) {
        extern vfs_node_t* vfs_open(const char* path); extern vfs_node_t* vfs_open_for_write(const char* path); extern vfs_node_t* vfs_create(const char* path, uint32_t type); extern int vfs_write(vfs_node_t* node, const void* buf, size_t size, size_t offset);
        if (vfs_open("/host") != NULL) {
            vfs_node_t* host_node = vfs_open_for_write(host_path);
            if (!host_node) host_node = vfs_create(host_path, VFS_TYPE_FILE);
            if (host_node && host_node != g_netlog_node) {
                g_netlog_node = host_node; g_netlog_off = host_node->size; const char* hdr = "=== QARMA Network Log (force migrated) ===\n"; int w = vfs_write(g_netlog_node, hdr, strlen(hdr), g_netlog_off); if (w>0) g_netlog_off += w; gfx_print("netlog: migrated to host\n"); return; }
            gfx_print("netlog: host node unavailable\n");
        } else {
            gfx_print("netlog: host not mounted\n");
        }
    } else {
        gfx_print("netlog: already on host or no parent\n");
    }
}
