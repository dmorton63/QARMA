// Minimal PCI access helpers
#pragma once

// Use kernel-defined integer types (freestanding environment)
#include "kernel_types.h"

// PCI config ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// Build a config address
static inline uint32_t pci_config_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
}

static inline uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_config_addr(bus, slot, func, offset);
    __asm__ volatile ("outl %0, %1" : : "a"(addr), "Nd"(PCI_CONFIG_ADDRESS));
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(PCI_CONFIG_DATA)); 
    return val;
}

// Helper to read 16-bit word
static inline uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t d = pci_read_config_dword(bus, slot, func, offset & 0xFC);
    return (uint16_t)((d >> ((offset & 2) * 8)) & 0xFFFF);
}


void pci_init(void);
void pci_scan_and_print(void);