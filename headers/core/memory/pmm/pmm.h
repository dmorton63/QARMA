#pragma once
#include "kernel_types.h"
#include "../../stdtools.h"

void pmm_init(void);
uint32_t pmm_alloc_page(void);
void pmm_free_page(uint32_t addr);
void pmm_mark_region_free(uint32_t start_addr, uint32_t length);
void pmm_mark_region_used(uint32_t start_addr, uint32_t length);
void pmm_print_stats(void);