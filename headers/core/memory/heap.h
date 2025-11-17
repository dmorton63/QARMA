#pragma once

#include "../stdtools.h"


void heap_init(void);
void* heap_alloc(size_t size);
void* heap_alloc_aligned(size_t size, size_t alignment);
void  heap_free(void* ptr);