#ifndef NETLOG_H
#define NETLOG_H

#include "kernel_types.h" // for size_t, uint32_t
#include "stdtools.h"
#include "config.h"

// Initialize network logging (lazy create). Returns true if ready.
bool netlog_init(void);
// Append a string (no formatting) to network log file.
void netlog_write(const char* msg);
// Append hex value convenience.
void netlog_write_hex(const char* prefix, uint32_t val);
// Flush or rotate (future; currently noop)
void netlog_flush(void);

// Diagnostic: print backend path and offset.
void netlog_status(void);
// Force migration from ramdisk to host if mounted.
void netlog_force_upgrade(void);

#endif // NETLOG_H
