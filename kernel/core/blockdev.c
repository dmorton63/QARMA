#include "blockdev.h"
#include "core/string.h"

static blockdev_t* blockdev_head = 0;

void blockdev_register(blockdev_t* dev) {
    if (!dev) return;
    dev->next = blockdev_head;
    blockdev_head = dev;
}

blockdev_t* blockdev_find(const char* name) {
    blockdev_t* d = blockdev_head;
    while (d) {
        if (strcmp(d->name, name) == 0) return d;
        d = d->next;
    }
    return 0;
}

blockdev_t* blockdev_list(void) {
    return blockdev_head;
}
