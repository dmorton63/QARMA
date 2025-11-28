#pragma once
#include "kernel_types.h"
#include "stdtools.h"

void pmm_init(void);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t addr);
void pmm_mark_region_free(uint64_t start_addr, uint64_t length);
void pmm_mark_region_used(uint64_t start_addr, uint64_t length);
void pmm_print_stats(void);