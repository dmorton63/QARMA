#pragma once

#include "qarma_win_handle.h"

QARMA_WIN_HANDLE* qarma_win_create(QARMA_WIN_TYPE type, const char* title, uint32_t flags);
QARMA_WIN_HANDLE* qarma_win_create_archetype(uint32_t archetype_id, const char* title, uint32_t flags);
