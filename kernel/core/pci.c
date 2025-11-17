#include "pci.h"
#include "graphics/graphics.h"
#include "keyboard/command.h"
#include "drivers/usb/uhci.h"


// Scan PCI bus 0..255, slot 0..31, func 0..7 and print vendor/device IDs
void pci_scan_and_print(void) {
    SERIAL_LOG("PCI: Starting PCI bus scan\n");
    gfx_print("Scanning PCI bus...\n");
    gfx_print("Starting PCI bus scan...\n");

    for (uint8_t bus = 0; bus < 2; ++bus) { // Scan first 2 buses
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint16_t vendor = pci_read_config_word(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) continue; // no device
                uint16_t device = pci_read_config_word(bus, slot, func, 0x02);
                
                // Debug: Print all devices found
                SERIAL_LOG("PCI Device found: ");
                SERIAL_LOG_HEX("Bus ", bus);
                SERIAL_LOG_HEX(" Slot ", slot);
                SERIAL_LOG_HEX(" Func ", func);
                SERIAL_LOG_HEX(" Vendor ", vendor);
                SERIAL_LOG_HEX(" Device ", device);
                
                uint8_t class_code = pci_read_config_word(bus, slot, func, 0x0A) >> 8;
                uint8_t subclass   = pci_read_config_word(bus, slot, func, 0x0A) & 0xFF;
                uint8_t prog_if    = pci_read_config_word(bus, slot, func, 0x08) >> 8; // Prog IF is at 0x09, read from 0x08 and shift
                
                SERIAL_LOG_HEX(" Class ", class_code);
                SERIAL_LOG_HEX(" Sub ", subclass);  
                SERIAL_LOG_HEX(" Prog ", prog_if);
                SERIAL_LOG("\n");
                
                if (class_code == 0x0C && subclass == 0x03) {
                    uint32_t bar4 = pci_read_config_dword(bus, slot, func, 0x20);
                    uint16_t io_base = bar4 & 0xFFF0;
                    
                    if (prog_if == 0x00 || prog_if == 0x01) {
                        // Some QEMU PIIX implementations use prog_if=0x01 for UHCI-compatible interface
                        SERIAL_LOG("UHCI controller detected\n");
                        SERIAL_LOG_HEX("BUS: ",bus);
                        SERIAL_LOG_HEX(" SLOT: ",slot);
                        SERIAL_LOG_HEX(" FUNC: ",func);
                        SERIAL_LOG_HEX(" IO_BASE: ",io_base);
                        SERIAL_LOG_HEX(" PROG_IF: ",prog_if);
                        SERIAL_LOG("\n");

                        // Initialize the UHCI controller
                        SERIAL_LOG("PCI: Calling uhci_init_controller\n");
                        int result = uhci_init_controller(bus, slot, func, io_base);
                        if (result == 0) {
                            SERIAL_LOG("PCI: UHCI controller initialized successfully\n");
                        } else {
                            SERIAL_LOG("PCI: UHCI controller initialization failed\n");
                            SERIAL_LOG_HEX("PCI: Error code: ", result);
                            SERIAL_LOG("\n");
                        }
                    } else if (prog_if == 0x10) {
                        SERIAL_LOG("OHCI controller detected\n");
                        SERIAL_LOG_HEX("BUS: ",bus);
                        SERIAL_LOG_HEX(" SLOT: ",slot);
                        SERIAL_LOG_HEX(" FUNC: ",func);
                        SERIAL_LOG_HEX(" IO_BASE: ",io_base);
                        SERIAL_LOG("\n");
                        // OHCI not implemented yet
                    } else if (prog_if == 0x20) {
                        SERIAL_LOG("EHCI controller detected\n");
                        SERIAL_LOG_HEX("BUS: ",bus);
                        SERIAL_LOG_HEX(" SLOT: ",slot);
                        SERIAL_LOG_HEX(" FUNC: ",func);
                        SERIAL_LOG_HEX(" IO_BASE: ",io_base);
                        SERIAL_LOG("\n");
                        // EHCI not implemented yet
                    } else {
                        SERIAL_LOG("Unknown USB controller type\n");
                        SERIAL_LOG_HEX("Prog IF: ",prog_if);
                        SERIAL_LOG("\n");
                    }
                }
                // for(int i=0; buf[i] != '\0'; i++)
                //     gfx_putchar(buf[i]);
                // gfx_print("Found device at bus ");
                // gfx_print_decimal(bus);
                // gfx_print(" slot ");
                // gfx_print_decimal(slot);
                // gfx_print(" func ");
                // gfx_print_decimal(func);
                // gfx_print(" vendor=0x");
                // gfx_print_hex(vendor);
                // gfx_print(" device=0x");
                // gfx_print_hex(device);

                // gfx_print("PCI ");
                // gfx_print_decimal(bus);
                // gfx_print(":");
                // gfx_print_decimal(slot);
                // gfx_print(":");
                // gfx_print_decimal(func);
                // gfx_print(" vendor=0x");
                // gfx_print_hex(vendor);
                // gfx_print(" device=0x");
                // gfx_print_hex(device);
                // gfx_print("\n");
            }
        }
    }
}

void pci_init(void) {
    SERIAL_LOG("PCI: Starting PCI initialization\n");
    gfx_print("Initializing PCI subsystem...\n");

    // Optional: parse ACPI/MCFG for PCIe
    // Optional: register PCI access helpers
    // Optional: initialize device list

    pci_scan_and_print(); // or pci_scan_devices()
    SERIAL_LOG("PCI: PCI initialization complete\n");
}