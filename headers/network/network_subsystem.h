/**
 * @file network_subsystem.h
 * @brief Network subsystem for QARMA
 * 
 * Provides:
 * - Network stack management
 * - Ethernet/IP/TCP/UDP protocol support
 * - Socket interface
 * - Network device drivers
 * - Port management integration
 */

#ifndef NETWORK_SUBSYSTEM_H
#define NETWORK_SUBSYSTEM_H

#include "kernel_types.h"

// Network device states
typedef enum {
    NET_DEV_DOWN = 0,
    NET_DEV_UP = 1,
    NET_DEV_RUNNING = 2,
    NET_DEV_ERROR = 3
} net_device_state_t;

// Network protocols
typedef enum {
    NET_PROTO_ETHERNET = 0,
    NET_PROTO_ARP = 1,
    NET_PROTO_IP = 2,
    NET_PROTO_ICMP = 3,
    NET_PROTO_TCP = 4,
    NET_PROTO_UDP = 5
} net_protocol_t;

// MAC address structure
typedef struct {
    uint8_t addr[6];
} mac_addr_t;

// IPv4 address structure
typedef struct {
    uint8_t addr[4];
} ipv4_addr_t;

// Network packet structure
typedef struct {
    uint8_t* data;
    uint32_t length;
    uint32_t capacity;
    net_protocol_t protocol;
    void* protocol_header;
} net_packet_t;

// Network device structure
typedef struct net_device {
    char name[16];              // e.g., "eth0"
    mac_addr_t mac_address;
    ipv4_addr_t ip_address;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    net_device_state_t state;
    uint32_t mtu;               // Maximum Transmission Unit
    
    // Statistics
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    
    // Driver callbacks
    int (*send_packet)(struct net_device* dev, net_packet_t* packet);
    int (*receive_packet)(struct net_device* dev, net_packet_t* packet);
    int (*init)(struct net_device* dev);
    int (*shutdown)(struct net_device* dev);
} net_device_t;

// Socket structure (basic)
typedef struct {
    uint16_t local_port;
    uint16_t remote_port;
    ipv4_addr_t remote_ip;
    net_protocol_t protocol;
    bool is_connected;
    bool is_listening;
} socket_t;

// Network subsystem statistics
typedef struct {
    uint32_t devices_registered;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_dropped;
    uint32_t active_sockets;
} network_stats_t;

/**
 * Initialize the network subsystem
 */
void network_subsystem_init(void);

/**
 * Register a network device
 */
int network_register_device(net_device_t* device);

/**
 * Unregister a network device
 */
int network_unregister_device(net_device_t* device);

/**
 * Send a packet through a network device
 */
int network_send_packet(const char* device_name, net_packet_t* packet);

/**
 * Receive a packet (called by device drivers)
 */
int network_receive_packet(net_device_t* device, net_packet_t* packet);

/**
 * Get network subsystem statistics
 */
void network_get_stats(network_stats_t* stats);

/**
 * Print network device information
 */
void network_print_devices(void);

/**
 * Configure network device
 */
int network_configure_device(const char* device_name, 
                            ipv4_addr_t* ip, 
                            ipv4_addr_t* netmask,
                            ipv4_addr_t* gateway);

/**
 * Bring network device up
 */
int network_device_up(const char* device_name);

/**
 * Bring network device down
 */
int network_device_down(const char* device_name);

// Helper functions
void mac_addr_to_string(mac_addr_t* mac, char* buffer);
void ipv4_addr_to_string(ipv4_addr_t* ip, char* buffer);
bool ipv4_addr_from_string(const char* str, ipv4_addr_t* ip);
net_device_t* network_get_device(uint32_t index);
net_device_t* network_get_default_device(void);

#endif // NETWORK_SUBSYSTEM_H
