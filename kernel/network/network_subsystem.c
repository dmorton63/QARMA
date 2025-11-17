/**
 * @file network_subsystem.c
 * @brief Network subsystem implementation
 */

#include "network_subsystem.h"
#include "core/scheduler/subsystem_registry.h"
#include "core/core_manager.h"
#include "core/string.h"

#define MAX_NETWORK_DEVICES 8

// Network devices registry
static net_device_t* network_devices[MAX_NETWORK_DEVICES];
static uint32_t device_count = 0;
static network_stats_t net_stats = {0};
static bool initialized = false;

// Network subsystem definition
static subsystem_t network_subsystem = {
    .id = SUBSYSTEM_NETWORK,
    .name = "network",
    .type = SUBSYSTEM_TYPE_NETWORK,
    .state = SUBSYSTEM_STATE_STOPPED,
    .start = NULL,
    .stop = NULL,
    .restart = NULL,
    .message_handler = NULL,
    .memory_limit_kb = 0,
    .cpu_affinity_mask = 0xFF,
    .stats_uptime_ms = 0,
    .stats_messages_handled = 0
};

void network_subsystem_init(void) {
    if (initialized) {
        return;
    }
    
    extern void gfx_print(const char*);
    gfx_print("Initializing Network subsystem...\n");
    
    // Clear device registry
    for (int i = 0; i < MAX_NETWORK_DEVICES; i++) {
        network_devices[i] = NULL;
    }
    
    device_count = 0;
    net_stats.devices_registered = 0;
    net_stats.packets_sent = 0;
    net_stats.packets_received = 0;
    net_stats.packets_dropped = 0;
    net_stats.active_sockets = 0;
    
    // Initialize protocol layers
    extern void ethernet_init(void);
    extern void arp_init(void);
    extern void ipv4_init(void);
    extern void icmp_init(void);
    extern void udp_init(void);
    extern void tcp_init(void);
    
    ethernet_init();
    arp_init();
    ipv4_init();
    icmp_init();
    udp_init();
    tcp_init();
    
    // Register with subsystem registry
    extern bool subsystem_register(subsystem_t *subsystem, char* name, uint16_t id);
    subsystem_register(&network_subsystem, "network", SUBSYSTEM_NETWORK);
    
    initialized = true;
    gfx_print("Network subsystem initialized.\n");
}

int network_register_device(net_device_t* device) {
    if (!device || device_count >= MAX_NETWORK_DEVICES) {
        return -1;
    }
    
    network_devices[device_count] = device;
    device_count++;
    net_stats.devices_registered++;
    
    extern void gfx_print(const char*);
    gfx_print("Network device registered: ");
    gfx_print(device->name);
    gfx_print("\n");
    
    return 0;
}

int network_unregister_device(net_device_t* device) {
    if (!device) {
        return -1;
    }
    
    for (uint32_t i = 0; i < device_count; i++) {
        if (network_devices[i] == device) {
            // Shift remaining devices
            for (uint32_t j = i; j < device_count - 1; j++) {
                network_devices[j] = network_devices[j + 1];
            }
            device_count--;
            net_stats.devices_registered--;
            return 0;
        }
    }
    
    return -1;
}

static net_device_t* find_device(const char* name) {
    for (uint32_t i = 0; i < device_count; i++) {
        if (network_devices[i] && strcmp(network_devices[i]->name, name) == 0) {
            return network_devices[i];
        }
    }
    return NULL;
}

int network_send_packet(const char* device_name, net_packet_t* packet) {
    if (!packet) {
        return -1;
    }
    
    net_device_t* device = find_device(device_name);
    if (!device) {
        net_stats.packets_dropped++;
        return -1;
    }
    
    if (device->state != NET_DEV_RUNNING) {
        net_stats.packets_dropped++;
        return -1;
    }
    
    if (device->send_packet) {
        int result = device->send_packet(device, packet);
        if (result == 0) {
            device->tx_packets++;
            device->tx_bytes += packet->length;
            net_stats.packets_sent++;
        } else {
            device->tx_errors++;
            net_stats.packets_dropped++;
        }
        return result;
    }
    
    return -1;
}

int network_receive_packet(net_device_t* device, net_packet_t* packet) {
    if (!device || !packet) {
        return -1;
    }
    
    device->rx_packets++;
    device->rx_bytes += packet->length;
    net_stats.packets_received++;
    
    // TODO: Process packet through protocol stack
    // For now, just update statistics
    
    return 0;
}

void network_get_stats(network_stats_t* stats) {
    if (stats) {
        stats->devices_registered = net_stats.devices_registered;
        stats->packets_sent = net_stats.packets_sent;
        stats->packets_received = net_stats.packets_received;
        stats->packets_dropped = net_stats.packets_dropped;
        stats->active_sockets = net_stats.active_sockets;
    }
}

void network_print_devices(void) {
    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    
    gfx_print("\n=== Network Devices ===\n");
    
    if (device_count == 0) {
        gfx_print("No network devices registered.\n\n");
        return;
    }
    
    for (uint32_t i = 0; i < device_count; i++) {
        net_device_t* dev = network_devices[i];
        if (!dev) continue;
        
        gfx_print("\nDevice: ");
        gfx_print(dev->name);
        gfx_print("\n");
        
        gfx_print("  State: ");
        switch (dev->state) {
            case NET_DEV_DOWN: gfx_print("DOWN"); break;
            case NET_DEV_UP: gfx_print("UP"); break;
            case NET_DEV_RUNNING: gfx_print("RUNNING"); break;
            case NET_DEV_ERROR: gfx_print("ERROR"); break;
        }
        gfx_print("\n");
        
        gfx_print("  MAC: ");
        char mac_str[18];
        mac_addr_to_string(&dev->mac_address, mac_str);
        gfx_print(mac_str);
        gfx_print("\n");
        
        gfx_print("  IP: ");
        char ip_str[16];
        ipv4_addr_to_string(&dev->ip_address, ip_str);
        gfx_print(ip_str);
        gfx_print("\n");
        
        gfx_print("  RX packets: ");
        gfx_print_hex((uint32_t)dev->rx_packets);
        gfx_print(" (");
        gfx_print_hex((uint32_t)(dev->rx_bytes / 1024));
        gfx_print(" KB)\n");
        
        gfx_print("  TX packets: ");
        gfx_print_hex((uint32_t)dev->tx_packets);
        gfx_print(" (");
        gfx_print_hex((uint32_t)(dev->tx_bytes / 1024));
        gfx_print(" KB)\n");
    }
    gfx_print("\n");
}

int network_configure_device(const char* device_name, 
                            ipv4_addr_t* ip, 
                            ipv4_addr_t* netmask,
                            ipv4_addr_t* gateway) {
    net_device_t* device = find_device(device_name);
    if (!device) {
        return -1;
    }
    
    if (ip) {
        device->ip_address = *ip;
    }
    if (netmask) {
        device->netmask = *netmask;
    }
    if (gateway) {
        device->gateway = *gateway;
    }
    
    return 0;
}

int network_device_up(const char* device_name) {
    net_device_t* device = find_device(device_name);
    if (!device) {
        return -1;
    }
    
    if (device->init && device->init(device) != 0) {
        device->state = NET_DEV_ERROR;
        return -1;
    }
    
    device->state = NET_DEV_RUNNING;
    return 0;
}

int network_device_down(const char* device_name) {
    net_device_t* device = find_device(device_name);
    if (!device) {
        return -1;
    }
    
    if (device->shutdown) {
        device->shutdown(device);
    }
    
    device->state = NET_DEV_DOWN;
    return 0;
}

// Helper functions
void mac_addr_to_string(mac_addr_t* mac, char* buffer) {
    const char hex[] = "0123456789ABCDEF";
    int pos = 0;
    
    for (int i = 0; i < 6; i++) {
        buffer[pos++] = hex[(mac->addr[i] >> 4) & 0x0F];
        buffer[pos++] = hex[mac->addr[i] & 0x0F];
        if (i < 5) {
            buffer[pos++] = ':';
        }
    }
    buffer[pos] = '\0';
}

void ipv4_addr_to_string(ipv4_addr_t* ip, char* buffer) {
    int pos = 0;
    
    for (int i = 0; i < 4; i++) {
        uint8_t octet = ip->addr[i];
        
        if (octet >= 100) {
            buffer[pos++] = '0' + (octet / 100);
            octet %= 100;
        }
        if (octet >= 10) {
            buffer[pos++] = '0' + (octet / 10);
            octet %= 10;
        }
        buffer[pos++] = '0' + octet;
        
        if (i < 3) {
            buffer[pos++] = '.';
        }
    }
    buffer[pos] = '\0';
}

bool ipv4_addr_from_string(const char* str, ipv4_addr_t* ip) {
    if (!str || !ip) {
        return false;
    }
    
    int octet = 0;
    int value = 0;
    
    for (int i = 0; str[i] != '\0' && octet < 4; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            value = value * 10 + (str[i] - '0');
            if (value > 255) {
                return false;
            }
        } else if (str[i] == '.') {
            ip->addr[octet++] = (uint8_t)value;
            value = 0;
        } else {
            return false;
        }
    }
    
    if (octet == 3) {
        ip->addr[octet] = (uint8_t)value;
        return true;
    }
    
    return false;
}

net_device_t* network_get_device(uint32_t index) {
    if (index >= device_count) {
        return NULL;
    }
    return network_devices[index];
}

net_device_t* network_get_default_device(void) {
    // Return first registered device
    if (device_count > 0) {
        return network_devices[0];
    }
    return NULL;
}

