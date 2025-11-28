/**
 * @file e1000.h
 * @brief Intel E1000 Gigabit Ethernet driver
 * 
 * Supports Intel 82540EM, 82545EM, 82574L and compatible NICs
 * Commonly used in QEMU and VirtualBox
 */

#ifndef E1000_DRIVER_H
#define E1000_DRIVER_H

#include "kernel_types.h"
#include "network/network_subsystem.h"

// E1000 PCI Device IDs
#define E1000_VENDOR_ID     0x8086
#define E1000_DEV_ID_82540EM 0x100E
#define E1000_DEV_ID_82545EM 0x100F
#define E1000_DEV_ID_82574L  0x10D3

// Register offsets
#define E1000_REG_CTRL      0x0000  // Device Control
#define E1000_REG_STATUS    0x0008  // Device Status
#define E1000_REG_EEPROM    0x0014  // EEPROM Read
#define E1000_REG_CTRL_EXT  0x0018  // Extended Device Control
#define E1000_REG_IMASK     0x00D0  // Interrupt Mask
#define E1000_REG_RCTRL     0x0100  // Receive Control
#define E1000_REG_RXDESCLO  0x2800  // RX Descriptor Base Low
#define E1000_REG_RXDESCHI  0x2804  // RX Descriptor Base High
#define E1000_REG_RXDESCLEN 0x2808  // RX Descriptor Length
#define E1000_REG_RXDESCHEAD 0x2810 // RX Descriptor Head
#define E1000_REG_RXDESCTAIL 0x2818 // RX Descriptor Tail
#define E1000_REG_TCTRL     0x0400  // Transmit Control
#define E1000_REG_TXDESCLO  0x3800  // TX Descriptor Base Low
#define E1000_REG_TXDESCHI  0x3804  // TX Descriptor Base High
#define E1000_REG_TXDESCLEN 0x3808  // TX Descriptor Length
#define E1000_REG_TXDESCHEAD 0x3810 // TX Descriptor Head
#define E1000_REG_TXDESCTAIL 0x3818 // TX Descriptor Tail
#define E1000_REG_RDTR      0x2820  // RX Delay Timer
#define E1000_REG_RXDCTL    0x3828  // RX Descriptor Control
#define E1000_REG_RADV      0x282C  // RX Int. Absolute Delay Timer
#define E1000_REG_RSRPD     0x2C00  // RX Small Packet Detect Interrupt

// Control Register bits
#define E1000_CTRL_FD       (1 << 0)  // Full Duplex
#define E1000_CTRL_LRST     (1 << 3)  // Link Reset
#define E1000_CTRL_ASDE     (1 << 5)  // Auto Speed Detection Enable
#define E1000_CTRL_SLU      (1 << 6)  // Set Link Up
#define E1000_CTRL_ILOS     (1 << 7)  // Invert Loss of Signal
#define E1000_CTRL_RST      (1 << 26) // Device Reset
#define E1000_CTRL_VME      (1 << 30) // VLAN Mode Enable
#define E1000_CTRL_PHY_RST  (1 << 31) // PHY Reset

// Receive Control Register bits
#define E1000_RCTL_EN       (1 << 1)  // Receive Enable
#define E1000_RCTL_SBP      (1 << 2)  // Store Bad Packets
#define E1000_RCTL_UPE      (1 << 3)  // Unicast Promiscuous Enable
#define E1000_RCTL_MPE      (1 << 4)  // Multicast Promiscuous Enable
#define E1000_RCTL_LPE      (1 << 5)  // Long Packet Enable
#define E1000_RCTL_LBM_NONE (0 << 6)  // No Loopback
#define E1000_RCTL_BAM      (1 << 15) // Broadcast Accept Mode
#define E1000_RCTL_BSIZE_2048 (0 << 16) // Buffer Size 2048
#define E1000_RCTL_BSIZE_4096 (3 << 16) // Buffer Size 4096
#define E1000_RCTL_BSIZE_8192 ((1 << 16) | (1 << 17)) // Buffer Size 8192
#define E1000_RCTL_SECRC    (1 << 26) // Strip Ethernet CRC

// Transmit Control Register bits
#define E1000_TCTL_EN       (1 << 1)  // Transmit Enable
#define E1000_TCTL_PSP      (1 << 3)  // Pad Short Packets
#define E1000_TCTL_CT_SHIFT 4         // Collision Threshold
#define E1000_TCTL_COLD_SHIFT 12      // Collision Distance
#define E1000_TCTL_SWXOFF   (1 << 22) // Software XOFF Transmission

// Descriptor counts
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32

// RX Descriptor
typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed)) e1000_rx_desc_t;

// TX Descriptor
typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed)) e1000_tx_desc_t;

// RX Descriptor Status bits
#define E1000_RXD_STAT_DD   (1 << 0)  // Descriptor Done
#define E1000_RXD_STAT_EOP  (1 << 1)  // End of Packet

// TX Descriptor Command bits
#define E1000_TXD_CMD_EOP   (1 << 0)  // End of Packet
#define E1000_TXD_CMD_IFCS  (1 << 1)  // Insert FCS
#define E1000_TXD_CMD_RS    (1 << 3)  // Report Status

// TX Descriptor Status bits
#define E1000_TXD_STAT_DD   (1 << 0)  // Descriptor Done

// E1000 device state
typedef struct {
    uint32_t mem_base;
    uint16_t io_base;
    bool has_eeprom;
    
    e1000_rx_desc_t* rx_descs;
    e1000_tx_desc_t* tx_descs;
    
    uint8_t* rx_buffers[E1000_NUM_RX_DESC];
    uint8_t* tx_buffers[E1000_NUM_TX_DESC];
    
    uint16_t rx_current;
    uint16_t tx_current;
    
    net_device_t net_dev;
} e1000_device_t;

/**
 * Initialize E1000 driver and detect devices
 */
void e1000_init(void);

/**
 * Detect and initialize E1000 device from PCI
 */
bool e1000_detect_pci(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * Read from E1000 register
 */
uint32_t e1000_read_reg(e1000_device_t* dev, uint16_t reg);

/**
 * Write to E1000 register
 */
void e1000_write_reg(e1000_device_t* dev, uint16_t reg, uint32_t value);

/**
 * Send packet callback
 */
int e1000_send_packet(net_device_t* netdev, net_packet_t* packet);

/**
 * Initialize device callback
 */
int e1000_init_device(net_device_t* netdev);

/**
 * Shutdown device callback
 */
int e1000_shutdown_device(net_device_t* netdev);

/**
 * Poll for received packets
 */
void e1000_check_packets(void);

#endif // E1000_DRIVER_H
