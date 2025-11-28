#ifndef ARP_H
#define ARP_H

#include "kernel_types.h"
#include "network_subsystem.h"

// ARP packet structure
typedef struct {
    uint16_t hw_type;      // Hardware type (1 = Ethernet)
    uint16_t proto_type;   // Protocol type (0x0800 = IPv4)
    uint8_t hw_addr_len;   // Hardware address length (6)
    uint8_t proto_addr_len; // Protocol address length (4)
    uint16_t opcode;       // Operation (1=request, 2=reply)
    mac_addr_t sender_mac;
    ipv4_addr_t sender_ip;
    mac_addr_t target_mac;
    ipv4_addr_t target_ip;
} __attribute__((packed)) arp_packet_t;

// ARP opcodes
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

// ARP cache entry
typedef struct {
    ipv4_addr_t ip;
    mac_addr_t mac;
    uint32_t timestamp;
    bool valid;
} arp_cache_entry_t;

#define ARP_CACHE_SIZE 32

// Functions
void arp_init(void);
void arp_receive(net_device_t* dev, const uint8_t* data, uint32_t len);
int arp_send_request(net_device_t* dev, ipv4_addr_t* target_ip);
bool arp_lookup(ipv4_addr_t* ip, mac_addr_t* mac_out);
void arp_add_entry(ipv4_addr_t* ip, mac_addr_t* mac);
void arp_print_cache(void);

#endif // ARP_H
