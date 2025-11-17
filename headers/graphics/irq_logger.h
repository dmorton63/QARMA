#ifndef IRQ_LOGGER_H
#define IRQ_LOGGER_H

#include "core/stdtools.h"
#include "core/boot_log.h"

#define IRQLOG_SLOTS 128
#define IRQLOG_LINE_LEN BOOT_LOG_LINE_LENGTH

void irq_log_init(void);
// Safe to call from IRQ context
int irq_log_enqueue(const char *s);
int irq_log_enqueue_hex(const char *prefix, uint32_t val);
// Non-IRQ: flush queued lines into the message box (call from non-IRQ)
void irq_log_flush_to_message_box(void);
// Flush queued IRQ log lines directly to the serial port. Safe to call
// from IRQ context for short debug prints.
void irq_log_flush_to_serial(void);

#endif // IRQ_LOGGER_H
