#ifndef E820_H  
#define E820_H

#include "../../stdtools.h"


#define E820_USABLE   1
#define E820_RESERVED 2
#define E820_ACPI     3
#define E820_NVS      4
#define E820_BAD      5

typedef struct {
    uint64_t addr;  // base address
    uint64_t len;   // length
    uint32_t type;  // E820_* type
} e820_entry_t;

#endif // E820_H
