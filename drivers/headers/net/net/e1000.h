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
#include "network_subsystem.h"

// E1000 PCI Device IDs
#define E1000_VENDOR_ID     0x8086
#define E1000_DEV_ID_82540EM 0x100E
#define E1000_DEV_ID_82545EM 0x100F
#define E1000_DEV_ID_82574L  0x10D3

// Register offsets
#define E1000_REG_CTRL      0x0000  // Device Control
#define E1000_REG_STATUS    0x0008  // Device Status
#define E1000_REG_EECD      0x0010  // EEPROM/Flash Control
#define E1000_REG_EEPROM    0x0014  // EEPROM Read
#define E1000_REG_MDIC      0x0020  // MDI Control
#define E1000_REG_CTRL_EXT  0x0018  // Extended Device Control
#define E1000_REG_IMASK     0x00D0  // Interrupt Mask
#define E1000_REG_ICR       0x00C0  // Interrupt Cause Read
#define E1000_REG_ICS       0x00C8  // Interrupt Cause Set
#define E1000_REG_IMS       0x00D0  // Interrupt Mask Set/Read
#define E1000_REG_RCTL      0x0100  // Receive Control
#define E1000_REG_RCTRL     0x0100  // Receive Control
#define E1000_REG_RXDESCLO  0x2800  // RX Descriptor Base Low
#define E1000_REG_RDBAL     0x2800  // RX Descriptor Base Low
#define E1000_REG_RXDESCBAH 0x2804  // RX Descriptor Base High
#define E1000_REG_RDBAH     0x2804  // RX Descriptor Base
#define E1000_REG_RXDESCHI  0x2804  // RX Descriptor Base High
#define E1000_REG_RXDESCLEN 0x2808  // RX Descriptor Length
#define E1000_REG_RDLEN     0x2808  // RX Descriptor Length
#define E1000_REG_RXDESCHEAD 0x2810 // RX Descriptor Head
#define E1000_REG_RXDESCTAIL 0x2818 // RX Descriptor Tail
#define E1000_REG_TCTRL     0x0400  // Transmit Control
#define E1000_REG_TIPG      0x0410  // Transmit Inter-Packet Gap
#define E1000_REG_TXDESCLO  0x3800  // TX Descriptor Base Low
#define E1000_REG_TXDESCHI  0x3804  // TX Descriptor Base High
#define E1000_REG_TXDESCLEN 0x3808  // TX Descriptor Length
#define E1000_REG_TXDESCHEAD 0x3810 // TX Descriptor Head
#define E1000_REG_TXDESCTAIL 0x3818 // TX Descriptor Tail
#define E1000_REG_RDTR      0x2820  // RX Delay Timer
#define E1000_REG_RXDCTL    0x2828  // RX Descriptor Control
#define E1000_REG_TXDCTL    0x3828  // TX Descriptor Control
#define E1000_REG_RADV      0x282C  // RX Int. Absolute Delay Timer
#define E1000_REG_RSRPD     0x2C00  // RX Small Packet Detect Interrupt
#define E1000_REG_MTA       0x5200  // Multicast Table Array (128 x 32-bit)
#define E1000_REG_RAL0      0x5400  // Receive Address Low 0
#define E1000_REG_RAH0      0x5404  // Receive Address High 0
#define E1000_REG_EEC        0x12010
#define E1000_EEC_AR_DONE    (1u << 9)   // Autoread done
#define E1000_EEC_PRES       (1u << 8)   // EEPROM present
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
// #define E1000_RCTL_EN       (1 << 1)  // Receive Enable
// #define E1000_RCTL_SBP      (1 << 2)  // Store Bad Packets
// #define E1000_RCTL_UPE      (1 << 3)  // Unicast Promiscuous Enable
// #define E1000_RCTL_MPE      (1 << 4)  // Multicast Promiscuous Enable
// #define E1000_RCTL_LPE      (1 << 5)  // Long Packet Enable
// #define E1000_RCTL_LBM_NONE (0 << 6)  // No Loopback
// #define E1000_RCTL_BAM      (1 << 15) // Broadcast Accept Mode
// #define E1000_RCTL_BSIZE_2048 (0 << 16) // Buffer Size 2048
// #define E1000_RCTL_BSIZE_4096 (3 << 16) // Buffer Size 4096
// #define E1000_RCTL_BSIZE_8192 ((1 << 16) | (1 << 17)) // Buffer Size 8192
// Receive Control Register bits (E1000/8254x)

#define E1000_RCTL_EN        (1u << 1)   // Receive Enable
#define E1000_RCTL_SBP       (1u << 2)   // Store Bad Packets
#define E1000_RCTL_UPE       (1u << 3)   // Unicast Promiscuous
#define E1000_RCTL_MPE       (1u << 4)   // Multicast Promiscuous
#define E1000_RCTL_LPE       (1u << 5)   // Long Packet Enable
#define E1000_RCTL_LBM_NONE  (0u << 6)   // No Loopback
#define E1000_RCTL_LBM_MAC   (2u << 6)   // MAC Loopback
#define E1000_RCTL_LBM_PHY   (3u << 6)   // PHY Loopback
#define E1000_RCTL_BAM       (1u << 15)  // Broadcast Accept
#define E1000_RCTL_BSIZE_SHIFT 16
#define E1000_RCTL_BSIZE_256  (3u << E1000_RCTL_BSIZE_SHIFT)
#define E1000_RCTL_BSIZE_512  (2u << E1000_RCTL_BSIZE_SHIFT)
#define E1000_RCTL_BSIZE_1024 (1u << E1000_RCTL_BSIZE_SHIFT)
#define E1000_RCTL_BSIZE_2048 (0u << E1000_RCTL_BSIZE_SHIFT)

#define E1000_RCTL_BSEX     (1u << 25)  // Buffer Size Extension
#define E1000_RCTL_SECRC    (1 << 26) // Strip Ethernet CRC
#define E1000_RCTL_BSIZE_4096 (E1000_RCTL_BSEX | (0u << E1000_RCTL_BSIZE_SHIFT))
#define E1000_RCTL_BSIZE_8192 (E1000_RCTL_BSEX | (1u << E1000_RCTL_BSIZE_SHIFT))
#define E1000_RCTL_BSIZE_16384 (E1000_RCTL_BSEX | (2u << E1000_RCTL_BSIZE_SHIFT))

// Transmit Control Register bits
#define E1000_TCTL_EN       (1 << 1)  // Transmit Enable
#define E1000_TCTL_PSP      (1 << 3)  // Pad Short Packets
#define E1000_TCTL_CT_SHIFT 4         // Collision Threshold
#define E1000_TCTL_COLD_SHIFT 12      // Collision Distance
#define E1000_TCTL_SWXOFF   (1 << 22) // Software XOFF Transmission

// Receive Address High register bits
#define E1000_RAH_AV        (1u << 31) // Address Valid

// MDIC (MDI Control) bits/fields (8254x spec)
// OP field uses two bits: 0x04000000 = write, 0x08000000 = read
// READY bit indicates completion, ERROR bit signals failure.
#define E1000_MDIC_DATA_MASK   0x0000FFFFu
#define E1000_MDIC_REG_SHIFT   16
#define E1000_MDIC_PHY_SHIFT   21
#define E1000_MDIC_OP_WRITE    0x04000000u
#define E1000_MDIC_OP_READ     0x08000000u
#define E1000_MDIC_READY       0x10000000u
#define E1000_MDIC_INT_EN      0x20000000u
#define E1000_MDIC_ERROR       0x40000000u

// RXDCTL threshold field shifts
#define E1000_RXDCTL_WTHRESH_SHIFT 0
#define E1000_RXDCTL_HTHRESH_SHIFT 8
#define E1000_RXDCTL_PTHRESH_SHIFT 16

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
    bool mmio_mapped;
    bool mmio_inert; // MMIO probe indicated writes don't stick
    uint8_t pci_bus, pci_slot, pci_func;
    
    e1000_rx_desc_t* rx_descs;
    e1000_tx_desc_t* tx_descs;
    uint64_t rx_desc_phys;
    uint64_t tx_desc_phys;
    
    uint8_t* rx_buffers[E1000_NUM_RX_DESC];
    uint8_t* tx_buffers[E1000_NUM_TX_DESC];
    uint64_t rx_buf_phys[E1000_NUM_RX_DESC];
    uint64_t tx_buf_phys[E1000_NUM_TX_DESC];
    
    uint16_t rx_current;
    uint16_t tx_current;
    
    // Persistent link state tracking (updated on init and periodic polls)
    bool link_up;           // Cached LU bit from STATUS
    uint32_t last_status;   // Last raw STATUS register value
    uint32_t link_retry_count; // Down polls since last link attempt

    // TX recovery bookkeeping
    uint32_t tx_busy_streak;   // consecutive busy returns
    uint32_t tx_recover_count; // number of recoveries attempted

    net_device_t net_dev;
} e1000_device_t;

// Define init modes as an enum for clarity
typedef enum {
    E1000_INIT_SAFE   = 0, // stub only
    E1000_INIT_STAGE1 = 1, // map MMIO + status
    E1000_INIT_FULL   = 2  // full init
} e1000_init_mode_t;


/**
 * Initialize E1000 driver and detect devices
 */
void e1000_init(void);

// Init modes: 0 = safe (stub only), 1 = stage1 (map MMIO + status), 2 = full
void e1000_set_init_mode(int mode);

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
 * Inject a minimal broadcast Ethernet frame for bring-up testing.
 * Copies a 60-byte payload (excluding FCS) into a TX descriptor.
 */
void e1000_send_test_broadcast(void);

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

// Loopback configuration helpers
typedef enum {
    E1000_LB_OFF = 0,
    E1000_LB_MAC = 2,
    E1000_LB_PHY = 3
} e1000_loopback_mode_t;

// Set loopback mode (off/mac/phy). Safely resequences RX afterward.
void e1000_config_loopback(e1000_loopback_mode_t mode);

// Get current loopback mode from RCTL[7:6]; returns -1 if unavailable.
int e1000_get_loopback_mode(void);

// Dump PHY registers for bring-up diagnostics (prints via gfx/netlog)
void e1000_phy_dump(void);

#endif // E1000_DRIVER_H
