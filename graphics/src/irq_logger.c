#include "irq_logger.h"
#include "message_box.h"
#include <string.h>

// Simple single-producer (IRQ), single-consumer (non-IRQ) ring buffer
static volatile uint32_t irq_head = 0; // next write index
static volatile uint32_t irq_tail = 0; // next read index
static char irq_slots[IRQLOG_SLOTS][IRQLOG_LINE_LEN];

void irq_log_init(void) {
    irq_head = 0;
    irq_tail = 0;
    // Clear slots
    for (int i = 0; i < IRQLOG_SLOTS; ++i) irq_slots[i][0] = '\0';
}

// Internal helper: copy with truncation, safe for IRQ
static void copy_trunc(char *dst, const char *src) {
    size_t i = 0;
    if (!dst || !src) return;
    while (i + 1 < IRQLOG_LINE_LEN && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int irq_log_enqueue(const char *s) {
    if (!s) return -1;
    uint32_t h = irq_head;
    uint32_t next = (h + 1) % IRQLOG_SLOTS;
    if (next == irq_tail) {
        // Buffer full: drop oldest to make room
        irq_tail = (irq_tail + 1) % IRQLOG_SLOTS;
    }
    copy_trunc(irq_slots[h], s);
    // Make data visible before advancing head
    __asm__ __volatile__("mfence" ::: "memory");
    irq_head = next;
    return 0;
}

// Small hex formatter (no sprintf). Writes prefix then 0xHEX
int irq_log_enqueue_hex(const char *prefix, uint32_t val) {
    char temp[IRQLOG_LINE_LEN];
    size_t pos = 0;
    if (prefix) {
        for (size_t i = 0; prefix[i] && pos + 1 < sizeof(temp); ++i) temp[pos++] = prefix[i];
    }
    if (pos + 3 < sizeof(temp)) {
        temp[pos++] = '0';
        temp[pos++] = 'x';
        // write 8 hex digits
        for (int i = 7; i >= 0 && pos + 1 < sizeof(temp); --i) {
            uint32_t nib = (val >> (i * 4)) & 0xF;
            temp[pos++] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
        }
    }
    temp[pos] = '\0';
    return irq_log_enqueue(temp);
}

void irq_log_flush_to_message_box(void) {
    // Move entries from ring buffer into message_box without triggering
    // a nested render call; the outer caller (message_box_render)
    // will handle the actual rendering.
    while (irq_tail != irq_head) {
        char *s = irq_slots[irq_tail];
        if (s && s[0]) {
            message_box_push_norender(s);
        }
        // Clear slot for cleanliness
        irq_slots[irq_tail][0] = '\0';
        irq_tail = (irq_tail + 1) % IRQLOG_SLOTS;
    }
}

// Flush queued IRQ log lines directly to serial. This is intended for
// short, infrequent debug prints and is safe to call from IRQ context.
void irq_log_flush_to_serial(void) {
    extern void serial_debug(const char* msg);
    while (irq_tail != irq_head) {
        char *s = irq_slots[irq_tail];
        if (s && s[0]) {
            serial_debug(s);
            serial_debug("\n");
        }
        irq_slots[irq_tail][0] = '\0';
        irq_tail = (irq_tail + 1) % IRQLOG_SLOTS;
    }
}
