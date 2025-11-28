/**
 * QARMA - Hardware I/O Port Access Functions
 * 
 * Common inline functions for x86 port I/O operations
 * Used across the kernel for hardware communication
 */

#ifndef IO_H
#define IO_H

#include "stdtools.h"

/**
 * Read a byte from an I/O port
 * @param port The port number to read from
 * @return The byte value read from the port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %w1, %b0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Write a byte to an I/O port
 * @param port The port number to write to
 * @param value The byte value to write
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

/**
 * Read a word (16-bit) from an I/O port
 * @param port The port number to read from
 * @return The word value read from the port
 */
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %w1, %w0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Write a word (16-bit) to an I/O port
 * @param port The port number to write to
 * @param value The word value to write
 */
static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %w0, %w1" : : "a"(value), "Nd"(port));
}

/**
 * Read a double word (32-bit) from an I/O port
 * @param port The port number to read from
 * @return The double word value read from the port
 */
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    __asm__ volatile ("inl %w1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * Write a double word (32-bit) to an I/O port
 * @param port The port number to write to
 * @param value The double word value to write
 */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %w1" : : "a"(value), "Nd"(port));
}

/**
 * Insert a small delay for I/O operations
 * Some hardware requires a brief pause between operations
 */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif // IO_H