/**
 * QARMA - Memory Management Header
 * 
 * Unified declarations for memory subsystems: PMM, VMM, Heap, and utilities.
 */

#ifndef MEMORY_H
#define MEMORY_H

#include "kernel_types.h"

//
// ─── Memory Constants ───────────────────────────────────────────────────────────
//

#define PAGE_SIZE 0x1000  // 4KB pages
#define BLOCK_SIZE sizeof(Block)
#define ALIGNED(x) __attribute__((aligned(x)))

//
// ─── Heap Block Structure (Legacy malloc/free) ──────────────────────────────────
//

typedef struct Block {
    size_t size;
    struct Block* next;
    int free;
} Block;

//
//
// ─── Legacy malloc/free (optional) ──────────────────────────────────────────────
//
void memory_init(void);
void* malloc(size_t size);
void* sbrk(int increment);
void  free(void* ptr);

//
// ─── Memory Utilities ───────────────────────────────────────────────────────────
//

void kernel_delay(uint32_t count);

//
// ─── String Functions (implemented in string.c) ─────────────────────────────────
//

void* memset(void* ptr, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);

#endif // MEMORY_H