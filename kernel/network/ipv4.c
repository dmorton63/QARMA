#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "graphics/graphics.h"
#include "core/string.h"

static uint16_t ip_packet_id = 1;

void ipv4_init(void) {
    gfx_print("IPv4 layer initialized\n");
}

uint16_t ipv4_checksum(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    
    // Sum all 16-bit words
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t word = (data[i] << 8) | data[i + 1];
        sum += word;
    }
    
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

int ipv4_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint8_t protocol,
              const uint8_t* payload, uint32_t payload_len) {
    extern void serial_debug(const char*);
    
    serial_debug("[IPv4: start]\\n");
    
    if (!dev || !dest_ip || !payload) {
        serial_debug("[IPv4: null ptr]\\n");
        return -1;
    }
    
    serial_debug("[IPv4: ARP lookup]\\n");
    
    // Look up destination MAC via ARP
    mac_addr_t dest_mac;
    if (!arp_lookup(dest_ip, &dest_mac)) {
        // Need to send ARP request first
        gfx_print("IPv4: MAC address not in ARP cache, sending request\n");
        arp_send_request(dev, dest_ip);
        return -1;  // Packet will need to be retried after ARP resolves
    }
    
    serial_debug("[IPv4: build packet]\\n");
    
    // Build IP packet - use static buffer to avoid stack overflow
    static uint8_t packet[1500];
    ipv4_header_t* ip = (ipv4_header_t*)packet;
    
    ip->version_ihl = 0x45;  // Version 4, IHL 5 (20 bytes)
    ip->tos = 0;
    ip->total_length = __builtin_bswap16(sizeof(ipv4_header_t) + payload_len);
    ip->id = __builtin_bswap16(ip_packet_id++);
    ip->flags_offset = __builtin_bswap16(0x4000);  // Don't fragment
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;  // Will calculate below
    memcpy(&ip->src_ip, &dev->ip_address, sizeof(ipv4_addr_t));
    memcpy(&ip->dest_ip, dest_ip, sizeof(ipv4_addr_t));
    
    // Calculate checksum
    ip->checksum = ipv4_checksum((uint8_t*)ip, sizeof(ipv4_header_t));
    
    serial_debug("[IPv4: copy payload]\\n");
    
    // Copy payload
    memcpy(packet + sizeof(ipv4_header_t), payload, payload_len);
    
    serial_debug("[IPv4: send eth]\\n");
    
    // Send via Ethernet
    int result = ethernet_send_frame(dev, &dest_mac, ETHERTYPE_IPV4, packet, 
                                     sizeof(ipv4_header_t) + payload_len);
    
    serial_debug("[IPv4: done]\\n");
    
    return result;
}

void ipv4_receive(net_device_t* dev, const uint8_t* data, uint32_t len) {
    extern void serial_debug(const char*);
    
    serial_debug("[IPv4_RX: start]\\n");
    
    if (len < sizeof(ipv4_header_t)) {
        serial_debug("[IPv4_RX: too short]\\n");
        return;
    }
    
    ipv4_header_t* ip = (ipv4_header_t*)data;
    
    serial_debug("[IPv4_RX: checksum]\\n");
    
    // Verify checksum
    uint16_t received_checksum = ip->checksum;
    ipv4_header_t temp;
    memcpy(&temp, ip, sizeof(ipv4_header_t));
    temp.checksum = 0;
    uint16_t calculated_checksum = ipv4_checksum((uint8_t*)&temp, sizeof(ipv4_header_t));
    
    if (received_checksum != calculated_checksum) {
        serial_debug("[IPv4_RX: bad checksum]\\n");
        gfx_print("IPv4: Checksum mismatch\n");
        return;
    }
    
    serial_debug("[IPv4_RX: check dest]\\n");
    
    // Check if packet is for us
    if (memcmp(&ip->dest_ip, &dev->ip_address, sizeof(ipv4_addr_t)) != 0) {
        serial_debug("[IPv4_RX: not for us]\\n");
        return;  // Not for us
    }
    
    serial_debug("[IPv4_RX: dispatch]\\n");
    
    const uint8_t* payload = data + sizeof(ipv4_header_t);
    uint32_t payload_len = len - sizeof(ipv4_header_t);
    
    // Dispatch to protocol handlers
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            serial_debug("[IPv4_RX: ICMP]\\n");
            extern void icmp_receive(net_device_t* dev, ipv4_addr_t* src_ip, 
                                    const uint8_t* data, uint32_t len);
            icmp_receive(dev, &ip->src_ip, payload, payload_len);
            serial_debug("[IPv4_RX: ICMP done]\\n");
            break;
            
        case IP_PROTO_TCP:
            extern void tcp_receive(net_device_t* dev, ipv4_addr_t* src_ip,
                                   const uint8_t* data, uint32_t len);
            tcp_receive(dev, &ip->src_ip, payload, payload_len);
            break;
            
        case IP_PROTO_UDP:
            extern void udp_receive(net_device_t* dev, ipv4_addr_t* src_ip,
                                   const uint8_t* data, uint32_t len);
            udp_receive(dev, &ip->src_ip, payload, payload_len);
            break;
            
        default:
            gfx_print("IPv4: Unknown protocol\n");
            break;
    }
}
