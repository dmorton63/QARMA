#include "ethernet.h"
#include "graphics.h"
#include "string.h"

void ethernet_init(void) {
    gfx_print("Ethernet layer initialized\n");
}

int ethernet_send_frame(net_device_t* dev, mac_addr_t* dest_mac, uint16_t ethertype, 
                        const uint8_t* payload, uint32_t payload_len) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    
    serial_debug("[ETH: start]\n");
    gfx_print("[ETH: start]");
    
    if (!dev || !dest_mac || !payload) {
        serial_debug("[ETH: null]\n");
        gfx_print("[ETH: null]");
        return -1;
    }
    
    if (payload_len > 1500) {
        serial_debug("Ethernet: Payload too large\n");
        gfx_print("Ethernet: Payload too large\n");
        return -1;
    }
    
    serial_debug("[ETH: create]\n");
    gfx_print("[ETH: create]");
    
    // Create packet with static buffer to avoid stack issues
    static uint8_t packet_buffer[1518];  // Max ethernet frame
    net_packet_t packet;
    packet.data = packet_buffer;
    packet.capacity = sizeof(packet_buffer);
    
    eth_header_t* eth = (eth_header_t*)packet.data;
    
    // Fill in Ethernet header
    memcpy(&eth->dest, dest_mac, sizeof(mac_addr_t));
    memcpy(&eth->src, &dev->mac_address, sizeof(mac_addr_t));
    eth->ethertype = __builtin_bswap16(ethertype);  // Convert to network byte order
    
    // Copy payload
    memcpy(packet.data + sizeof(eth_header_t), payload, payload_len);
    
    packet.length = sizeof(eth_header_t) + payload_len;
    packet.protocol = NET_PROTO_ETHERNET;
    
    serial_debug("[ETH: send]\n");
    gfx_print("[ETH: send]");
    
    // ARP instrumentation: log frame details
    if (ethertype == ETHERTYPE_ARP) {
        extern void netlog_write(const char*);
        extern void netlog_write_hex(const char*, uint32_t);
        netlog_write("[arp-tx] len="); netlog_write_hex("", payload_len); netlog_write(" dest=");
        for (int i=0;i<6;i++){ netlog_write_hex("", dest_mac->addr[i]); }
        netlog_write(" src=");
        for (int i=0;i<6;i++){ netlog_write_hex("", dev->mac_address.addr[i]); }
        netlog_write(" op=");
        if (payload_len >= 8) { // opcode at offset 6 inside ARP struct (after hw/proto types and lens)
            uint16_t op = (payload[6]<<8)|payload[7];
            netlog_write_hex("", op);
        }
        netlog_write("\n");
    }
    // Send via device driver
    if (dev->send_packet) {
        int result = dev->send_packet(dev, &packet);
        if (ethertype == ETHERTYPE_ARP) {
            extern void netlog_write(const char*); extern void netlog_write_hex(const char*, uint32_t);
            netlog_write("[arp-tx] queued result="); netlog_write_hex("", result); netlog_write("\n");
        }
        serial_debug("[ETH: done]\n");
        gfx_print("[ETH: done]");
        return result;
    }
    
    serial_debug("[ETH: no send]\n");
    gfx_print("[ETH: no send]");
    return -1;
}

void ethernet_receive_frame(net_device_t* dev, const uint8_t* frame_data, uint32_t frame_len) {
    extern void serial_debug(const char*);
    extern void netlog_write(const char*);
    extern void netlog_write_hex(const char*, uint32_t);
    
    serial_debug("[ETH_RX: start]\\n");
    
    (void)dev;
    
    if (frame_len < sizeof(eth_header_t)) {
        serial_debug("[ETH_RX: too short]\\n");
        return;  // Frame too short
    }
    
    eth_header_t* eth = (eth_header_t*)frame_data;
    uint16_t ethertype = __builtin_bswap16(eth->ethertype);
    
    const uint8_t* payload = frame_data + sizeof(eth_header_t);
    uint32_t payload_len = frame_len - sizeof(eth_header_t);
    
    // Log a concise RX summary for diagnostics
    netlog_write("[eth-rx] len="); netlog_write_hex("", frame_len); netlog_write(" type="); netlog_write_hex("", ethertype); netlog_write(" dst=");
    for (int i=0;i<6;i++){ netlog_write_hex("", ((const uint8_t*)&eth->dest)[i]); }
    netlog_write(" src=");
    for (int i=0;i<6;i++){ netlog_write_hex("", ((const uint8_t*)&eth->src)[i]); }
    netlog_write("\n");
    serial_debug("[ETH_RX: dispatch]\\n");
    
    // Dispatch to protocol handlers
    switch (ethertype) {
        case ETHERTYPE_ARP:
            serial_debug("[ETH_RX: ARP]\\n");
            extern void arp_receive(net_device_t* dev, const uint8_t* data, uint32_t len);
            arp_receive(dev, payload, payload_len);
            serial_debug("[ETH_RX: ARP done]\\n");
            break;
            
        case ETHERTYPE_IPV4:
            serial_debug("[ETH_RX: IPv4]\\n");
            extern void ipv4_receive(net_device_t* dev, const uint8_t* data, uint32_t len);
            ipv4_receive(dev, payload, payload_len);
            serial_debug("[ETH_RX: IPv4 done]\\n");
            break;
            
        default:
            // Unknown protocol, drop
            break;
    }
}
