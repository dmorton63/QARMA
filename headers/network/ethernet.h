#ifndef ETHERNET_H
#define ETHERNET_H

#include "kernel_types.h"
#include "network_subsystem.h"

// Ethernet frame header (14 bytes)
typedef struct {
    mac_addr_t dest;      // Destination MAC address
    mac_addr_t src;       // Source MAC address
    uint16_t ethertype;   // Protocol type
} __attribute__((packed)) eth_header_t;

// Ethernet frame (max 1518 bytes)
typedef struct {
    eth_header_t header;
    uint8_t payload[1500];  // Maximum payload size
} __attribute__((packed)) eth_frame_t;

// EtherTypes
#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV6  0x86DD

// Functions
void ethernet_init(void);
int ethernet_send_frame(net_device_t* dev, mac_addr_t* dest_mac, uint16_t ethertype, 
                        const uint8_t* payload, uint32_t payload_len);
void ethernet_receive_frame(net_device_t* dev, const uint8_t* frame_data, uint32_t frame_len);

#endif // ETHERNET_H
