/**
 * QARMA - VirtIO 9P File System Driver
 * 
 * Provides access to host filesystem via QEMU's virtio-9p (VirtFS)
 * This allows mounting host directories into the guest OS
 */

#pragma once

#include "core/stdtools.h"

// VirtIO PCI Configuration
#define VIRTIO_VENDOR_ID        0x1AF4
#define VIRTIO_9P_DEVICE_ID     0x1009  // VirtIO 9P device

// VirtIO device status bits
#define VIRTIO_STATUS_ACK           1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

// 9P2000.L protocol commands
#define P9_TVERSION     100
#define P9_RVERSION     101
#define P9_TATTACH      104
#define P9_RATTACH      105
#define P9_TWALK        110
#define P9_RWALK        111
#define P9_TOPEN        112
#define P9_ROPEN        113
#define P9_TREAD        116
#define P9_RREAD        117
#define P9_TWRITE       118
#define P9_RWRITE       119
#define P9_TCLUNK       120
#define P9_RCLUNK       121
#define P9_TSTAT        124
#define P9_RSTAT        125

// 9P open modes
#define P9_OREAD        0x00
#define P9_OWRITE       0x01
#define P9_ORDWR        0x02
#define P9_OTRUNC       0x10

// 9P file identifiers (FIDs)
#define P9_NOFID        (~0U)
#define P9_ROOT_FID     1
#define P9_MAXWELEM     16

// Initialize VirtIO 9P driver
bool virtio_9p_init(void);

// Mount operations
bool virtio_9p_mount(const char* mount_tag, const char* mount_point);

// File operations
int virtio_9p_open(const char* path, int mode);
int virtio_9p_read(int fid, void* buffer, uint32_t count, uint64_t offset);
int virtio_9p_write(int fid, const void* buffer, uint32_t count, uint64_t offset);
void virtio_9p_close(int fid);

// Directory operations
bool virtio_9p_readdir(const char* path, void (*callback)(const char* name, bool is_dir));

// Utility
bool virtio_9p_is_mounted(void);
const char* virtio_9p_get_mount_point(void);
