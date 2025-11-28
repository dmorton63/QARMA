#pragma once
#include "core/blockdev.h"
#include "vfs.h"

// Simple filesystem driver (for RAM disk demonstration)
void simplefs_init(void);

// File access functions
vfs_node_t* simplefs_find_file(const char* filename);
int simplefs_read_file(const char* filename, void* buffer, size_t size, size_t offset);