#ifndef UHCI_H
#define UHCI_H

#include "core/kernel.h"
#include "usb.h"

// UHCI Register Offsets
#define UHCI_USBCMD     0x00    // USB Command
#define UHCI_USBSTS     0x02    // USB Status  
#define UHCI_USBINTR    0x04    // USB Interrupt Enable
#define UHCI_FRNUM      0x06    // Frame Number
#define UHCI_FLBASEADD  0x08    // Frame List Base Address
#define UHCI_SOFMOD     0x0C    // Start of Frame Modify
#define UHCI_PORTSC1    0x10    // Port Status/Control 1
#define UHCI_PORTSC2    0x12    // Port Status/Control 2

// UHCI Command Register Bits
#define UHCI_CMD_RS     0x0001  // Run/Stop
#define UHCI_CMD_HCRESET 0x0002 // Host Controller Reset
#define UHCI_CMD_GRESET 0x0004  // Global Reset
#define UHCI_CMD_EGSM   0x0008  // Enter Global Suspend Mode
#define UHCI_CMD_FGR    0x0010  // Force Global Resume
#define UHCI_CMD_SWDBG  0x0020  // Software Debug
#define UHCI_CMD_CF     0x0040  // Configure Flag
#define UHCI_CMD_MAXP   0x0080  // Max Packet (0=32, 1=64)

// UHCI Status Register Bits
#define UHCI_STS_USBINT 0x0001  // USB Interrupt
#define UHCI_STS_ERROR  0x0002  // USB Error Interrupt
#define UHCI_STS_RD     0x0004  // Resume Detect
#define UHCI_STS_HSE    0x0008  // Host System Error
#define UHCI_STS_HCPE   0x0010  // Host Controller Process Error
#define UHCI_STS_HCH    0x0020  // Host Controller Halted

// UHCI Port Status/Control Bits
#define UHCI_PORT_CCS   0x0001  // Current Connect Status
#define UHCI_PORT_CSC   0x0002  // Connect Status Change
#define UHCI_PORT_PE    0x0004  // Port Enable
#define UHCI_PORT_PEC   0x0008  // Port Enable Change
#define UHCI_PORT_LS    0x0030  // Line Status (bits 4-5)
#define UHCI_PORT_RD    0x0040  // Resume Detect
#define UHCI_PORT_LSDA  0x0100  // Low Speed Device Attached
#define UHCI_PORT_PR    0x0200  // Port Reset
#define UHCI_PORT_SUSP  0x1000  // Suspend
#define UHCI_PORT_RWC   (UHCI_PORT_CSC | UHCI_PORT_PEC)  // Read-Write-Clear bits

// TD Link Pointer Flags
#define TD_PTR_TERMINATE  0x00000001  // Terminate (no next TD/QH)
#define TD_PTR_QH         0x00000002  // Link points to QH (vs TD)
#define TD_PTR_DEPTH      0x00000004  // Depth-first traversal
#define TD_PTR_MASK       0xFFFFFFF0  // Physical address mask

// Transfer Descriptor (TD) Structure
typedef struct uhci_td_hw {
    uint32_t link_ptr;      // Link to next TD
    uint32_t control;       // Control and status bits
    uint32_t token;         // Token (device address, endpoint, etc.)
    uint32_t buffer;        // Buffer pointer
 } __attribute__((packed, aligned(16))) uhci_td_hw_t;

typedef struct uhci_td {
    uhci_td_hw_t hw;
    uint32_t link_ptr;      // Link to next TD
    // // Software fields (not seen by hardware)
    struct uhci_td *next;   // Software link
    void *callback_data;    // For completion callbacks
    void (*callback)(usb_transfer_t *);
} __attribute__((packed)) uhci_td_t;

// Queue Head (QH) Structure
typedef struct uhci_qh {
    uint32_t link_ptr;      // Link to next QH/TD
    uint32_t element_ptr;   // Element link pointer
    
    // Software fields
    struct uhci_qh *next;
    uhci_td_t *first_td;
} __attribute__((packed)) uhci_qh_t;

// UHCI Controller Structure
typedef struct uhci_controller {
    uint16_t io_base;       // I/O port base address
    uint32_t *frame_list;   // Frame list (1024 entries)
    uhci_qh_t *int_qh;      // Interrupt queue head
    uhci_qh_t *ctrl_qh;     // Control queue head
    uhci_qh_t *bulk_qh;     // Bulk queue head
    
    // Memory pools
    uhci_td_t *td_pool;     // Pool of TDs
    uhci_qh_t *qh_pool;     // Pool of QHs
    
    uint8_t bus, slot, func; // PCI location
} uhci_controller_t;

// TD Control Bits
#define UHCI_TD_BITSTUFF    0x00020000
#define UHCI_TD_CRC_TIMEOUT 0x00040000
#define UHCI_TD_NAK         0x00080000
#define UHCI_TD_BABBLE      0x00100000
#define UHCI_TD_DATABUFFER  0x00200000
#define UHCI_TD_STALL       0x00400000
#define UHCI_TD_ACTIVE      0x00800000
#define UHCI_TD_IOC         0x01000000  // Interrupt on Complete
#define UHCI_TD_IOS         0x02000000  // Isochronous Select
#define UHCI_TD_LS          0x04000000  // Low Speed
#define UHCI_TD_C_ERR       0x18000000  // Error count (bits 27-28)
#define UHCI_TD_SPD         0x20000000  // Short Packet Detect

// TD Token Bits
#define UHCI_TD_PID_SETUP   0x2D
#define UHCI_TD_PID_IN      0x69
#define UHCI_TD_PID_OUT     0xE1



// Function declarations
int uhci_pci_init(void);
int uhci_init_controller(uint8_t bus, uint8_t slot, uint8_t func, uint16_t io_base);
void uhci_reset_controller(uhci_controller_t *uhci);
int uhci_start_controller(uhci_controller_t *uhci);
void uhci_detect_ports(uhci_controller_t *uhci);

uhci_td_t *uhci_alloc_td(uhci_controller_t *uhci);
void uhci_free_td(uhci_controller_t *uhci, uhci_td_t *td);
uhci_qh_t *uhci_alloc_qh(uhci_controller_t *uhci);
void uhci_free_qh(uhci_controller_t *uhci, uhci_qh_t *qh);

int uhci_control_transfer(uhci_controller_t *uhci, usb_device_t *device, 
                         usb_setup_packet_t *setup, void *data, uint16_t length);
int uhci_interrupt_transfer(uhci_controller_t *uhci, usb_device_t *device,
                           uint8_t endpoint, void *data, uint16_t length,
                           void (*callback)(usb_transfer_t *));

void uhci_interrupt_handler(uhci_controller_t *uhci);
bool uhci_port_device_connected(uhci_controller_t *uhci, int port);
bool uhci_reset_port(uhci_controller_t *uhci, int port);

void uhci_delay_ms(int ms);

// Runtime control for CLFLUSH use (set via kernel cmdline)
void uhci_set_clflush_enabled(int enabled);

// Bulk transfer helper: schedule bulk TDs for an IN or OUT transfer on a non-control endpoint.
// pid should be UHCI_TD_PID_IN or UHCI_TD_PID_OUT. For IN, buffer will be filled.
int uhci_bulk_transfer(uhci_controller_t *uhci, usb_device_t *device, uint8_t pid, uint8_t endpoint, void *buffer, uint32_t length);

// Port control helpers
void uhci_enable_port(uhci_controller_t *uhci, int port);
void uhci_disable_port(uhci_controller_t *uhci, int port);

// Global controller list
extern uhci_controller_t *g_uhci_controllers;
extern int g_uhci_count;

#endif // UHCI_H