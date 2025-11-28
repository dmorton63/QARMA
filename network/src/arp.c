#include "arp.h"
#include "ethernet.h"
#include "graphics/graphics.h"
#include "core/string.h"

static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];

void arp_init(void) {
    // Clear ARP cache
    memset(arp_cache, 0, sizeof(arp_cache));
    gfx_print("ARP layer initialized\n");
}

void arp_add_entry(ipv4_addr_t* ip, mac_addr_t* mac) {
    // Find empty slot or oldest entry
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        slot = 0;  // Overwrite first entry
    }
    
    memcpy(&arp_cache[slot].ip, ip, sizeof(ipv4_addr_t));
    memcpy(&arp_cache[slot].mac, mac, sizeof(mac_addr_t));
    arp_cache[slot].valid = true;
}

bool arp_lookup(ipv4_addr_t* ip, mac_addr_t* mac_out) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && 
            memcmp(&arp_cache[i].ip, ip, sizeof(ipv4_addr_t)) == 0) {
            memcpy(mac_out, &arp_cache[i].mac, sizeof(mac_addr_t));
            return true;
        }
    }
    return false;
}

int arp_send_request(net_device_t* dev, ipv4_addr_t* target_ip) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    
    serial_debug("[ARP: start]\n");
    gfx_print("[ARP: start]");
    
    if (!dev || !target_ip) {
        serial_debug("[ARP: null ptr]\n");
        gfx_print("[ARP: null ptr]");
        return -1;
    }
    
    arp_packet_t arp_req;
    
    serial_debug("[ARP: fill]\n");
    gfx_print("[ARP: fill]");
    
    // Fill ARP request
    arp_req.hw_type = __builtin_bswap16(1);         // Ethernet
    arp_req.proto_type = __builtin_bswap16(0x0800); // IPv4
    arp_req.hw_addr_len = 6;
    arp_req.proto_addr_len = 4;
    arp_req.opcode = __builtin_bswap16(ARP_OP_REQUEST);
    
    memcpy(&arp_req.sender_mac, &dev->mac_address, sizeof(mac_addr_t));
    memcpy(&arp_req.sender_ip, &dev->ip_address, sizeof(ipv4_addr_t));
    
    // Target MAC is unknown (broadcast)
    memset(&arp_req.target_mac, 0xFF, sizeof(mac_addr_t));
    memcpy(&arp_req.target_ip, target_ip, sizeof(ipv4_addr_t));
    
    serial_debug("[ARP: send]\n");
    gfx_print("[ARP: send]");
    
    // Send as Ethernet frame (broadcast)
    mac_addr_t broadcast_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    int result = ethernet_send_frame(dev, &broadcast_mac, ETHERTYPE_ARP, 
                                     (uint8_t*)&arp_req, sizeof(arp_req));
    
    serial_debug("[ARP: done]\n");
    gfx_print("[ARP: done]");
    return result;
}

void arp_receive(net_device_t* dev, const uint8_t* data, uint32_t len) {
    extern void serial_debug(const char*);
    
    serial_debug("[ARP_RX: start]\\n");
    
    if (len < sizeof(arp_packet_t)) {
        serial_debug("[ARP_RX: too short]\\n");
        return;
    }
    
    arp_packet_t* arp = (arp_packet_t*)data;
    uint16_t opcode = __builtin_bswap16(arp->opcode);
    
    serial_debug("[ARP_RX: add cache]\\n");
    
    // Add sender to cache
    arp_add_entry(&arp->sender_ip, &arp->sender_mac);
    
    serial_debug("[ARP_RX: cached]\\n");
    
    if (opcode == ARP_OP_REQUEST) {
        // Check if request is for us
        if (memcmp(&arp->target_ip, &dev->ip_address, sizeof(ipv4_addr_t)) == 0) {
            // Send ARP reply
            arp_packet_t arp_reply;
            arp_reply.hw_type = __builtin_bswap16(1);
            arp_reply.proto_type = __builtin_bswap16(0x0800);
            arp_reply.hw_addr_len = 6;
            arp_reply.proto_addr_len = 4;
            arp_reply.opcode = __builtin_bswap16(ARP_OP_REPLY);
            
            memcpy(&arp_reply.sender_mac, &dev->mac_address, sizeof(mac_addr_t));
            memcpy(&arp_reply.sender_ip, &dev->ip_address, sizeof(ipv4_addr_t));
            memcpy(&arp_reply.target_mac, &arp->sender_mac, sizeof(mac_addr_t));
            memcpy(&arp_reply.target_ip, &arp->sender_ip, sizeof(ipv4_addr_t));
            
            ethernet_send_frame(dev, &arp->sender_mac, ETHERTYPE_ARP,
                               (uint8_t*)&arp_reply, sizeof(arp_reply));
        }
    }
    else if (opcode == ARP_OP_REPLY) {
        // Already added to cache above
        gfx_print("ARP: Received reply\n");
    }
}

void arp_print_cache(void) {
    extern void gfx_print(const char*);
    extern void ipv4_addr_to_string(ipv4_addr_t* ip, char* buffer);
    extern void mac_addr_to_string(mac_addr_t* mac, char* buffer);
    
    gfx_print("IP Address        MAC Address         Status\n");
    gfx_print("------------------------------------------------\n");
    
    char ip_buf[16];
    char mac_buf[18];
    
    int count = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid) {
            ipv4_addr_to_string(&arp_cache[i].ip, ip_buf);
            mac_addr_to_string(&arp_cache[i].mac, mac_buf);
            
            gfx_print(ip_buf);
            gfx_print("    ");
            gfx_print(mac_buf);
            gfx_print("    Valid\n");
            count++;
        }
    }
    
    if (count == 0) {
        gfx_print("(ARP cache is empty)\n");
    } else {
        gfx_print("\nTotal entries: ");
        char buf[16];
        int len = 0;
        int num = count;
        if (num == 0) {
            buf[len++] = '0';
        } else {
            char temp[16];
            int temp_len = 0;
            while (num > 0) {
                temp[temp_len++] = '0' + (num % 10);
                num /= 10;
            }
            for (int i = temp_len - 1; i >= 0; i--) {
                buf[len++] = temp[i];
            }
        }
        buf[len] = '\0';
        gfx_print(buf);
        gfx_print("\n");
    }
}
