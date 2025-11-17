/**
 * @file e1000.c
 * @brief Intel E1000 Gigabit Ethernet driver implementation
 */

#include "e1000.h"
#include "core/pci.h"
#include "core/io.h"
#include "core/memory/heap.h"
#include "core/string.h"

// Define offsetof if not available
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif

static e1000_device_t* e1000_dev = NULL;

// Define memory allocation wrappers
#define kmalloc(size) heap_alloc(size)
#define pci_config_read_word pci_read_config_word
#define pci_config_read_dword pci_read_config_dword

// PCI write function
static inline void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | 
                      ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    uint32_t old = inl(0xCFC);
    uint32_t mask = 0xFFFF << ((offset & 2) * 8);
    uint32_t new_val = (old & ~mask) | ((uint32_t)value << ((offset & 2) * 8));
    outl(0xCFC, new_val);
}

uint32_t e1000_read_reg(e1000_device_t* dev, uint16_t reg) {
    return *((volatile uint32_t*)(dev->mem_base + reg));
}

void e1000_write_reg(e1000_device_t* dev, uint16_t reg, uint32_t value) {
    *((volatile uint32_t*)(dev->mem_base + reg)) = value;
}

static uint16_t e1000_read_eeprom(e1000_device_t* dev, uint8_t addr) {
    uint32_t tmp = 0;
    
    e1000_write_reg(dev, E1000_REG_EEPROM, 1 | ((uint32_t)(addr) << 8));
    
    while (!((tmp = e1000_read_reg(dev, E1000_REG_EEPROM)) & (1 << 4)));
    
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static void e1000_read_mac_address(e1000_device_t* dev) {
    if (dev->has_eeprom) {
        uint16_t mac_part;
        
        mac_part = e1000_read_eeprom(dev, 0);
        dev->net_dev.mac_address.addr[0] = mac_part & 0xFF;
        dev->net_dev.mac_address.addr[1] = mac_part >> 8;
        
        mac_part = e1000_read_eeprom(dev, 1);
        dev->net_dev.mac_address.addr[2] = mac_part & 0xFF;
        dev->net_dev.mac_address.addr[3] = mac_part >> 8;
        
        mac_part = e1000_read_eeprom(dev, 2);
        dev->net_dev.mac_address.addr[4] = mac_part & 0xFF;
        dev->net_dev.mac_address.addr[5] = mac_part >> 8;
    } else {
        // Try reading from hardware registers
        uint32_t mac_low = e1000_read_reg(dev, 0x5400);
        uint32_t mac_high = e1000_read_reg(dev, 0x5404);
        
        dev->net_dev.mac_address.addr[0] = mac_low & 0xFF;
        dev->net_dev.mac_address.addr[1] = (mac_low >> 8) & 0xFF;
        dev->net_dev.mac_address.addr[2] = (mac_low >> 16) & 0xFF;
        dev->net_dev.mac_address.addr[3] = (mac_low >> 24) & 0xFF;
        dev->net_dev.mac_address.addr[4] = mac_high & 0xFF;
        dev->net_dev.mac_address.addr[5] = (mac_high >> 8) & 0xFF;
    }
}

static void e1000_init_rx(e1000_device_t* dev) {
    extern void gfx_print(const char*);
    
    // Allocate receive descriptors (aligned to 16 bytes)
    dev->rx_descs = (e1000_rx_desc_t*)kmalloc(sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);
    
    // Allocate receive buffers
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        dev->rx_buffers[i] = (uint8_t*)kmalloc(8192); // 8KB buffer
        dev->rx_descs[i].addr = (uint64_t)(uint32_t)dev->rx_buffers[i];
        dev->rx_descs[i].status = 0;
        dev->rx_descs[i].errors = 0;
        dev->rx_descs[i].length = 0;
        dev->rx_descs[i].checksum = 0;
        dev->rx_descs[i].special = 0;
    }
    
    // Setup the receive descriptor ring BEFORE enabling
    e1000_write_reg(dev, E1000_REG_RXDESCLO, (uint32_t)dev->rx_descs);
    e1000_write_reg(dev, E1000_REG_RXDESCHI, 0);
    e1000_write_reg(dev, E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write_reg(dev, E1000_REG_RXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, 0);
    
    dev->rx_current = 0;
    
    // Set RX delay timer to 0 for immediate writeback
    e1000_write_reg(dev, E1000_REG_RDTR, 0);
    
    // Configure receive control with 2KB buffers (standard for E1000)
    uint32_t rctl = E1000_RCTL_EN |          // Enable receiver
                    E1000_RCTL_SBP |         // Store bad packets
                    E1000_RCTL_UPE |         // Unicast promiscuous
                    E1000_RCTL_MPE |         // Multicast promiscuous
                    E1000_RCTL_LBM_NONE |    // No loopback
                    E1000_RCTL_BAM |         // Broadcast accept
                    E1000_RCTL_BSIZE_2048 |  // 2KB buffer size
                    E1000_RCTL_SECRC;        // Strip CRC
    
    e1000_write_reg(dev, E1000_REG_RCTRL, rctl);
    
    // Small delay for hardware to initialize
    for (volatile int i = 0; i < 100000; i++);
    
    // NOW make all descriptors available by setting tail
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    
    // Verify RX is enabled
    uint32_t rctl_check = e1000_read_reg(dev, E1000_REG_RCTRL);
    if (rctl_check & E1000_RCTL_EN) {
        gfx_print("E1000: RX enabled with promiscuous mode\n");
    } else {
        gfx_print("E1000: WARNING - RX not enabled after write!\n");
    }
}

static void e1000_init_tx(e1000_device_t* dev) {
    // Allocate transmit descriptors (aligned to 16 bytes)
    dev->tx_descs = (e1000_tx_desc_t*)kmalloc(sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);
    
    // Allocate transmit buffers
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        dev->tx_buffers[i] = (uint8_t*)kmalloc(8192); // 8KB buffer
        dev->tx_descs[i].addr = (uint64_t)(uint32_t)dev->tx_buffers[i];
        dev->tx_descs[i].status = E1000_TXD_STAT_DD;
        dev->tx_descs[i].cmd = 0;
    }
    
    // Setup the transmit descriptor ring
    e1000_write_reg(dev, E1000_REG_TXDESCLO, (uint32_t)dev->tx_descs);
    e1000_write_reg(dev, E1000_REG_TXDESCHI, 0);
    e1000_write_reg(dev, E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write_reg(dev, E1000_REG_TXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_TXDESCTAIL, 0);
    
    dev->tx_current = 0;
    
    // Enable transmitting
    e1000_write_reg(dev, E1000_REG_TCTRL, 
        E1000_TCTL_EN | 
        E1000_TCTL_PSP | 
        (15 << E1000_TCTL_CT_SHIFT) | 
        (64 << E1000_TCTL_COLD_SHIFT));
}

int e1000_send_packet(net_device_t* netdev, net_packet_t* packet) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    
    serial_debug("[TX: start]\n");
    gfx_print("[TX: start]");
    
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
    if (!dev || !dev->mem_base || !dev->tx_descs || !packet) {
        serial_debug("[TX: null]\n");
        gfx_print("[TX: null]");
        return -1;
    }
    
    if (!dev->tx_buffers) {
        serial_debug("[TX: no bufs]\n");
        gfx_print("[TX: no bufs]");
        return -1;
    }
    
    serial_debug("[TX: wait]\n");
    gfx_print("[TX: wait]");
    
    // Get current TX descriptor
    e1000_tx_desc_t* desc = &dev->tx_descs[dev->tx_current];
    
    // Wait for descriptor to be available (with timeout)
    int timeout = 100000;
    while (!(desc->status & E1000_TXD_STAT_DD) && timeout-- > 0);
    
    if (timeout <= 0) {
        serial_debug("[TX: timeout]\n");
        gfx_print("[TX: timeout]");
        return -1;
    }
    
    serial_debug("[TX: copy]\n");
    gfx_print("[TX: copy]");
    
    // Copy packet data to TX buffer
    uint8_t* tx_buf = dev->tx_buffers[dev->tx_current];
    if (!tx_buf) {
        serial_debug("[TX: no buf]\n");
        gfx_print("[TX: no buf]");
        return -1;
    }
    
    for (uint32_t i = 0; i < packet->length && i < 8192; i++) {
        tx_buf[i] = packet->data[i];
    }
    
    serial_debug("[TX: setup]\n");
    gfx_print("[TX: setup]");
    
    // Setup descriptor
    desc->length = packet->length;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;
    
    // Move to next descriptor
    dev->tx_current = (dev->tx_current + 1) % E1000_NUM_TX_DESC;
    
    serial_debug("[TX: notify]\n");
    gfx_print("[TX: notify]");
    
    // Notify hardware
    e1000_write_reg(dev, E1000_REG_TXDESCTAIL, dev->tx_current);
    
    serial_debug("[TX: done]\n");
    gfx_print("[TX: done]");
    
    return 0;
}

int e1000_init_device(net_device_t* netdev) {
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
    extern void gfx_print(const char*);
    gfx_print("E1000: Bringing device up...\n");
    
    // Link up
    uint32_t ctrl = e1000_read_reg(dev, E1000_REG_CTRL);
    ctrl |= E1000_CTRL_SLU;
    e1000_write_reg(dev, E1000_REG_CTRL, ctrl);
    
    // Check link status
    uint32_t status = e1000_read_reg(dev, E1000_REG_STATUS);
    if (status & 0x02) {
        gfx_print("E1000: Link is UP\n");
    } else {
        gfx_print("E1000: Link is DOWN\n");
    }
    
    gfx_print("E1000: Device is now UP\n");
    
    return 0;
}

int e1000_shutdown_device(net_device_t* netdev) {
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
    extern void gfx_print(const char*);
    gfx_print("E1000: Shutting device down...\n");
    
    // Disable RX and TX
    e1000_write_reg(dev, E1000_REG_RCTRL, 0);
    e1000_write_reg(dev, E1000_REG_TCTRL, 0);
    
    gfx_print("E1000: Device is now DOWN\n");
    
    return 0;
}

bool e1000_detect_pci(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_config_read_word(bus, slot, func, 0);
    
    // Check for invalid/non-existent device
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
        return false;
    }
    
    if (vendor_id != E1000_VENDOR_ID) {
        return false;
    }
    
    uint16_t device_id = pci_config_read_word(bus, slot, func, 2);
    
    // Check if it's a supported E1000 device
    if (device_id != E1000_DEV_ID_82540EM && 
        device_id != E1000_DEV_ID_82545EM &&
        device_id != E1000_DEV_ID_82574L) {
        return false;
    }
    
    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    
    gfx_print("E1000: Found Intel NIC (Device ID: ");
    gfx_print_hex(device_id);
    gfx_print(")\n");
    
    // Allocate device structure
    e1000_dev = (e1000_device_t*)kmalloc(sizeof(e1000_device_t));
    if (!e1000_dev) {
        gfx_print("E1000: Failed to allocate device structure\n");
        return false;
    }
    
    // Get BAR0 (memory mapped registers)
    uint32_t bar0 = pci_config_read_dword(bus, slot, func, 0x10);
    uint32_t bar0_phys = bar0 & 0xFFFFFFF0;
    
    // Verify we got a valid memory address
    if (bar0_phys == 0 || bar0_phys == 0xFFFFFFF0) {
        gfx_print("E1000: Invalid BAR0 address\n");
        return false;
    }
    
    gfx_print("E1000: BAR0 physical address: ");
    gfx_print_hex(bar0_phys);
    gfx_print("\n");
    
    // Map BAR0 into virtual memory (E1000 registers are ~128KB)
    // We'll map 128KB (32 pages) to be safe
    extern void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
    #define PAGE_PRESENT  0x001
    #define PAGE_WRITE    0x002
    #define PAGE_NO_CACHE 0x040
    
    uint32_t bar0_size = 128 * 1024; // 128KB
    uint32_t num_pages = (bar0_size + 4095) / 4096; // Round up to pages
    
    // Use the physical address as virtual address (identity mapping for simplicity)
    e1000_dev->mem_base = bar0_phys;
    
    gfx_print("E1000: Mapping BAR0 (");
    gfx_print_hex(num_pages);
    gfx_print(" pages)...\n");
    
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t virt = bar0_phys + (i * 4096);
        uint32_t phys = bar0_phys + (i * 4096);
        vmm_map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);
    }
    
    gfx_print("E1000: BAR0 mapped successfully\n");
    
    // Enable bus mastering
    uint16_t command = pci_config_read_word(bus, slot, func, 0x04);
    command |= 0x04; // Bus Master Enable
    pci_config_write_word(bus, slot, func, 0x04, command);
    
    gfx_print("E1000: Bus mastering enabled\n");
    
    // Check for EEPROM
    e1000_write_reg(e1000_dev, E1000_REG_EEPROM, 0x01);
    for (int i = 0; i < 1000 && !(e1000_read_reg(e1000_dev, E1000_REG_EEPROM) & 0x10); i++);
    if (e1000_read_reg(e1000_dev, E1000_REG_EEPROM) & 0x10) {
        e1000_dev->has_eeprom = true;
        gfx_print("E1000: EEPROM detected\n");
    } else {
        e1000_dev->has_eeprom = false;
        gfx_print("E1000: No EEPROM, using registers\n");
    }
    
    // Read MAC address
    e1000_read_mac_address(e1000_dev);
    
    gfx_print("E1000: MAC address: ");
    char mac_str[18];
    extern void mac_addr_to_string(mac_addr_t* mac, char* buffer);
    mac_addr_to_string(&e1000_dev->net_dev.mac_address, mac_str);
    gfx_print(mac_str);
    gfx_print("\n");
    
    // Initialize RX and TX rings
    gfx_print("E1000: Initializing RX/TX rings...\n");
    e1000_init_rx(e1000_dev);
    e1000_init_tx(e1000_dev);
    gfx_print("E1000: RX/TX rings initialized\n");
    
    // Setup network device structure
    strcpy(e1000_dev->net_dev.name, "eth0");
    e1000_dev->net_dev.state = NET_DEV_DOWN;
    e1000_dev->net_dev.mtu = 1500;
    e1000_dev->net_dev.rx_packets = 0;
    e1000_dev->net_dev.tx_packets = 0;
    e1000_dev->net_dev.rx_bytes = 0;
    e1000_dev->net_dev.tx_bytes = 0;
    e1000_dev->net_dev.rx_errors = 0;
    e1000_dev->net_dev.tx_errors = 0;
    e1000_dev->net_dev.send_packet = e1000_send_packet;
    e1000_dev->net_dev.receive_packet = NULL;
    e1000_dev->net_dev.init = e1000_init_device;
    e1000_dev->net_dev.shutdown = e1000_shutdown_device;
    
    // Set IP address for QEMU user-mode networking (10.0.2.15)
    e1000_dev->net_dev.ip_address.addr[0] = 10;
    e1000_dev->net_dev.ip_address.addr[1] = 0;
    e1000_dev->net_dev.ip_address.addr[2] = 2;
    e1000_dev->net_dev.ip_address.addr[3] = 15;
    
    // Register with network subsystem
    extern int network_register_device(net_device_t* device);
    if (network_register_device(&e1000_dev->net_dev) != 0) {
        gfx_print("E1000: Failed to register network device\n");
        return false;
    }
    
    // Bring device UP
    e1000_dev->net_dev.state = NET_DEV_RUNNING;
    e1000_init_device(&e1000_dev->net_dev);
    
    gfx_print("E1000: Device initialized successfully\n");
    
    return true;
}

// Poll for received packets
void e1000_poll_receive(e1000_device_t* dev) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    
    if (!dev) {
        gfx_print("[RX: dev null]");
        return;
    }
    if (!dev->mem_base) {
        gfx_print("[RX: no mem_base]");
        return;
    }
    if (!dev->rx_descs) {
        gfx_print("[RX: descs null]");
        return;
    }
    
    // Check link status
    uint32_t status = e1000_read_reg(dev, E1000_REG_STATUS);
    if (!(status & 0x02)) {
        gfx_print("[RX: link down]");
        return;
    }
    
    // Check receive descriptors for new packets
    int checked = 0;
    while (checked < E1000_NUM_RX_DESC) {
        uint32_t current = dev->rx_current;
        if (current >= E1000_NUM_RX_DESC) {
            dev->rx_current = 0;
            break;
        }
        
        e1000_rx_desc_t* desc = &dev->rx_descs[current];
        
        // Debug on first check
        if (checked == 0) {
            uint32_t rdh = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
            uint32_t rdt = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
            serial_debug("[RX: cur=");
            char buf[32];
            buf[0] = (current / 10) + '0';
            buf[1] = (current % 10) + '0';
            buf[2] = ' '; buf[3] = 'H'; buf[4] = '=';
            buf[5] = (rdh / 10) + '0';
            buf[6] = (rdh % 10) + '0';
            buf[7] = ' '; buf[8] = 'T'; buf[9] = '=';
            buf[10] = (rdt / 10) + '0';
            buf[11] = (rdt % 10) + '0';
            buf[12] = ' '; buf[13] = 's'; buf[14] = '=';
            uint8_t s = desc->status;
            buf[15] = (s >> 4) < 10 ? '0' + (s >> 4) : 'A' + (s >> 4) - 10;
            buf[16] = (s & 0xF) < 10 ? '0' + (s & 0xF) : 'A' + (s & 0xF) - 10;
            buf[17] = ']'; buf[18] = '\\'; buf[19] = 'n'; buf[20] = '\0';
            serial_debug(buf);
        }
        
        // Check if descriptor has been used by hardware (DD bit set)
        if (!(desc->status & E1000_RXD_STAT_DD)) {
            break; // No more packets
        }
        
        gfx_print("[RX: pkt!]");
        serial_debug("[RX: pkt!]\\n");
        checked++;
        
        // We have a packet!
        uint16_t length = desc->length;
        if (length > 8192 || length == 0) {
            // Invalid length, skip this packet
            desc->status = 0;
            dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
            continue;
        }
        
        // Use the buffer pointer from our array instead of casting from descriptor
        uint8_t* packet_data = dev->rx_buffers[current];
        if (!packet_data) {
            desc->status = 0;
            dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
            continue;
        }
        
        // Process the packet through the network stack
        extern void ethernet_receive_frame(net_device_t* dev, const uint8_t* data, uint32_t len);
        extern void serial_debug(const char*);
        
        serial_debug("[RX: process]\\n");
        ethernet_receive_frame(&dev->net_dev, packet_data, length);
        serial_debug("[RX: processed]\\n");
        
        // Update statistics
        dev->net_dev.rx_packets++;
        dev->net_dev.rx_bytes += length;
        
        // Reset descriptor for reuse  
        desc->status = 0;
        desc->errors = 0;
        desc->length = 0;
        
        // Move to next descriptor
        dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
        
        // Don't update TAIL - keep it at initial value (31)
        // This keeps descriptors 0-30 perpetually available
        // The hardware will cycle through them automatically
        serial_debug("[RX: freed]\\n");
    }
}

// Helper function to poll for packets (can be called from commands)
void e1000_check_packets(void) {
    extern void gfx_print(const char*);
    
    if (!e1000_dev) {
        gfx_print("[RX: no dev]");
        return;
    }
    
    if (e1000_dev->net_dev.state != NET_DEV_RUNNING) {
        gfx_print("[RX: not running]");
        return;
    }
    
    if (!e1000_dev->rx_descs) {
        gfx_print("[RX: no descs]");
        return;
    }
    
    e1000_poll_receive(e1000_dev);
}

void e1000_init(void) {
    extern void gfx_print(const char*);
    
    // Serial debug output
    extern void serial_debug(const char*);
    serial_debug("[E1000] Init starting...\n");
    
    gfx_print("E1000: Scanning PCI bus for Intel NICs...\n");
    
    // Scan only bus 0 (most common) to avoid crashes
    // Full bus scan can cause issues with invalid memory access
    serial_debug("[E1000] Starting PCI scan of bus 0\n");
    for (uint8_t slot = 0; slot < 32; slot++) {
        // Check if device exists first
        uint16_t vendor = pci_config_read_word(0, slot, 0, 0x00);
        if (vendor == 0xFFFF || vendor == 0x0000) {
            continue; // No device in this slot
        }
        
        for (uint8_t func = 0; func < 8; func++) {
            if (e1000_detect_pci(0, slot, func)) {
                serial_debug("[E1000] Device found and initialized\n");
                gfx_print("E1000: Found and initialized Intel NIC\n");
                return; // Found one, that's enough for now
            }
        }
    }
    
    serial_debug("[E1000] No device found, init complete\n");
    gfx_print("E1000: No Intel NIC found on bus 0\n");
}

void e1000_print_info(void) {
    extern void gfx_print(const char*);
    extern void gfx_printf(const char*, ...);
    
    if (!e1000_dev) {
        gfx_print("E1000: Not initialized\n");
        return;
    }
    
    gfx_print("E1000 Network Interface:\n");
    gfx_printf("  Device: eth0 (E1000)\n");
    gfx_printf("  Status: Initialized\n");
    
    uint32_t status = e1000_read_reg(e1000_dev, E1000_REG_STATUS);
    gfx_printf("  Link: %s\n", (status & 0x02) ? "UP" : "DOWN");
    gfx_printf("  Speed: %s\n", (status & 0x40) ? "1000Mbps" : "10/100Mbps");
}

