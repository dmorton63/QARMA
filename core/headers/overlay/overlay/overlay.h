#pragma once
#include "../stdtools.h"

typedef struct {
    void* base;
    size_t size;
    int flags;
    const char* name;
} OverlayRegion;

void overlay_init(void);
void* overlay_alloc(const char* name, size_t size, int flags);
void  overlay_free(void* addr);
bool  overlay_protect(void* addr, int flags);
bool  overlay_is_valid(void* addr);
void  overlay_debug_dump(void);