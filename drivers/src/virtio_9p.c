/**
 * QARMA - VirtIO 9P File System Driver Implementation
 */

#include "virtio_9p.h"
#include "pci.h"
#include "string.h"
#include "memory/heap.h"
#include "memory/dma_allocator.h"
#include "graphics.h"
#include "config.h"
#include "io.h"

// VirtIO legacy I/O port offsets
#define VIRTIO_PCI_HOST_FEATURES    0
#define VIRTIO_PCI_GUEST_FEATURES   4
#define VIRTIO_PCI_QUEUE_PFN        8
#define VIRTIO_PCI_QUEUE_NUM        12
#define VIRTIO_PCI_QUEUE_SEL        14
#define VIRTIO_PCI_QUEUE_NOTIFY     16
#define VIRTIO_PCI_STATUS           18
#define VIRTIO_PCI_ISR              19
#define VIRTIO_PCI_CONFIG           20

// Virtqueue structure
#define VIRTQUEUE_SIZE 128

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTQUEUE_SIZE];
} __attribute__((packed)) virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTQUEUE_SIZE];
} __attribute__((packed)) virtq_used_t;

typedef struct {
    virtq_desc_t desc[VIRTQUEUE_SIZE];
    virtq_avail_t avail;
    uint8_t padding[4096 - (sizeof(virtq_desc_t) * VIRTQUEUE_SIZE + sizeof(virtq_avail_t)) % 4096];
    virtq_used_t used;
    uint16_t last_used_idx;
    uint16_t free_head;
    uint16_t num_free;
} virtqueue_t;

// 9P message header
typedef struct {
    uint32_t size;
    uint8_t type;
    uint16_t tag;
} __attribute__((packed)) p9_header_t;

// 9P Qid (unique file identifier)
typedef struct {
    uint8_t type;
    uint32_t version;
    uint64_t path;
} __attribute__((packed)) p9_qid_t;

// Driver state
static struct {
    bool initialized;
    bool mounted;
    uint32_t pci_bus;
    uint32_t pci_device;
    uint32_t pci_function;
    uint16_t io_base;
    char mount_point[64];
    char mount_tag[32];
    uint32_t next_fid;
    uint32_t next_tag;
    virtqueue_t* vq;
    uint64_t vq_phys;
    uint8_t* msg_buffer;
    uint64_t msg_phys;
    uint32_t msize;  // Maximum message size
    p9_qid_t root_qid;
} g_9p_state = {0};

// Virtqueue initialization
static void virtqueue_init(virtqueue_t* vq) {
    memset(vq, 0, sizeof(virtqueue_t));
    vq->last_used_idx = 0;
    vq->free_head = 0;
    vq->num_free = VIRTQUEUE_SIZE;
    
    // Initialize free list
    for (uint16_t i = 0; i < VIRTQUEUE_SIZE - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[VIRTQUEUE_SIZE - 1].next = 0;
}

// Allocate descriptor chain
static uint16_t virtqueue_alloc_desc_chain(virtqueue_t* vq, uint16_t count) {
    if (vq->num_free < count) return 0xFFFF;
    
    uint16_t head = vq->free_head;
    uint16_t current = head;
    
    for (uint16_t i = 0; i < count; i++) {
        vq->num_free--;
        if (i == count - 1) {
            vq->free_head = vq->desc[current].next;
            vq->desc[current].flags = 0;
            vq->desc[current].next = 0;
        } else {
            uint16_t next = vq->desc[current].next;
            vq->desc[current].flags = 1; // VIRTQ_DESC_F_NEXT
            current = next;
        }
    }
    
    return head;
}

// Send request and wait for response
static bool virtio_9p_rpc(void* request, uint32_t req_len, void* response, uint32_t* resp_len) {
    virtqueue_t* vq = g_9p_state.vq;
    SERIAL_LOG("[9P] RPC: begin\n");
    
    // Allocate descriptors (one for request, one for response)
    uint16_t head = virtqueue_alloc_desc_chain(vq, 2);
    if (head == 0xFFFF) {
        SERIAL_LOG("[9P] ERROR: No free descriptors\n");
        return false;
    }
    
    // Compute physical addresses for request/response within the DMA buffer
    uint64_t req_phys = 0;
    uint64_t resp_phys = 0;
    if (g_9p_state.msg_buffer && g_9p_state.msg_phys) {
        req_phys = g_9p_state.msg_phys + ((uint8_t*)request - g_9p_state.msg_buffer);
        resp_phys = g_9p_state.msg_phys + ((uint8_t*)response - g_9p_state.msg_buffer);
    }
    
    // Setup request descriptor
    vq->desc[head].addr = req_phys ? req_phys : (uint64_t)(uintptr_t)request;
    vq->desc[head].len = req_len;
    vq->desc[head].flags = 1; // VIRTQ_DESC_F_NEXT
    
    // Setup response descriptor
    uint16_t resp_desc = vq->desc[head].next;
    vq->desc[resp_desc].addr = resp_phys ? resp_phys : (uint64_t)(uintptr_t)response;
    vq->desc[resp_desc].len = *resp_len;
    vq->desc[resp_desc].flags = 2; // VIRTQ_DESC_F_WRITE
    
    // Add to available ring
    uint16_t avail_idx = vq->avail.idx;
    vq->avail.ring[avail_idx % VIRTQUEUE_SIZE] = head;
    vq->avail.idx++;
    
    // Notify device
    SERIAL_LOG("[9P] RPC: notify queue 0\n");
    outw(g_9p_state.io_base + VIRTIO_PCI_QUEUE_NOTIFY, 0);
    
    // Wait for response (simple polling - production should use interrupts)
    uint32_t timeout = 1000000;
    while (vq->used.idx == vq->last_used_idx && timeout-- > 0) {
        // small pause or CPU relax could be inserted here
    }
    
    if (timeout == 0) {
        SERIAL_LOG("[9P] ERROR: Request timeout\n");
        return false;
    }
    
    // Get response length
    SERIAL_LOG("[9P] RPC: response received\n");
    virtq_used_elem_t* used_elem = &vq->used.ring[vq->last_used_idx % VIRTQUEUE_SIZE];
    *resp_len = used_elem->len;
    vq->last_used_idx++;
    
    // Free descriptors
    vq->desc[resp_desc].next = vq->free_head;
    vq->free_head = head;
    vq->num_free += 2;
    
    SERIAL_LOG("[9P] RPC: end\n");
    return true;
}

// Initialize VirtIO device
bool virtio_9p_init(void) {
    SERIAL_LOG("[9P] Scanning for VirtIO 9P device...\n");
    
    // Scan PCI for VirtIO 9P device
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t device = 0; device < 32; device++) {
            uint16_t vendor = pci_read_config_word(bus, device, 0, 0);
            if (vendor == 0xFFFF || vendor == 0x0000) continue;
            
            uint16_t device_id = pci_read_config_word(bus, device, 0, 2);
            
            // Check for VirtIO 9P
            if (vendor == VIRTIO_VENDOR_ID && device_id == VIRTIO_9P_DEVICE_ID) {
                SERIAL_LOG("[9P] Found VirtIO 9P device at PCI ");
                serial_debug_hex(bus);
                SERIAL_LOG(":");
                serial_debug_hex(device);
                SERIAL_LOG("\n");
                
                g_9p_state.pci_bus = bus;
                g_9p_state.pci_device = device;
                g_9p_state.pci_function = 0;
                
                // Read BAR0 for I/O base (legacy VirtIO)
                uint32_t bar0 = pci_read_config_dword(bus, device, 0, 0x10);
                if (bar0 & 1) {  // I/O space
                    g_9p_state.io_base = bar0 & ~3;
                    SERIAL_LOG("[9P] I/O base: 0x");
                    serial_debug_hex(g_9p_state.io_base);
                    SERIAL_LOG("\n");
                }
                
                // Reset device
                SERIAL_LOG("[9P] Resetting device status=0\n");
                outb(g_9p_state.io_base + VIRTIO_PCI_STATUS, 0);
                
                // Set ACKNOWLEDGE status
                SERIAL_LOG("[9P] Setting ACK\n");
                outb(g_9p_state.io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK);
                
                // Set DRIVER status
                SERIAL_LOG("[9P] Setting DRIVER\n");
                outb(g_9p_state.io_base + VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);
                
                // Read features
                uint32_t features = inl(g_9p_state.io_base + VIRTIO_PCI_HOST_FEATURES);
                SERIAL_LOG("[9P] Device features: 0x");
                serial_debug_hex(features);
                SERIAL_LOG("\n");
                
                // Write guest features (accept all for now)
                outl(g_9p_state.io_base + VIRTIO_PCI_GUEST_FEATURES, features);
                // Set FEATURES_OK and verify
                uint8_t status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
                SERIAL_LOG("[9P] Setting FEATURES_OK\n");
                outb(g_9p_state.io_base + VIRTIO_PCI_STATUS, status);
                uint8_t status_read = inb(g_9p_state.io_base + VIRTIO_PCI_STATUS);
                if ((status_read & VIRTIO_STATUS_FEATURES_OK) == 0) {
                    SERIAL_LOG("[9P] ERROR: Device did not accept features\n");
                    return false;
                }
                
                // Allocate virtqueue (DMA-safe, 2 pages)
                SERIAL_LOG("[9P] Allocating virtqueue (DMA pool 64KB)\n");
                void* vq_virt = dma_allocator_alloc(64 * 1024, 4096, 0);
                g_9p_state.vq = (virtqueue_t*)vq_virt;
                g_9p_state.vq_phys = dma_allocator_get_phys(vq_virt);
                if (!g_9p_state.vq || g_9p_state.vq_phys == 0) {
                    SERIAL_LOG("[9P] ERROR: Failed to allocate virtqueue\n");
                    return false;
                }
                SERIAL_LOG("[9P] Virtqueue allocated virt=0x");
                serial_debug_hex((uint32_t)(uintptr_t)g_9p_state.vq);
                SERIAL_LOG(" phys=0x");
                serial_debug_hex((uint32_t)(g_9p_state.vq_phys));
                SERIAL_LOG("\n");
                memset(g_9p_state.vq, 0, 8192);
                virtqueue_init(g_9p_state.vq);
                
                // Setup virtqueue 0
                SERIAL_LOG("[9P] Selecting queue 0\n");
                outw(g_9p_state.io_base + VIRTIO_PCI_QUEUE_SEL, 0);
                // Optionally set queue size if available; assume default
                SERIAL_LOG("[9P] Setting QUEUE_PFN (phys>>12)=0x");
                serial_debug_hex((uint32_t)(g_9p_state.vq_phys >> 12));
                SERIAL_LOG("\n");
                outl(g_9p_state.io_base + VIRTIO_PCI_QUEUE_PFN, (uint32_t)(g_9p_state.vq_phys >> 12));
                
                // Allocate message buffer (DMA-safe, 2 pages)
                SERIAL_LOG("[9P] Allocating message buffer (DMA pool 64KB)\n");
                void* msg_virt = dma_allocator_alloc(64 * 1024, 4096, 0);
                g_9p_state.msg_buffer = (uint8_t*)msg_virt;
                g_9p_state.msg_phys = dma_allocator_get_phys(msg_virt);
                if (!g_9p_state.msg_buffer || g_9p_state.msg_phys == 0) {
                    SERIAL_LOG("[9P] ERROR: Failed to allocate message buffer\n");
                    return false;
                }
                SERIAL_LOG("[9P] Message buffer allocated virt=0x");
                serial_debug_hex((uint32_t)(uintptr_t)g_9p_state.msg_buffer);
                SERIAL_LOG(" phys=0x");
                serial_debug_hex((uint32_t)(g_9p_state.msg_phys));
                SERIAL_LOG("\n");
                memset(g_9p_state.msg_buffer, 0, 8192);
                g_9p_state.msize = 8192;
                
                // Set DRIVER_OK status
                SERIAL_LOG("[9P] Setting DRIVER_OK\n");
                outb(g_9p_state.io_base + VIRTIO_PCI_STATUS, status | VIRTIO_STATUS_DRIVER_OK);
                
                g_9p_state.initialized = true;
                g_9p_state.next_fid = P9_ROOT_FID + 1;
                g_9p_state.next_tag = 1;
                
                SERIAL_LOG("[9P] VirtIO driver initialized\n");
                return true;
            }
        }
    }
    
    SERIAL_LOG("[9P] No VirtIO 9P device found\n");
    return false;
}

// Helper to write string to buffer
static uint32_t write_string(uint8_t* buf, const char* str) {
    uint16_t len = strlen(str);
    *(uint16_t*)buf = len;
    memcpy(buf + 2, str, len);
    return 2 + len;
}

// Helper to read string from buffer
static uint32_t read_string(uint8_t* buf, char* str, uint32_t max_len) {
    uint16_t len = *(uint16_t*)buf;
    if (len >= max_len) len = max_len - 1;
    memcpy(str, buf + 2, len);
    str[len] = 0;
    return 2 + *(uint16_t*)buf;
}

// Mount filesystem
bool virtio_9p_mount(const char* mount_tag, const char* mount_point) {
    if (!g_9p_state.initialized) {
        SERIAL_LOG("[9P] ERROR: Driver not initialized\n");
        return false;
    }
    
    if (g_9p_state.mounted) {
        SERIAL_LOG("[9P] WARNING: Already mounted\n");
        return true;
    }
    
    // Store mount information
    strncpy(g_9p_state.mount_tag, mount_tag, sizeof(g_9p_state.mount_tag) - 1);
    strncpy(g_9p_state.mount_point, mount_point, sizeof(g_9p_state.mount_point) - 1);
    
    SERIAL_LOG("[9P] Mounting '");
    SERIAL_LOG(mount_tag);
    SERIAL_LOG("' at '");
    SERIAL_LOG(mount_point);
    SERIAL_LOG("'\n");
    
    uint8_t* req = g_9p_state.msg_buffer;
    uint8_t* resp = g_9p_state.msg_buffer + 4096;
    
    // Step 1: TVERSION - negotiate protocol version
    SERIAL_LOG("[9P] Sending TVERSION...\n");
    
    uint8_t* p = req;
    p += 4; // Skip size (fill later)
    *p++ = P9_TVERSION;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = g_9p_state.msize;
    p += 4;
    p += write_string(p, "9P2000.L");
    
    uint32_t req_size = p - req;
    *(uint32_t*)req = req_size;
    
    uint32_t resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TVERSION failed\n");
        return false;
    }
    
    // Parse RVERSION
    if (resp[4] != P9_RVERSION) {
        SERIAL_LOG("[9P] ERROR: Expected RVERSION\n");
        return false;
    }
    
    uint32_t server_msize = *(uint32_t*)(resp + 7);
    if (server_msize < g_9p_state.msize) {
        g_9p_state.msize = server_msize;
    }
    
    char version[32];
    read_string(resp + 11, version, sizeof(version));
    SERIAL_LOG("[9P] Version negotiated: ");
    SERIAL_LOG(version);
    SERIAL_LOG(" msize=");
    serial_debug_hex(g_9p_state.msize);
    SERIAL_LOG("\n");
    
    // Step 2: TATTACH - attach to root (9P2000.L requires n_uname)
    SERIAL_LOG("[9P] Sending TATTACH...\n");
    
    p = req;
    p += 4; // Skip size
    *p++ = P9_TATTACH;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = P9_ROOT_FID;
    p += 4;
    *(uint32_t*)p = P9_NOFID; // auth fid
    p += 4;
    p += write_string(p, "root"); // uname
    p += write_string(p, mount_tag); // aname (mount tag)
    *(uint32_t*)p = 0xFFFFFFFF; // n_uname = -1 (no mapping)
    p += 4;
    
    req_size = p - req;
    *(uint32_t*)req = req_size;
    
    resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TATTACH failed\n");
        return false;
    }
    
    // Parse RATTACH
    if (resp[4] != P9_RATTACH) {
        SERIAL_LOG("[9P] ERROR: Expected RATTACH\n");
        return false;
    }
    
    // Extract root QID
    p9_qid_t* qid = (p9_qid_t*)(resp + 7);
    g_9p_state.root_qid = *qid;
    
    g_9p_state.mounted = true;
    
    SERIAL_LOG("[9P] Mount successful - root qid path=0x");
    serial_debug_hex((uint32_t)qid->path);
    SERIAL_LOG("\n");
    
    gfx_print("[9P] Host filesystem mounted at ");
    gfx_print(mount_point);
    gfx_print("\n");
    
    return true;
}

// Open file
int virtio_9p_open(const char* path, int mode) {
    if (!g_9p_state.mounted) {
        SERIAL_LOG("[9P] ERROR: Filesystem not mounted\n");
        return -1;
    }
    
    SERIAL_LOG("[9P] Opening file: ");
    SERIAL_LOG(path);
    SERIAL_LOG("\n");
    
    // Allocate new FID
    uint32_t fid = g_9p_state.next_fid++;
    
    uint8_t* req = g_9p_state.msg_buffer;
    uint8_t* resp = g_9p_state.msg_buffer + 4096;
    uint8_t* p;
    uint32_t resp_size;
    
    // Parse path into components (skip leading /)
    const char* path_start = path;
    if (*path_start == '/') path_start++;
    
    char components[16][64];
    int num_components = 0;
    const char* start = path_start;
    
    while (*start && num_components < 16) {
        const char* end = start;
        while (*end && *end != '/') end++;
        
        int len = end - start;
        if (len > 0 && len < 64) {
            memcpy(components[num_components], start, len);
            components[num_components][len] = 0;
            num_components++;
        }
        
        if (*end == '/') start = end + 1;
        else break;
    }
    
    // TWALK from root to file
    SERIAL_LOG("[9P] Walking ");
    serial_debug_hex(num_components);
    SERIAL_LOG(" components\n");
    
    p = req;
    p += 4; // Skip size
    *p++ = P9_TWALK;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = P9_ROOT_FID;
    p += 4;
    *(uint32_t*)p = fid;
    p += 4;
    *(uint16_t*)p = num_components;
    p += 2;
    
    for (int i = 0; i < num_components; i++) {
        p += write_string(p, components[i]);
    }
    
    uint32_t req_size = p - req;
    *(uint32_t*)req = req_size;
    
    resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TWALK failed\n");
        return -1;
    }
    
    if (resp[4] != P9_RWALK) {
        SERIAL_LOG("[9P] ERROR: Expected RWALK\n");
        return -1;
    }
    
    // TOPEN to open the file
    SERIAL_LOG("[9P] Opening FID ");
    serial_debug_hex(fid);
    SERIAL_LOG("\n");
    
    p = req;
    p += 4;
    *p++ = P9_TOPEN;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = fid;
    p += 4;
    *(uint32_t*)p = mode;
    p += 4;
    
    req_size = p - req;
    *(uint32_t*)req = req_size;
    
    resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TOPEN failed\n");
        return -1;
    }
    
    if (resp[4] != P9_ROPEN) {
        SERIAL_LOG("[9P] ERROR: Expected ROPEN\n");
        return -1;
    }
    
    SERIAL_LOG("[9P] File opened - FID: ");
    serial_debug_hex(fid);
    SERIAL_LOG("\n");
    
    return fid;
}

// Read from file
int virtio_9p_read(int fid, void* buffer, uint32_t count, uint64_t offset) {
    if (!g_9p_state.mounted) return -1;
    
    SERIAL_LOG("[9P] Reading FID ");
    serial_debug_hex(fid);
    SERIAL_LOG(" count=");
    serial_debug_hex(count);
    SERIAL_LOG(" offset=");
    serial_debug_hex((uint32_t)offset);
    SERIAL_LOG("\n");
    
    uint8_t* req = g_9p_state.msg_buffer;
    uint8_t* resp = g_9p_state.msg_buffer + 4096;
    
    // Limit read size to fit in message buffer
    if (count > g_9p_state.msize - 24) {
        count = g_9p_state.msize - 24;
    }
    
    uint8_t* p = req;
    p += 4;
    *p++ = P9_TREAD;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = fid;
    p += 4;
    *(uint64_t*)p = offset;
    p += 8;
    *(uint32_t*)p = count;
    p += 4;
    
    uint32_t req_size = p - req;
    *(uint32_t*)req = req_size;
    
    uint32_t resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TREAD failed\n");
        return -1;
    }
    
    if (resp[4] != P9_RREAD) {
        SERIAL_LOG("[9P] ERROR: Expected RREAD\n");
        return -1;
    }
    
    // Data starts at offset 7 with a 4-byte count prefix
    uint32_t data_count = *(uint32_t*)(resp + 7);
    memcpy(buffer, resp + 11, data_count);
    
    SERIAL_LOG("[9P] Read ");
    serial_debug_hex(data_count);
    SERIAL_LOG(" bytes\n");
    
    return data_count;
}

// Write to file
int virtio_9p_write(int fid, const void* buffer, uint32_t count, uint64_t offset) {
    if (!g_9p_state.mounted) return -1;
    
    SERIAL_LOG("[9P] Writing FID ");
    serial_debug_hex(fid);
    SERIAL_LOG(" count=");
    serial_debug_hex(count);
    SERIAL_LOG("\n");
    
    uint8_t* req = g_9p_state.msg_buffer;
    uint8_t* resp = g_9p_state.msg_buffer + 4096;
    
    // Limit write size
    if (count > g_9p_state.msize - 32) {
        count = g_9p_state.msize - 32;
    }
    
    uint8_t* p = req;
    p += 4;
    *p++ = P9_TWRITE;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = fid;
    p += 4;
    *(uint64_t*)p = offset;
    p += 8;
    *(uint32_t*)p = count;
    p += 4;
    memcpy(p, buffer, count);
    p += count;
    
    uint32_t req_size = p - req;
    *(uint32_t*)req = req_size;
    
    uint32_t resp_size = 4096;
    if (!virtio_9p_rpc(req, req_size, resp, &resp_size)) {
        SERIAL_LOG("[9P] ERROR: TWRITE failed\n");
        return -1;
    }
    
    if (resp[4] != P9_RWRITE) {
        SERIAL_LOG("[9P] ERROR: Expected RWRITE\n");
        return -1;
    }
    
    uint32_t written = *(uint32_t*)(resp + 7);
    
    SERIAL_LOG("[9P] Wrote ");
    serial_debug_hex(written);
    SERIAL_LOG(" bytes\n");
    
    return written;
}

// Close file
void virtio_9p_close(int fid) {
    if (!g_9p_state.mounted) return;
    
    SERIAL_LOG("[9P] Closing FID ");
    serial_debug_hex(fid);
    SERIAL_LOG("\n");
    
    uint8_t* req = g_9p_state.msg_buffer;
    uint8_t* resp = g_9p_state.msg_buffer + 4096;
    
    uint8_t* p = req;
    p += 4;
    *p++ = P9_TCLUNK;
    *(uint16_t*)p = g_9p_state.next_tag++;
    p += 2;
    *(uint32_t*)p = fid;
    p += 4;
    
    uint32_t req_size = p - req;
    *(uint32_t*)req = req_size;
    
    uint32_t resp_size = 4096;
    virtio_9p_rpc(req, req_size, resp, &resp_size);
    
    SERIAL_LOG("[9P] File closed\n");
}

bool virtio_9p_readdir(const char* path, void (*callback)(const char* name, bool is_dir)) {
    if (!g_9p_state.mounted) return false;
    
    SERIAL_LOG("[9P] Reading directory: ");
    SERIAL_LOG(path);
    SERIAL_LOG("\n");
    
    // TODO: Implement directory listing
    // 1. TWALK to directory
    // 2. TOPEN in read mode
    // 3. Multiple TREAD calls to get directory entries
    // 4. Parse stat structures and call callback
    
    return true;
}

bool virtio_9p_is_mounted(void) {
    return g_9p_state.mounted;
}

const char* virtio_9p_get_mount_point(void) {
    return g_9p_state.mounted ? g_9p_state.mount_point : NULL;
}
