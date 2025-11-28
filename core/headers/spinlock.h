#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "../core/stdtools.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline void spin_lock(spinlock_t *lock) {
    // Acquire with pause to reduce contention
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static inline bool spin_try_lock(spinlock_t *lock) {
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

static inline bool spin_is_locked(spinlock_t *lock) {
    // Relaxed read is fine for a hint
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0;
}

/* Optional: IRQ-safe variants for x86 */
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory");
}

static inline uint32_t spin_lock_irqsave(spinlock_t *lock) {
    uint32_t flags = irq_save();
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, uint32_t flags) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
    irq_restore(flags);
}

#endif /* SPINLOCK_H */