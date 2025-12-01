/**
 * @file e1000.c
 * @brief Intel E1000 Gigabit Ethernet driver implementation
 */

#include "net/net/e1000.h"
#include "pci.h"
#include "io.h"
#include "memory/heap.h"
#include "memory/pmm/pmm.h"
#include "memory/dma_allocator.h"
#include "string.h"
#include "log_timestamp.h"
#include "netlog.h"

// Define offsetof if not available
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif

static e1000_device_t* e1000_dev = NULL;
static bool g_e1000_poll_enabled = false;
static bool g_e1000_safe_mode = true; // avoid MMIO until mapping hardened
static int g_e1000_init_mode = 0; // 0=safe, 1=stage1, 2=full

// Define memory allocation wrappers
#define kmalloc(size) heap_alloc(size)
// For 8254x, enabling is implicit when RCTL.EN set, but thresholds still matter:
static inline void e1000_rx_queue_cfg(e1000_device_t* dev) {
    uint32_t rxdctl = (1u << E1000_RXDCTL_PTHRESH_SHIFT)
                    | (1u << E1000_RXDCTL_HTHRESH_SHIFT)
                    | (1u << E1000_RXDCTL_WTHRESH_SHIFT);
    e1000_write_reg(dev, E1000_REG_RXDCTL, rxdctl);
}

// Forceful RX resequencing: disable RX, reset head/tail, re-enable, and re-advertise
static void e1000_rx_resequence(e1000_device_t* dev, const char* reason) {
    if (!dev || !dev->mem_base) return;
    if (!reason) reason = "unknown";
    uint32_t rctl_before = e1000_read_reg(dev, E1000_REG_RCTRL);
    uint32_t rdh_before  = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
    uint32_t rdt_before  = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
    netlog_write("[rxrese] begin reason="); netlog_write(reason);
    netlog_write(" RCTL="); netlog_write_hex("", rctl_before);
    netlog_write(" H="); netlog_write_hex("", rdh_before);
    netlog_write(" T="); netlog_write_hex("", rdt_before);
    netlog_write("\n");

    // Disable RX
    e1000_write_reg(dev, E1000_REG_RCTRL, (rctl_before & ~E1000_RCTL_EN));
    (void)e1000_read_reg(dev, E1000_REG_STATUS);
    // Reset head, post all with tail=N-1
    e1000_write_reg(dev, E1000_REG_RXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    (void)e1000_read_reg(dev, E1000_REG_STATUS);
    // Re-enable RX
    e1000_write_reg(dev, E1000_REG_RCTRL, (rctl_before | E1000_RCTL_EN));
    (void)e1000_read_reg(dev, E1000_REG_STATUS);
    // Re-advertise tail once more to ensure visibility
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

    uint32_t rctl_after = e1000_read_reg(dev, E1000_REG_RCTRL);
    uint32_t rdh_after  = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
    uint32_t rdt_after  = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
    netlog_write("[rxrese] end RCTL="); netlog_write_hex("", rctl_after);
    netlog_write(" H="); netlog_write_hex("", rdh_after);
    netlog_write(" T="); netlog_write_hex("", rdt_after);
    netlog_write("\n");
}

// static inline void e1000_pci_enable(uint8_t bus, uint8_t slot, uint8_t func) {
//     uint16_t cmd = pci_cfg_read16(bus, slot, func, 0x04);
//     cmd |= (1u << 1) | (1u << 2); // MSE | BME
//     pci_cfg_write16(bus, slot, func, 0x04, cmd);
// }

// static inline void e1000_write_reg(e1000_device_t* dev, uint16_t reg, uint32_t v) {
//     *(volatile uint32_t *)(uintptr_t)(dev->mem_base + reg) = v;
// }
// static inline uint32_t e1000_read_reg(e1000_device_t* dev, uint16_t reg) {
//     return *(volatile uint32_t *)(uintptr_t)(dev->mem_base + reg);
// }

// Simple DMA page mapper: map physical pages to a fixed VA window
extern void vmm_map_page(uint64_t virtual_addr, uint64_t physical_addr, uint64_t flags);
#define PAGE_PRESENT  0x001
#define PAGE_WRITE    0x002
#define PAGE_NO_CACHE 0x040
static uint64_t g_dma_next_va = 0x00000000E2000000ULL;
static void* dma_kmap_page(uint64_t phys) {
    if (phys == 0) {
        // Invalid physical address; avoid mapping page 0
        return NULL;
    }
    uint64_t va = g_dma_next_va;
    vmm_map_page(va, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);
    g_dma_next_va += 0x1000ULL;
    // Return a 64-bit safe pointer to the mapped VA window
    return (void*)(uintptr_t)va;
}

// Safe writer helpers for packed descriptors to avoid any potential
// misaligned 64-bit store issues on some toolchains/ABIs.
static inline void rx_desc_set_addr(e1000_rx_desc_t* d, uint64_t phys) {
    uint32_t lo = (uint32_t)(phys & 0xFFFFFFFFULL);
    uint32_t hi = (uint32_t)((phys >> 32) & 0xFFFFFFFFULL);
    uint32_t* w = (uint32_t*)d;
    w[0] = lo;
    w[1] = hi;
}
static inline void tx_desc_set_addr(e1000_tx_desc_t* d, uint64_t phys) {
    uint32_t lo = (uint32_t)(phys & 0xFFFFFFFFULL);
    uint32_t hi = (uint32_t)((phys >> 32) & 0xFFFFFFFFULL);
    uint32_t* w = (uint32_t*)d;
    w[0] = lo;
    w[1] = hi;
}

// Ensure DMA allocations are below 4GB for 32-bit capable devices (e1000 variants)
static uint64_t alloc_dma_page_below_4g(void) {
    // Allocate a non-zero page strictly below 4GB. Treat 0 as failure and retry.
    for (int tries = 0; tries < 512; ++tries) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            // OOM or allocation failed this attempt; retry a few times
            continue;
        }
        if (phys < 0x100000000ULL) {
            return phys;
        }
        // Above 4G: skip and retry
    }
    return 0; // give up; caller must handle failure
}

#define E1000_RX_BUF_SIZE 2048
#define E1000_TX_BUF_SIZE 2048
#define pci_config_read_word pci_read_config_word
#define pci_config_read_dword pci_read_config_dword

// PCI write function
static inline void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | 
                      ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    uint32_t old = inl(0xCFC);
    uint32_t mask = 0xFFFF << ((offset & 2) * 8);
    uint32_t new_val = (old & ~mask) | ((uint32_t)value << ((offset & 2) * 8));
    outl(0xCFC, new_val);
}

uint32_t e1000_read_reg(e1000_device_t* dev, uint16_t reg) {
    if (!dev || !dev->mem_base || !dev->mmio_mapped) {
        return 0; // MMIO not available; avoid fault
    }
    volatile uint32_t* addr = (volatile uint32_t*)((uintptr_t)dev->mem_base + (uintptr_t)reg);
    return *addr;
}

void e1000_write_reg(e1000_device_t* dev, uint16_t reg, uint32_t value) {
    if (!dev || !dev->mem_base || !dev->mmio_mapped) {
        return; // MMIO not available; avoid fault
    }
    volatile uint32_t* addr = (volatile uint32_t*)((uintptr_t)dev->mem_base + (uintptr_t)reg);
    *addr = value;
}

// Control RX polling to avoid input lag
void e1000_set_polling_enabled(bool enabled) {
    g_e1000_poll_enabled = enabled;
}

void e1000_set_init_mode(int mode) {
    if (mode < 0) mode = 0; if (mode > 2) mode = 2;
    g_e1000_init_mode = mode;
    g_e1000_safe_mode = (mode == 0);
}

static uint16_t e1000_read_eeprom(e1000_device_t* dev, uint8_t addr) {
    uint32_t tmp = 0;
    // Start EEPROM read
    e1000_write_reg(dev, E1000_REG_EEPROM, 1 | ((uint32_t)(addr) << 8));
    // Poll with timeout to avoid hangs if EEPROM not present/ready
    int spins = 0;
    while (spins++ < 100000) { // ~bounded wait
        tmp = e1000_read_reg(dev, E1000_REG_EEPROM);
        if (tmp & (1 << 4)) break;
    }
    if (!(tmp & (1 << 4))) {
        // Timed out: indicate no data
        return 0;
    }
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static void e1000_read_mac_address(e1000_device_t* dev) {
    // Preferred: read current MAC from RAL0/RAH0 registers
    uint32_t ral = e1000_read_reg(dev, E1000_REG_RAL0);
    uint32_t rah = e1000_read_reg(dev, E1000_REG_RAH0);
    uint8_t mac_from_regs[6] = {
        (uint8_t)(ral & 0xFF),
        (uint8_t)((ral >> 8) & 0xFF),
        (uint8_t)((ral >> 16) & 0xFF),
        (uint8_t)((ral >> 24) & 0xFF),
        (uint8_t)(rah & 0xFF),
        (uint8_t)((rah >> 8) & 0xFF)
    };

    bool regs_zero = (ral == 0) && ((rah & 0xFFFF) == 0);
    if (!regs_zero) {
        memcpy(dev->net_dev.mac_address.addr, mac_from_regs, 6);
        return;
    }

    // Fallback: try EEPROM words 0..2 (guarded by presence and non-zero reads)
    if (dev->has_eeprom) {
        uint16_t w0 = e1000_read_eeprom(dev, 0);
        uint16_t w1 = e1000_read_eeprom(dev, 1);
        uint16_t w2 = e1000_read_eeprom(dev, 2);
        uint8_t mac_tmp[6] = { (uint8_t)(w0 & 0xFF), (uint8_t)(w0 >> 8),
                               (uint8_t)(w1 & 0xFF), (uint8_t)(w1 >> 8),
                               (uint8_t)(w2 & 0xFF), (uint8_t)(w2 >> 8) };
        bool all_zero = true;
        for (int i = 0; i < 6; ++i) if (mac_tmp[i] != 0) { all_zero = false; break; }
        if (!all_zero) {
            memcpy(dev->net_dev.mac_address.addr, mac_tmp, 6);
            return;
        }
    }

    // As a last resort leave zeros; caller may apply a default
    memset(dev->net_dev.mac_address.addr, 0, 6);
}

static void e1000_program_mac_address(e1000_device_t* dev) {
    if (!dev || !dev->mem_base) return;
    uint8_t* m = dev->net_dev.mac_address.addr;
    uint32_t ral = ((uint32_t)m[0]) | ((uint32_t)m[1] << 8) | ((uint32_t)m[2] << 16) | ((uint32_t)m[3] << 24);
    uint32_t rah = ((uint32_t)m[4]) | ((uint32_t)m[5] << 8) | E1000_RAH_AV;
    e1000_write_reg(dev, E1000_REG_RAL0, ral);
    e1000_write_reg(dev, E1000_REG_RAH0, rah);
}

// MDIC helpers for PHY access
static bool mdic_wait_ready(e1000_device_t* dev, uint32_t* val_out) {
    int spins = 0; uint32_t v = 0;
    while (spins++ < 100000) {
        v = e1000_read_reg(dev, E1000_REG_MDIC);
        if (v & E1000_MDIC_READY) { if (val_out) *val_out = v; return true; }
    }
    if (val_out) *val_out = v;
    return false;
}

static bool mdic_write(e1000_device_t* dev, uint8_t phy, uint8_t reg, uint16_t data) {
    uint32_t cmd = ((uint32_t)data & E1000_MDIC_DATA_MASK)
                 | ((uint32_t)reg << E1000_MDIC_REG_SHIFT)
                 | ((uint32_t)phy << E1000_MDIC_PHY_SHIFT)
                 | E1000_MDIC_OP_WRITE;
    e1000_write_reg(dev, E1000_REG_MDIC, cmd);
    uint32_t v = 0; if (!mdic_wait_ready(dev, &v)) return false;
    if (v & E1000_MDIC_ERROR) return false;
    netlog_write("[mdic] write phy="); netlog_write_hex("", phy); netlog_write(" reg="); netlog_write_hex("", reg); netlog_write(" data="); netlog_write_hex("", data); netlog_write(" result="); netlog_write_hex("", (uint32_t)(v & E1000_MDIC_DATA_MASK)); netlog_write("\n");
    return true;
}

static bool mdic_read(e1000_device_t* dev, uint8_t phy, uint8_t reg, uint16_t* data_out) {
    uint32_t cmd = ((uint32_t)reg << E1000_MDIC_REG_SHIFT)
                 | ((uint32_t)phy << E1000_MDIC_PHY_SHIFT)
                 | E1000_MDIC_OP_READ;
    e1000_write_reg(dev, E1000_REG_MDIC, cmd);
    uint32_t v = 0; if (!mdic_wait_ready(dev, &v)) return false;
    if (v & E1000_MDIC_ERROR) return false;
    if (data_out) *data_out = (uint16_t)(v & E1000_MDIC_DATA_MASK);
    // Duplicate / zero-value suppression to reduce log spam
    static uint8_t last_phy = 0xFF;
    static uint8_t last_reg = 0xFF;
    static uint16_t last_val = 0xFFFF;
    static uint32_t repeat_count = 0;
    uint16_t cur_val = (uint16_t)(v & E1000_MDIC_DATA_MASK);
    bool same_key = (phy == last_phy && reg == last_reg);
    bool same_val = (cur_val == last_val);
    if (same_key && same_val) {
        // Accumulate repeats; only log periodically
        repeat_count++;
        if ((repeat_count & 0x3F) == 0) { // every 64 repeats, emit a summary
            netlog_write("[mdic] repeat phy="); netlog_write_hex("", phy); netlog_write(" reg="); netlog_write_hex("", reg);
            netlog_write(" val="); netlog_write_hex("", (uint32_t)cur_val); netlog_write(" count="); netlog_write_hex("", repeat_count); netlog_write("\n");
        }
    } else {
        // If we suppressed previous duplicates, emit a closure summary first
        if (same_key && repeat_count > 1) {
            netlog_write("[mdic] repeats total phy="); netlog_write_hex("", phy); netlog_write(" reg="); netlog_write_hex("", reg);
            netlog_write(" val="); netlog_write_hex("", (uint32_t)last_val); netlog_write(" total="); netlog_write_hex("", repeat_count); netlog_write("\n");
        }
        // Log this fresh value (always log non-zero change; log first zero)
        netlog_write("[mdic] read phy="); netlog_write_hex("", phy); netlog_write(" reg="); netlog_write_hex("", reg);
        netlog_write(" val="); netlog_write_hex("", (uint32_t)cur_val); netlog_write(" err="); netlog_write_hex("", (v & E1000_MDIC_ERROR)?1:0); netlog_write("\n");
        last_phy = phy; last_reg = reg; last_val = cur_val; repeat_count = 1;
    }
    return true;
}

// Minimal copper autoneg advertisement (GPL-aligned approach inspired by Linux e1000e):
// - Advertise 10/100 HD/FD via MII ANAR (reg 4)
// - Advertise 1000 FD via 1000BASE-T Control (reg 9)
// - Restart autoneg in MII CTRL (reg 0)
static void e1000_configure_autoneg_advert_m88(e1000_device_t* dev, uint8_t phy_addr) {
    if (!dev || !dev->mem_base) return;
    // MII ANAR bits: selector(0..4)=IEEE 802.3 (0x1), 10HD(5), 10FD(6), 100HD(7), 100FD(8), pause(10), asym_pause(11)
    const uint16_t anar = (uint16_t)(0x0001 /* selector */
                                     | (1u<<5) /* 10HD */
                                     | (1u<<6) /* 10FD */
                                     | (1u<<7) /* 100HD */
                                     | (1u<<8) /* 100FD */
                                     | (1u<<10) /* pause */
                                     | (1u<<11) /* asym pause */);
    (void)mdic_write(dev, phy_addr, 4 /* ANAR */, anar);

    // 1000BASE-T control: advertise 1000FD (bit 9)
    const uint16_t gtcr = (uint16_t)(1u<<9);
    (void)mdic_write(dev, phy_addr, 9 /* 1000BASE-T Control */, gtcr);

    // Enable autoneg + restart in BMCR (reg 0): ANE (bit 12), RESTART (bit 9), set full duplex (bit 8)
    uint16_t bmcr = 0;
    if (!mdic_read(dev, phy_addr, 0 /* BMCR */, &bmcr)) {
        bmcr = 0;
    }
    bmcr = (uint16_t)(bmcr | (1u<<12) | (1u<<9) | (1u<<8));
    (void)mdic_write(dev, phy_addr, 0 /* BMCR */, bmcr);
}

// Wait for EEPROM auto-read completion after device reset
static void e1000_wait_autoread(e1000_device_t* dev) {
    // Prefer EECD.AR_DONE (legacy) then try EEC.AR_DONE (newer)
    const int max_spins = 100000;
    int spins = 0;
    for (; spins < max_spins; ++spins) {
        uint32_t eecd = e1000_read_reg(dev, E1000_REG_EECD);
        if (eecd & E1000_EEC_AR_DONE) break;
    }
    if (spins >= max_spins) {
        // Try alternate EEC register space
        spins = 0;
        for (; spins < max_spins; ++spins) {
            uint32_t eec = e1000_read_reg(dev, E1000_REG_EEC);
            if (eec & E1000_EEC_AR_DONE) break;
        }
        if (spins >= max_spins) {
            netlog_write("[init] WARN: EEPROM auto-read not observed (continuing)\n");
        } else {
            netlog_write("[init] EEC AR_DONE observed\n");
        }
    } else {
        netlog_write("[init] EECD AR_DONE observed\n");
    }
}

// Decode STATUS for logging
static void e1000_log_status(uint32_t status, const char* tag) {
    netlog_write("[status] "); netlog_write(tag); netlog_write(" raw="); netlog_write_hex("", status);
    uint32_t link_up = (status & 0x00000002) ? 1 : 0; // LU bit
    uint32_t fd = (status & 0x00000001) ? 1 : 0;      // FD bit
    uint32_t speed = 0; // approximate decoding
    if (status & 0x000000C0) speed = 1000; else if (status & 0x00000040) speed = 100; else speed = 10;
    netlog_write(" lu="); netlog_write_hex("", link_up);
    netlog_write(" fd="); netlog_write_hex("", fd);
    netlog_write(" sp="); netlog_write_hex("", speed);
    netlog_write("\n");
}

// PHY scan to find responding PHY address
static uint8_t e1000_scan_phy(e1000_device_t* dev) {
    uint16_t ctrl=0, sts=0; uint8_t found = 0xFF;
    for (uint8_t a = 0; a < 8; ++a) {
        bool okc = mdic_read(dev, a, 0, &ctrl);
        bool oks = mdic_read(dev, a, 1, &sts);
        netlog_write("[phy] addr="); netlog_write_hex("", a); netlog_write(" ctrl="); netlog_write_hex("", okc?ctrl:0); netlog_write(" sts="); netlog_write_hex("", oks?sts:0); netlog_write("\n");
        if (okc && oks && (ctrl != 0x0000 || sts != 0x0000)) {
            found = a; break;
        }
    }
    if (found == 0xFF) netlog_write("[phy] none-responsive; defaulting to 1\n");
    else { netlog_write("[phy] using addr="); netlog_write_hex("", found); netlog_write("\n"); }
    return (found == 0xFF) ? 1 : found;
}

// Ensure hardware sees all RX descriptors by re-advertising tail.
static inline void e1000_rx_advertise_all(e1000_device_t* dev) {
    if (!dev || !dev->mem_base) return;
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    (void)e1000_read_reg(dev, E1000_REG_STATUS);
}

// Public helpers to control/query loopback for quick diagnostics
void e1000_config_loopback(e1000_loopback_mode_t mode) {
    if (!e1000_dev || !e1000_dev->mem_base || e1000_dev->mmio_inert) {
        netlog_write("[lb] unavailable (no device/mmio)\n");
        return;
    }
    // Normalize mode values to valid encodings (off=0, mac=2, phy=3)
    uint32_t lb_bits = 0;
    if (mode == E1000_LB_MAC) lb_bits = E1000_RCTL_LBM_MAC;
    else if (mode == E1000_LB_PHY) lb_bits = E1000_RCTL_LBM_PHY;
    else lb_bits = E1000_RCTL_LBM_NONE;

    // Preserve common bits, clear loopback field, enforce EN|BAM|SECRC for tests
    uint32_t rctl = e1000_read_reg(e1000_dev, E1000_REG_RCTRL);
    uint32_t base = (rctl & ~(3u << 6));
    // During loopback/self-test, also enable UPE|MPE to avoid filter drops
    uint32_t new_rctl = base | lb_bits | E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC | E1000_RCTL_UPE | E1000_RCTL_MPE;
    e1000_write_reg(e1000_dev, E1000_REG_RCTRL, new_rctl);
    (void)e1000_read_reg(e1000_dev, E1000_REG_STATUS);
    netlog_write("[lb] rctl="); netlog_write_hex("", new_rctl); netlog_write("\n");
    // Resequence RX and re-advertise all descriptors so change takes effect
    e1000_rx_resequence(e1000_dev, "loopback-toggle");
    e1000_rx_advertise_all(e1000_dev);

    // Proactively transmit a small test frame to drive loopback paths,
    // even if link is reported down. This helps validate RX immediately.
    extern void e1000_send_test_broadcast(void);
    e1000_send_test_broadcast();
    // Immediately poll once to try to catch the looped frame fast
    e1000_poll_receive(e1000_dev);
}

int e1000_get_loopback_mode(void) {
    if (!e1000_dev || !e1000_dev->mem_base || e1000_dev->mmio_inert) return -1;
    uint32_t rctl = e1000_read_reg(e1000_dev, E1000_REG_RCTRL);
    return (int)((rctl >> 6) & 0x3u);
}

void e1000_phy_dump(void) {
    if (!e1000_dev || !e1000_dev->mem_base || e1000_dev->mmio_inert) {
        netlog_write("[phy-dump] unavailable (no device/mmio)\n");
        return;
    }
    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    gfx_print("PHY dump (addrs 0..7 regs 0..6,4/5):\n");
    for (uint8_t a = 0; a < 8; ++a) {
        uint16_t r0=0,r1=0,r2=0,r3=0,r4=0,r5=0,r6=0; bool ok=false;
        ok |= mdic_read(e1000_dev, a, 0, &r0);
        ok |= mdic_read(e1000_dev, a, 1, &r1);
        mdic_read(e1000_dev, a, 2, &r2);
        mdic_read(e1000_dev, a, 3, &r3);
        mdic_read(e1000_dev, a, 4, &r4); // ANAR
        mdic_read(e1000_dev, a, 5, &r5); // ANLPAR
        mdic_read(e1000_dev, a, 6, &r6);
        if (!ok) continue;
        gfx_print("  A="); gfx_print_hex(a);
        gfx_print(" CTRL="); gfx_print_hex(r0);
        gfx_print(" BMSR="); gfx_print_hex(r1);
        gfx_print(" ANAR="); gfx_print_hex(r4);
        gfx_print(" ANLPAR="); gfx_print_hex(r5);
        gfx_print("\n");
        netlog_write("[phy-dump] a="); netlog_write_hex("", a);
        netlog_write(" r0="); netlog_write_hex("", r0);
        netlog_write(" r1="); netlog_write_hex("", r1);
        netlog_write(" r4="); netlog_write_hex("", r4);
        netlog_write(" r5="); netlog_write_hex("", r5);
        netlog_write("\n");
    }
}

static void e1000_init_rx(e1000_device_t* dev) {
    extern void gfx_print(const char*);
    
    // Allocate receive descriptor ring (1 page is enough for 32 desc × 16B = 512B)
        void* rx_desc_va = dma_allocator_alloc(4096, 4096, 0);
    uint64_t rx_desc_phys = dma_allocator_get_phys(rx_desc_va);
    if (!rx_desc_va || rx_desc_phys == 0) {
        // Fallback to PMM
        rx_desc_phys = alloc_dma_page_below_4g();
    }
    // Prefer the allocator's VA if available; fallback to explicit DMA map
    if (rx_desc_va && rx_desc_phys) {
        dev->rx_descs = (e1000_rx_desc_t*)rx_desc_va;
    } else {
        dev->rx_descs = (e1000_rx_desc_t*)dma_kmap_page(rx_desc_phys);
    }
    dev->rx_desc_phys = rx_desc_phys;
    if (rx_desc_phys == 0 || dev->rx_descs == NULL) {
        netlog_write("[fatal] rx ring alloc/map failed\n");
        return;
    }
    memset(dev->rx_descs, 0, 4096);
    
    // Allocate receive buffers
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        void* v = dma_allocator_alloc(E1000_RX_BUF_SIZE, 4096, 0);
        uint64_t buf_phys = dma_allocator_get_phys(v);
        if (!v || buf_phys == 0) {
            // Fallback to PMM path
            buf_phys = alloc_dma_page_below_4g();
        }
        // Always use uncached DMA mapping for buffers to read device writes coherently
        v = dma_kmap_page(buf_phys);
        dev->rx_buffers[i] = (uint8_t*)v;
        dev->rx_buf_phys[i] = buf_phys;
        if (buf_phys == 0 || v == NULL) {
            netlog_write("[rxbuf-alloc-fail] i="); netlog_write_hex("", i); netlog_write("\n");
            rx_desc_set_addr(&dev->rx_descs[i], 0); // leave for repair path
        } else {
            rx_desc_set_addr(&dev->rx_descs[i], buf_phys); // program physical address for DMA
        }
        dev->rx_descs[i].status = 0;
        dev->rx_descs[i].errors = 0;
        dev->rx_descs[i].length = 0;
        dev->rx_descs[i].checksum = 0;
        dev->rx_descs[i].special = 0;
    }

    // Log mapping + test pattern for first 4 buffers
    for (int i = 0; i < 4 && i < E1000_NUM_RX_DESC; ++i) {
        uint64_t phys = ((uint64_t)((uint32_t*)(&dev->rx_descs[i]))[1] << 32) | ((uint64_t)((uint32_t*)(&dev->rx_descs[i]))[0]);
        uint8_t* virt = dev->rx_buffers[i];
        uint8_t r0=0, r1=0, r2=0, r3=0; int ok = 0;
        if (virt) {
            // Write pattern
            virt[0] = 0xA0 + (uint8_t)i; virt[1] = 0x5A; virt[2] = 0xC3; virt[3] = 0x3C;
            r0 = virt[0]; r1 = virt[1]; r2 = virt[2]; r3 = virt[3];
            ok = (r0 == (0xA0 + (uint8_t)i) && r1 == 0x5A && r2 == 0xC3 && r3 == 0x3C);
        }
        netlog_write("[rxbuf-map] i="); netlog_write_hex("", i);
        netlog_write(" phys="); netlog_write_hex("", (uint32_t)phys);
        netlog_write(" virt="); netlog_write_hex("", (uint32_t)(uintptr_t)virt);
        netlog_write(" patt="); netlog_write_hex("", r0); netlog_write(" "); netlog_write_hex("", r1); netlog_write(" "); netlog_write_hex("", r2); netlog_write(" "); netlog_write_hex("", r3);
        netlog_write(" ok="); netlog_write_hex("", ok); netlog_write("\n");
    }
    
    // Setup the receive descriptor ring BEFORE enabling
    e1000_write_reg(dev, E1000_REG_RXDESCLO, (uint32_t)dev->rx_desc_phys);
    e1000_write_reg(dev, E1000_REG_RXDESCHI, (uint32_t)(dev->rx_desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write_reg(dev, E1000_REG_RXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    
    dev->rx_current = 0;
    
    // Set RX delay timer to 0 for immediate writeback
    e1000_write_reg(dev, E1000_REG_RDTR, 0);
    
    // Only program ring buffers here; defer enabling RX until full device init
    gfx_print("E1000: RX ring programmed (deferred enable)\n");
}

static void e1000_init_tx(e1000_device_t* dev) {
    extern void gfx_print(const char*);
    // Allocate transmit descriptor ring
        void* tx_desc_va = dma_allocator_alloc(4096, 4096, 0);
    uint64_t tx_desc_phys = dma_allocator_get_phys(tx_desc_va);
    if (!tx_desc_va || tx_desc_phys == 0) {
        tx_desc_phys = alloc_dma_page_below_4g();
    }
    // Prefer the allocator's VA if available; fallback to explicit DMA map
    if (tx_desc_va && tx_desc_phys) {
        dev->tx_descs = (e1000_tx_desc_t*)tx_desc_va;
    } else {
        dev->tx_descs = (e1000_tx_desc_t*)dma_kmap_page(tx_desc_phys);
    }
    dev->tx_desc_phys = tx_desc_phys;
    memset(dev->tx_descs, 0, 4096);
    
    // Allocate transmit buffers
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        void* v = dma_allocator_alloc(E1000_TX_BUF_SIZE, 4096, 0);
        uint64_t buf_phys = dma_allocator_get_phys(v);
        if (!v || buf_phys == 0) {
            buf_phys = alloc_dma_page_below_4g();
        }
        // Use uncached mapping to ensure device sees CPU writes promptly
        v = dma_kmap_page(buf_phys);
        dev->tx_buffers[i] = (uint8_t*)v;
        dev->tx_buf_phys[i] = buf_phys;
        tx_desc_set_addr(&dev->tx_descs[i], buf_phys);
        dev->tx_descs[i].status = E1000_TXD_STAT_DD;
        dev->tx_descs[i].cmd = 0;
    }
    
    // Setup the transmit descriptor ring
    e1000_write_reg(dev, E1000_REG_TXDESCLO, (uint32_t)dev->tx_desc_phys);
    e1000_write_reg(dev, E1000_REG_TXDESCHI, (uint32_t)(dev->tx_desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write_reg(dev, E1000_REG_TXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_TXDESCTAIL, 0);
    
    dev->tx_current = 0;
    
    // Defer enabling TX (TCTRL) until full device init; only program ring
    gfx_print("E1000: TX ring programmed (deferred enable)\n");
}

int e1000_send_packet(net_device_t* netdev, net_packet_t* packet) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    
    LOG_TS("[TX: start]\n"); netlog_write("[tx] start\n");
    gfx_print("[TX: start]");
    
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
    if (!dev || !dev->mem_base || !dev->tx_descs || !packet) {
        serial_debug("[TX: null]\n");
        gfx_print("[TX: null]");
        return -1;
    }
    
    if (!dev->tx_buffers) {
        serial_debug("[TX: no bufs]\n");
        gfx_print("[TX: no bufs]");
        return -1;
    }
    
    LOG_TS("[TX: wait]\n"); netlog_write("[tx] wait\n");
    gfx_print("[TX: wait]");
    
    // Get current TX descriptor
    e1000_tx_desc_t* desc = &dev->tx_descs[dev->tx_current];
    
    // Descriptor availability logic:
    // Hardware sets DD when transmission of a previously queued descriptor completes.
    // At init descriptors are zeroed then marked DD to indicate free. If DD is clear
    // and we have already queued this descriptor (cmd != 0), we must wait.
    if (!(desc->status & E1000_TXD_STAT_DD) && desc->cmd != 0) {
        // Busy descriptor previously queued and not yet completed
        uint32_t tdh_dbg = e1000_read_reg(dev, E1000_REG_TXDESCHEAD);
        uint32_t tdt_dbg = e1000_read_reg(dev, E1000_REG_TXDESCTAIL);
        netlog_write("[tx] busy cur="); netlog_write_hex("", dev->tx_current);
        netlog_write(" TDH="); netlog_write_hex("", tdh_dbg);
        netlog_write(" TDT="); netlog_write_hex("", tdt_dbg);
        netlog_write(" st="); netlog_write_hex("", desc->status);
        netlog_write(" cmd="); netlog_write_hex("", desc->cmd);
        netlog_write("\n");
        // Increment busy streak and try recovery if threshold exceeded
        dev->tx_busy_streak++;
        if (dev->tx_busy_streak >= 64) {
            // Recovery helper: toggle TCTL, re-post ring registers, and re-bump TDT
            uint32_t tctl_before = e1000_read_reg(dev, E1000_REG_TCTRL);
            uint32_t txdctl_before = e1000_read_reg(dev, E1000_REG_TXDCTL);
            netlog_write("[tx-recover] start streak="); netlog_write_hex("", dev->tx_busy_streak);
            netlog_write(" TDH="); netlog_write_hex("", tdh_dbg);
            netlog_write(" TDT="); netlog_write_hex("", tdt_dbg);
            netlog_write(" TCTL="); netlog_write_hex("", tctl_before);
            netlog_write(" TXDCTL="); netlog_write_hex("", txdctl_before);
            netlog_write("\n");
            // Disable then re-enable TX
            e1000_write_reg(dev, E1000_REG_TCTRL, tctl_before & ~E1000_TCTL_EN);
            (void)e1000_read_reg(dev, E1000_REG_STATUS);
            // Re-assert descriptor base/len (belt-and-suspenders)
            e1000_write_reg(dev, E1000_REG_TXDESCLO, (uint32_t)dev->tx_desc_phys);
            e1000_write_reg(dev, E1000_REG_TXDESCHI, (uint32_t)(dev->tx_desc_phys >> 32));
            e1000_write_reg(dev, E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
            // Re-enable TX and thresholds
            e1000_write_reg(dev, E1000_REG_TCTRL, tctl_before | E1000_TCTL_EN);
            uint32_t txdctl_cfg = (8 & 0x3F) | ((8 & 0x3F) << 8) | ((4 & 0x3F) << 16);
            e1000_write_reg(dev, E1000_REG_TXDCTL, txdctl_cfg);
            // Poke tail again (no-op if unchanged, harmless)
            e1000_write_reg(dev, E1000_REG_TXDESCTAIL, dev->tx_current);
            // Snapshot after
            uint32_t tdh_after2 = e1000_read_reg(dev, E1000_REG_TXDESCHEAD);
            uint32_t tdt_after2 = e1000_read_reg(dev, E1000_REG_TXDESCTAIL);
            uint32_t tctl_after = e1000_read_reg(dev, E1000_REG_TCTRL);
            netlog_write("[tx-recover] done TDH="); netlog_write_hex("", tdh_after2);
            netlog_write(" TDT="); netlog_write_hex("", tdt_after2);
            netlog_write(" TCTL="); netlog_write_hex("", tctl_after);
            netlog_write("\n");
            dev->tx_recover_count++;
            dev->tx_busy_streak = 0;
        }
        return -1;
    }
    
    LOG_TS("[TX: copy]\n"); netlog_write("[tx] copy\n");
    gfx_print("[TX: copy]");
    
    // Copy packet data to TX buffer (limit to TX buf size)
    uint8_t* tx_buf = dev->tx_buffers[dev->tx_current];
    if (!tx_buf) {
        LOG_TS("[TX: no buf]\n");
        gfx_print("[TX: no buf]");
        return -1;
    }
    
    uint32_t copy_len = packet->length;
    if (copy_len > E1000_TX_BUF_SIZE) copy_len = E1000_TX_BUF_SIZE;
    for (uint32_t i = 0; i < copy_len; i++) {
        tx_buf[i] = packet->data[i];
    }

    // Instrumentation: if this is an ARP frame, dump first 64 bytes and descriptor meta
    {
        extern void netlog_write(const char*);
        extern void netlog_write_hex(const char*, uint32_t);
        // Ethernet header is at packet->data; ethertype at bytes 12-13 (big-endian)
        if (copy_len >= 14) {
            uint16_t et = ((uint16_t)packet->data[12] << 8) | (uint16_t)packet->data[13];
            if (et == 0x0806) {
                netlog_write("[arp-tx-dump] len="); netlog_write_hex("", copy_len); netlog_write(" desc_cur="); netlog_write_hex("", dev->tx_current); netlog_write("\n");
                netlog_write(" dst="); for (int i=0;i<6;i++){ netlog_write_hex("", packet->data[i]); }
                netlog_write(" src="); for (int i=6;i<12;i++){ netlog_write_hex("", packet->data[i]); }
                netlog_write(" et="); netlog_write_hex("", et); netlog_write("\n");
                netlog_write(" payload[0..31]=");
                int dump_n = copy_len < 46 ? copy_len : 46; // up to header+32
                for (int i=14;i<14+dump_n && i<64;i++){ netlog_write_hex("", packet->data[i]); }
                netlog_write("\n");
            }
        }
    }
    
    LOG_TS("[TX: setup]\n"); netlog_write("[tx] setup\n");
    gfx_print("[TX: setup]");
    
    // Setup descriptor
    desc->length = copy_len;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0; // Clear DD to hand to hardware

    // If ARP, log descriptor fields
    {
        extern void netlog_write(const char*);
        extern void netlog_write_hex(const char*, uint32_t);
        if (copy_len >= 14) {
            uint16_t et = ((uint16_t)packet->data[12] << 8) | (uint16_t)packet->data[13];
            if (et == 0x0806) {
                uint64_t buf_phys = dev->tx_buf_phys[dev->tx_current];
                netlog_write("[arp-tx-desc] phys_lo="); netlog_write_hex("", (uint32_t)(buf_phys & 0xFFFFFFFF));
                netlog_write(" phys_hi="); netlog_write_hex("", (uint32_t)(buf_phys >> 32));
                netlog_write(" len="); netlog_write_hex("", desc->length);
                netlog_write(" cmd="); netlog_write_hex("", desc->cmd);
                netlog_write(" st="); netlog_write_hex("", desc->status);
                netlog_write("\n");
            }
        }
    }
    
    // Notify hardware for the descriptor we just filled.
    // For E1000, TDT should point to one past the last descriptor to transmit.
    uint16_t next = (uint16_t)((dev->tx_current + 1) % E1000_NUM_TX_DESC);
    LOG_TS("[TX: notify]\n"); netlog_write("[tx] notify tail(next)="); netlog_write_hex("", next); netlog_write("\n");
    gfx_print("[TX: notify]");
    uint32_t tdh_before = e1000_read_reg(dev, E1000_REG_TXDESCHEAD);
    uint32_t tdt_before = e1000_read_reg(dev, E1000_REG_TXDESCTAIL);
    e1000_write_reg(dev, E1000_REG_TXDESCTAIL, next);
    uint32_t tdh_after = e1000_read_reg(dev, E1000_REG_TXDESCHEAD);
    uint32_t tdt_after = e1000_read_reg(dev, E1000_REG_TXDESCTAIL);
    netlog_write("[tx] tail advance cur="); netlog_write_hex("", dev->tx_current);
    netlog_write(" TDH(before)="); netlog_write_hex("", tdh_before);
    netlog_write(" TDT(before)="); netlog_write_hex("", tdt_before);
    netlog_write(" TDH(after)="); netlog_write_hex("", tdh_after);
    netlog_write(" TDT(after)="); netlog_write_hex("", tdt_after);
    netlog_write("\n");

    // Move to next descriptor (the one after the last we just queued)
    dev->tx_current = next;
    // Reset busy streak on successful queueing
    dev->tx_busy_streak = 0;
    
    LOG_TS("[TX: done]\n"); netlog_write("[tx] done cur="); netlog_write_hex("", dev->tx_current); netlog_write("\n");
    gfx_print("[TX: done]");
    
    return 0;
}

static inline bool mac_is_zero(const mac_addr_t* mac) {
    return mac->addr[0]==0 && mac->addr[1]==0 && mac->addr[2]==0 && mac->addr[3]==0 && mac->addr[4]==0 && mac->addr[5]==0;
}

int e1000_init_device(net_device_t* netdev) {
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
        extern void gfx_print(const char*);
    gfx_print("E1000: Bringing device up...\n"); netlog_write("[init] start\n");
    netlog_write("[init] pci bus="); netlog_write_hex("", e1000_dev->pci_bus); netlog_write(" slot="); netlog_write_hex("", e1000_dev->pci_slot); netlog_write(" func="); netlog_write_hex("", e1000_dev->pci_func); netlog_write("\n");
    // MMIO mapping sanity
    netlog_write("[init] mmio base="); netlog_write_hex("", (uint32_t)dev->mem_base); netlog_write(" mapped="); netlog_write_hex("", dev->mmio_mapped ? 1 : 0); netlog_write("\n");
    
    // Ensure PCI Memory Space and Bus Master are enabled (safety after reset)
    uint8_t bus = e1000_dev->pci_bus, slot = e1000_dev->pci_slot, func = e1000_dev->pci_func;
    uint16_t pcicmd = pci_config_read_word(bus, slot, func, 0x04);
    pcicmd |= 0x0006; // Memory Space Enable | Bus Master Enable
    pci_config_write_word(bus, slot, func, 0x04, pcicmd);
    // Read back and log command register persistence
    uint16_t pcicmd_chk = pci_config_read_word(bus, slot, func, 0x04);
    netlog_write("[init] pci cmd after set="); netlog_write_hex("", pcicmd_chk); netlog_write("\n");

    // 1) Reset
    uint32_t ctrl_before = e1000_read_reg(dev, E1000_REG_CTRL);
    e1000_write_reg(dev, E1000_REG_CTRL, ctrl_before | E1000_CTRL_RST);
    for (volatile int i = 0; i < 400000; ++i) { __asm__ __volatile__("pause"); }
    // Wait EEPROM auto-read done to ensure internal config has settled
    e1000_wait_autoread(dev);
    // Clear interrupts and allow internal init to finish
    uint32_t icr0 = e1000_read_reg(dev, E1000_REG_ICR);
    uint32_t status0 = e1000_read_reg(dev, E1000_REG_STATUS);
    netlog_write("[init] post-reset ICR="); netlog_write_hex("", icr0); netlog_write(" STATUS="); netlog_write_hex("", status0); netlog_write("\n");
    e1000_log_status(status0, "reset");
    // MMIO write probe on a known RW register (IMASK)
    e1000_write_reg(dev, E1000_REG_IMASK, 0x00000000);
    uint32_t im0 = e1000_read_reg(dev, E1000_REG_IMASK);
    e1000_write_reg(dev, E1000_REG_IMASK, 0xFFFFFFFF);
    uint32_t im1 = e1000_read_reg(dev, E1000_REG_IMASK);
    netlog_write("[init] imask rw probe 0->"); netlog_write_hex("", im0); netlog_write(" -> "); netlog_write_hex("", im1); netlog_write("\n");
    // Re-assert PCI command bits (Bus Master + Memory) in case reset cleared them
    uint16_t pcicmd_after = pci_config_read_word(bus, slot, func, 0x04);
    if ((pcicmd_after & 0x0006) != 0x0006) {
        pcicmd_after |= 0x0006;
        pci_config_write_word(bus, slot, func, 0x04, pcicmd_after);
        netlog_write("[init] re-enable PCI cmd="); netlog_write_hex("", pcicmd_after); netlog_write("\n");
    }

    // Determine MAC from RAL/RAH or EEPROM; fallback if still zero
    e1000_read_mac_address(dev);
    e1000_program_mac_address(dev);
    if (mac_is_zero(&dev->net_dev.mac_address)) {
        for (volatile int d = 0; d < 200000; ++d) {}
        e1000_read_mac_address(dev);
        e1000_program_mac_address(dev);
    }
    if (mac_is_zero(&dev->net_dev.mac_address)) {
        dev->net_dev.mac_address.addr[0] = 0x52;
        dev->net_dev.mac_address.addr[1] = 0x54;
        dev->net_dev.mac_address.addr[2] = 0x00;
        dev->net_dev.mac_address.addr[3] = 0x12;
        dev->net_dev.mac_address.addr[4] = 0x34;
        dev->net_dev.mac_address.addr[5] = 0x56;
        netlog_write("[init] MAC fallback to 52:54:00:12:34:56\n");
    }
    // Program receive address 0 with our MAC and set valid bit
    uint32_t ral = (uint32_t)dev->net_dev.mac_address.addr[0]
                 | ((uint32_t)dev->net_dev.mac_address.addr[1] << 8)
                 | ((uint32_t)dev->net_dev.mac_address.addr[2] << 16)
                 | ((uint32_t)dev->net_dev.mac_address.addr[3] << 24);
    uint32_t rah = (uint32_t)dev->net_dev.mac_address.addr[4]
                 | ((uint32_t)dev->net_dev.mac_address.addr[5] << 8)
                 | E1000_RAH_AV;
    e1000_write_reg(dev, E1000_REG_RAL0, ral);
    e1000_write_reg(dev, E1000_REG_RAH0, rah);
    // Read back to confirm MMIO write stuck
    uint32_t ral_rb = e1000_read_reg(dev, E1000_REG_RAL0);
    uint32_t rah_rb = e1000_read_reg(dev, E1000_REG_RAH0);
    netlog_write("[init] MAC RAL0="); netlog_write_hex("", ral_rb); netlog_write(" RAH0="); netlog_write_hex("", rah_rb); netlog_write("\n");

    // 2) Clear multicast table array (filters)
    for (int i = 0; i < 128; ++i) e1000_write_reg(dev, E1000_REG_MTA + i*4, 0);

    // After a device reset all queue registers are cleared; we must reprogram RX/TX rings now.
    // Ring register pattern test before allocation: write a temp length then read back.
    e1000_write_reg(dev, E1000_REG_RXDESCLEN, 0x00001000);
    uint32_t rdlen_test = e1000_read_reg(dev, E1000_REG_RXDESCLEN);
    netlog_write("[init] program rings rdlen_test="); netlog_write_hex("", rdlen_test); netlog_write("\n");
    if (rdlen_test == 0) {
        netlog_write("[fatal] ring length register inert; abort ring setup\n");
        return 0;
    }

    //   wait for eeprom auto-read done (if present)



    //

    // Use rings allocated during detect-phase init (e1000_init_rx/tx)
    if (!dev->rx_descs || !dev->tx_descs || dev->rx_desc_phys == 0 || dev->tx_desc_phys == 0) {
        netlog_write("[fatal] rings not allocated prior to init_device\n");
        return 0;
    }
    netlog_write("[ring] phys rx="); netlog_write_hex("", (uint32_t)dev->rx_desc_phys); netlog_write(" tx="); netlog_write_hex("", (uint32_t)dev->tx_desc_phys); netlog_write("\n");

    // --- RX ring program (minimal sequence) ---
    // RCTL without EN while programming
    // Include SBP during bring-up to accept frames the MAC may flag bad (useful with loopback/tests)
    uint32_t rctl_base = E1000_RCTL_LBM_NONE | E1000_RCTL_BAM | E1000_RCTL_SBP | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC;
    e1000_write_reg(dev, E1000_REG_RCTRL, rctl_base);
    (void)e1000_read_reg(dev, E1000_REG_STATUS);

    // 3) RX ring base / length / head / tail (post buffers via tail=N-1)
    e1000_write_reg(dev, E1000_REG_RXDESCLO, (uint32_t)dev->rx_desc_phys);
    e1000_write_reg(dev, E1000_REG_RXDESCHI, (uint32_t)(dev->rx_desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write_reg(dev, E1000_REG_RXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    // Read back all ring programming registers for verification
    uint32_t rdlo_dbg = e1000_read_reg(dev, E1000_REG_RXDESCLO);
    uint32_t rdhi_dbg = e1000_read_reg(dev, E1000_REG_RXDESCHI);
    uint32_t rdlen_dbg = e1000_read_reg(dev, E1000_REG_RXDESCLEN);
    uint32_t rdt_after_post = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
    netlog_write("[ring] rx post RDLO="); netlog_write_hex("", rdlo_dbg);
    netlog_write(" RDHI="); netlog_write_hex("", rdhi_dbg);
    netlog_write(" RDLEN="); netlog_write_hex("", rdlen_dbg);
    netlog_write(" RDT="); netlog_write_hex("", rdt_after_post); netlog_write("\n");
    if (rdlo_dbg == 0 || rdlen_dbg == 0 || rdt_after_post == 0) {
        netlog_write("[fatal] RX ring write failed; abort enabling\n");
        return 0;
    }

    // Ensure descriptor address fields are populated from stored phys addresses
    for (int di=0; di < E1000_NUM_RX_DESC; ++di) {
        uint32_t* wd = (uint32_t*)&dev->rx_descs[di];
        uint64_t a64 = ((uint64_t)wd[1] << 32) | wd[0];
        if (a64 == 0 && dev->rx_buf_phys[di] != 0) {
            rx_desc_set_addr(&dev->rx_descs[di], dev->rx_buf_phys[di]);
        }
    }

    // 4) RXDCTL thresholds (small = 1) before enabling
    uint32_t rxdctl_cfg = (1 << E1000_RXDCTL_PTHRESH_SHIFT) |
                          (1 << E1000_RXDCTL_HTHRESH_SHIFT) |
                          (1 << E1000_RXDCTL_WTHRESH_SHIFT);
    e1000_write_reg(dev, E1000_REG_RXDCTL, rxdctl_cfg);
    (void)e1000_read_reg(dev, E1000_REG_STATUS);

    // 5) Enable RX (add debug promiscuous bits UPE/MPE for bring-up)
    // Add Broadcast Accept Mode (BAM) to ensure broadcast ARP frames are received
    // Keep SBP during bring-up to ensure loopback/self-test traffic is not dropped
    uint32_t rctl_final = rctl_base | E1000_RCTL_EN | E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BAM | E1000_RCTL_SBP;
    e1000_write_reg(dev, E1000_REG_RCTRL, rctl_final);
    uint32_t rdh0 = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
    uint32_t rdt0 = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
    netlog_write("[ring] rx enable RCTL="); netlog_write_hex("", rctl_final); netlog_write(" RDH="); netlog_write_hex("", rdh0); netlog_write(" RDT="); netlog_write_hex("", rdt0); netlog_write("\n");
    // Forceful resequence to guarantee descriptors are visible to hardware
    e1000_rx_resequence(dev, "post-enable");
    uint32_t rdt1 = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
    netlog_write("[ring] rx tail re-adv RDT="); netlog_write_hex("", rdt1); netlog_write("\n");
    // Log first few RX descriptor addresses for DMA sanity
    for (int di=0; di<4; ++di) {
        uint64_t a = dev->rx_descs[di].addr;
        netlog_write("[rxdesc] i="); netlog_write_hex("", di); netlog_write(" addr_lo="); netlog_write_hex("", (uint32_t)(a & 0xFFFFFFFFULL)); netlog_write(" addr_hi="); netlog_write_hex("", (uint32_t)(a >> 32)); netlog_write("\n");
    }
    // Verify all RX descriptor addresses are non-zero; reallocate if needed
    int rx_fixups = 0;
    for (int di=0; di < E1000_NUM_RX_DESC; ++di) {
        uint32_t* wd = (uint32_t*)&dev->rx_descs[di];
        uint64_t addr64 = ((uint64_t)wd[1] << 32) | wd[0];
        if (addr64 == 0) {
            // Try DMA allocator first
            void* v = dma_allocator_alloc(E1000_RX_BUF_SIZE, 4096, 0);
            uint64_t buf_phys = dma_allocator_get_phys(v);
            if (buf_phys == 0) {
                buf_phys = alloc_dma_page_below_4g();
            }
            if (buf_phys != 0) {
                dev->rx_buffers[di] = (uint8_t*)dma_kmap_page(buf_phys);
                rx_desc_set_addr(&dev->rx_descs[di], buf_phys);
                rx_fixups++;
            }
        }
    }
    if (rx_fixups) {
        netlog_write("[rxfix] repaired zero addr desc count="); netlog_write_hex("", rx_fixups); netlog_write("\n");
        // Re-advertise tail after repairs
        e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
        (void)e1000_read_reg(dev, E1000_REG_STATUS);
    }
    // Dump full RX descriptor table (addr/status/length) once at init for validation
    for (int di=0; di < E1000_NUM_RX_DESC; ++di) {
        uint64_t a = dev->rx_descs[di].addr;
        netlog_write("[rxdesc-all] i="); netlog_write_hex("", di);
        netlog_write(" addr_lo="); netlog_write_hex("", (uint32_t)(a & 0xFFFFFFFFULL));
        netlog_write(" addr_hi="); netlog_write_hex("", (uint32_t)(a >> 32));
        netlog_write(" st="); netlog_write_hex("", dev->rx_descs[di].status);
        netlog_write(" l="); netlog_write_hex("", dev->rx_descs[di].length);
        netlog_write("\n");
    }
    // Simple test pattern on first RX buffer to confirm writable mapping
    if (dev->rx_buffers[0]) {
        dev->rx_buffers[0][0] = 0xA5; dev->rx_buffers[0][1] = 0x5A;
        uint8_t v0 = dev->rx_buffers[0][0]; uint8_t v1 = dev->rx_buffers[0][1];
        netlog_write("[rxbuf-test] b0="); netlog_write_hex("", v0); netlog_write(" b1="); netlog_write_hex("", v1); netlog_write("\n");
    }

    // 6) TX ring reprogram (minimal sequence)
    e1000_write_reg(dev, E1000_REG_TXDESCLO, (uint32_t)dev->tx_desc_phys);
    e1000_write_reg(dev, E1000_REG_TXDESCHI, (uint32_t)(dev->tx_desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write_reg(dev, E1000_REG_TXDESCHEAD, 0);
    e1000_write_reg(dev, E1000_REG_TXDESCTAIL, 0);
    e1000_write_reg(dev, E1000_REG_TIPG, 0x0060200A);
    e1000_write_reg(dev, E1000_REG_TCTRL, E1000_TCTL_EN | E1000_TCTL_PSP | (15 << E1000_TCTL_CT_SHIFT) | (64 << E1000_TCTL_COLD_SHIFT));
    uint32_t txdctl_cfg = (8 & 0x3F) | ((8 & 0x3F) << 8) | ((4 & 0x3F) << 16);
    e1000_write_reg(dev, E1000_REG_TXDCTL, txdctl_cfg);
    uint32_t tdh0 = e1000_read_reg(dev, E1000_REG_TXDESCHEAD);
    uint32_t tdt0 = e1000_read_reg(dev, E1000_REG_TXDESCTAIL);
    netlog_write("[ring] tx enable TDH="); netlog_write_hex("", tdh0); netlog_write(" TDT="); netlog_write_hex("", tdt0); netlog_write("\n");
    if (e1000_read_reg(dev, E1000_REG_TXDESCLO) == 0 || e1000_read_reg(dev, E1000_REG_TXDESCLEN) == 0) {
        netlog_write("[fatal] TX ring write failed; abort enabling\n");
        return 0;
    }
    // Repair TX descriptor addresses if any are zero (shouldn't be, but guard)
    int tx_fixups = 0;
    for (int di=0; di < E1000_NUM_TX_DESC; ++di) {
        uint32_t* wd = (uint32_t*)&dev->tx_descs[di];
        uint64_t addr64 = ((uint64_t)wd[1] << 32) | wd[0];
        if (addr64 == 0) {
            void* v = dma_allocator_alloc(E1000_TX_BUF_SIZE, 4096, 0);
            uint64_t buf_phys = dma_allocator_get_phys(v);
            if (buf_phys == 0) {
                buf_phys = alloc_dma_page_below_4g();
            }
            if (buf_phys != 0) {
                dev->tx_buffers[di] = (uint8_t*)dma_kmap_page(buf_phys);
                tx_desc_set_addr(&dev->tx_descs[di], buf_phys);
                dev->tx_descs[di].status = E1000_TXD_STAT_DD;
                tx_fixups++;
            }
        }
    }
    if (tx_fixups) {
        netlog_write("[txfix] repaired zero addr desc count="); netlog_write_hex("", tx_fixups); netlog_write("\n");
    }

    // 7) PHY scan and link setup
    uint8_t phy_addr = e1000_scan_phy(dev);
    // 7a) Program autoneg advertisement (10/100/1000 FD/HD + pause) and restart autoneg
    e1000_configure_autoneg_advert_m88(dev, phy_addr);
    // 7b) Link up: ASDE + SLU (FD retained)
    uint32_t ctrl = e1000_read_reg(dev, E1000_REG_CTRL);
    ctrl |= (E1000_CTRL_ASDE | E1000_CTRL_SLU | E1000_CTRL_FD);
    e1000_write_reg(dev, E1000_REG_CTRL, ctrl);
    netlog_write("[link] ctrl set="); netlog_write_hex("", ctrl); netlog_write("\n");
    uint16_t phy_sts = 0;
    uint16_t phy_ctrl = 0;
    (void)mdic_read(dev, phy_addr, 0, &phy_ctrl);
    // Poll PHY status for link (bit 2) and autoneg complete (bit 5)
    int phy_tries = 0; uint16_t phy_sts_last = 0; uint16_t phy_sts_live = 0;
    while (phy_tries++ < 256) { // extended retries
        // Double-read BMSR (reg 1) to get live (non-latched) link bit per IEEE spec
        if (mdic_read(dev, phy_addr, 1, &phy_sts)) {
            phy_sts_last = phy_sts; // latched snapshot
            // Second read for live status
            if (mdic_read(dev, phy_addr, 1, &phy_sts_live)) {
                // Use second value for link/autoneg bits
                if ((phy_sts_live & (1<<2)) && (phy_sts_live & (1<<5))) break;
            }
        }
    }
    netlog_write("[link] phy sts final(latched)="); netlog_write_hex("", phy_sts_last);
    netlog_write(" live="); netlog_write_hex("", phy_sts_live);
    netlog_write(" tries="); netlog_write_hex("", phy_tries); netlog_write("\n");
    if (!((phy_sts_live & (1<<2)) && (phy_sts_live & (1<<5)))) {
        // Forced PHY reset fallback before giving up
        uint32_t ctrl_before_force = e1000_read_reg(dev, E1000_REG_CTRL);
        e1000_write_reg(dev, E1000_REG_CTRL, ctrl_before_force | E1000_CTRL_PHY_RST);
        for (volatile int r=0; r < 200000; ++r) { __asm__ __volatile__("pause"); }
        // Ensure PHY_RST bit is deasserted (some emulations may not auto-clear)
        uint32_t ctrl_after_force = e1000_read_reg(dev, E1000_REG_CTRL);
        netlog_write("[link] forced phy reset ctrl="); netlog_write_hex("", ctrl_after_force); netlog_write("\n");
        // Explicitly clear PHY_RST and reassert ASDE|SLU|FD
        uint32_t ctrl_clear_rst = (ctrl_after_force & ~E1000_CTRL_PHY_RST) | E1000_CTRL_ASDE | E1000_CTRL_SLU | E1000_CTRL_FD;
        e1000_write_reg(dev, E1000_REG_CTRL, ctrl_clear_rst);
        (void)e1000_read_reg(dev, E1000_REG_STATUS);
        // Re-scan a few quick times post reset
        for (int f=0; f<32; ++f) {
            uint16_t lat=0, live=0;
            if (mdic_read(dev, phy_addr, 1, &lat) && mdic_read(dev, phy_addr, 1, &live)) {
                if ((live & (1<<2)) && (live & (1<<5))) { phy_sts_live = live; phy_sts_last = lat; break; }
            }
        }
        // Reassert CTRL bits and restart autoneg if needed
        uint32_t ctrl_retry = e1000_read_reg(dev, E1000_REG_CTRL);
        // Mask out PHY_RST in case it remains set, then assert link-related bits
        ctrl_retry = (ctrl_retry & ~E1000_CTRL_PHY_RST) | (E1000_CTRL_ASDE | E1000_CTRL_SLU | E1000_CTRL_FD);
        e1000_write_reg(dev, E1000_REG_CTRL, ctrl_retry);
        netlog_write("[link] ctrl reassert (link still down) ctrl="); netlog_write_hex("", ctrl_retry); netlog_write("\n");
        // Kick autoneg restart once more
        uint16_t phy_ctrl2 = 0;
        if (mdic_read(dev, phy_addr, 0, &phy_ctrl2)) {
            uint16_t new_ctrl2 = (uint16_t)(phy_ctrl2 | (1<<12) | (1<<9) | (1<<8));
            (void)mdic_write(dev, phy_addr, 0, new_ctrl2);
            netlog_write("[link] phy ctrl re-write new="); netlog_write_hex("", new_ctrl2); netlog_write("\n");
        }

        // As a last resort, force 100Mbps full-duplex (disable autoneg, set speed+FD)
        // BMCR bits: bit12 AN enable (clear), bit8 FD (set), bit6 speed LSB (set=100Mbps)
        uint16_t bmcr_force_100fd = (uint16_t)((1u<<8) | (1u<<6));
        (void)mdic_write(dev, phy_addr, 0, bmcr_force_100fd);
        netlog_write("[link] forcing BMCR=100FD write="); netlog_write_hex("", bmcr_force_100fd); netlog_write("\n");
        for (volatile int r2=0; r2 < 200000; ++r2) { __asm__ __volatile__("pause"); }
    }

    // Poll link status with intermediate logging
    uint32_t status = 0;
    bool link_up = false;
    for (int i = 0; i < 200000; ++i) {
        status = e1000_read_reg(dev, E1000_REG_STATUS);
        if ((i % 40000) == 0) {
            netlog_write("[link] poll status="); netlog_write_hex("", status); netlog_write(" i="); netlog_write_hex("", i); netlog_write("\n");
            e1000_log_status(status, "poll");
        }
        if (status & 0x02) { link_up = true; break; }
    }
    if (link_up) { gfx_print("E1000: Link is UP\n"); netlog_write("[link] UP status="); netlog_write_hex("", status); netlog_write("\n"); }
    else { gfx_print("E1000: Link is DOWN (continuing)\n"); netlog_write("[link] DOWN status="); netlog_write_hex("", status); netlog_write("\n"); }
    // Optional TX test frame (broadcast) to stimulate path
    if (link_up) {
        netlog_write("[txtest] sending broadcast test frame\n");
        e1000_send_test_broadcast();
    } else {
        netlog_write("[txtest] skipped (link down)\n");
    }
    
    // Cache initial link state & status for ongoing polling
    dev->link_up = link_up;
    dev->last_status = status;
    dev->link_retry_count = 0;

    // Enable RX polling now that link sequence finished
    g_e1000_poll_enabled = true;
    netlog_write("[rx] polling enabled\n");
    // Initial poll snapshot
    e1000_poll_receive(dev);

    // Final init message reflects actual link state
    gfx_print(link_up ? "E1000: Init complete (link UP)\n" : "E1000: Init complete (link DOWN)\n");
    netlog_write("[init] complete\n");
    
    return 0;
}

int e1000_shutdown_device(net_device_t* netdev) {
    e1000_device_t* dev = (e1000_device_t*)((char*)netdev - offsetof(e1000_device_t, net_dev));
    
    extern void gfx_print(const char*);
    gfx_print("E1000: Shutting device down...\n");
    
    // Disable RX and TX
    e1000_write_reg(dev, E1000_REG_RCTRL, 0);
    e1000_write_reg(dev, E1000_REG_TCTRL, 0);
    
    gfx_print("E1000: Device is now DOWN\n");
    
    return 0;
}

bool e1000_detect_pci(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_config_read_word(bus, slot, func, 0);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) return false;
    if (vendor_id != E1000_VENDOR_ID) return false;
    uint16_t device_id = pci_config_read_word(bus, slot, func, 2);
    if (device_id != E1000_DEV_ID_82540EM && device_id != E1000_DEV_ID_82545EM && device_id != E1000_DEV_ID_82574L) return false;

    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    gfx_print("E1000: Found Intel NIC (Device ID: "); gfx_print_hex(device_id); gfx_print(")\n");

    // Allocate and zero device struct
    e1000_dev = (e1000_device_t*)kmalloc(sizeof(e1000_device_t));
    if (!e1000_dev) { gfx_print("E1000: Failed to allocate device structure\n"); return false; }
    memset(e1000_dev, 0, sizeof(*e1000_dev));
    e1000_dev->pci_bus = bus; e1000_dev->pci_slot = slot; e1000_dev->pci_func = func;

    if (g_e1000_init_mode >= 1) {
        // Enable PCI Memory Space + Bus Master before MMIO usage
        uint16_t command = pci_config_read_word(bus, slot, func, 0x04);
        if ((command & 0x0006) != 0x0006) {
            command |= 0x0006;
            pci_config_write_word(bus, slot, func, 0x04, command);
        }

        // Select a valid MMIO BAR (skip I/O BARs)
        int chosen_index = -1; uint32_t chosen_bar = 0; uint32_t bar_phys = 0;
        for (int i = 0; i < 6; ++i) {
            uint32_t bar = pci_config_read_dword(bus, slot, func, 0x10 + (i * 4));
            if (bar == 0 || bar == 0xFFFFFFFF) continue;
            if (bar & 0x1) continue; // I/O space BAR — skip
            // Memory BAR
            chosen_index = i; chosen_bar = bar; bar_phys = bar & 0xFFFFFFF0; break;
        }
        if (chosen_index < 0 || bar_phys == 0 || bar_phys == 0xFFFFFFF0) {
            gfx_print("E1000: No usable MMIO BAR found\n");
            return false;
        }

        // First try identity mapping (BAR phys == virt) and probe; if it works we keep it.
        uint32_t bar_size = 128 * 1024; // legacy space
        uint32_t pages_id = (bar_size + 4095) / 4096;
        for (uint32_t i = 0; i < pages_id; ++i) {
            uint64_t addr = (uint64_t)bar_phys + (uint64_t)i * 4096ULL;
            vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);
        }
        e1000_dev->mem_base = bar_phys; e1000_dev->mmio_mapped = true;

        // Basic MMIO sanity log (identity mapping)
        extern void gfx_print_hex(uint32_t);
        gfx_print("E1000: Using identity MMIO BAR["); gfx_print_hex((uint32_t)chosen_index); gfx_print("] raw=0x"); gfx_print_hex(chosen_bar); gfx_print(" phys=virt=0x"); gfx_print_hex(bar_phys); gfx_print("\n");

        // Netlog BAR diagnostics (raw values for all BARs)
        netlog_write("[detect] BAR dump: ");
        for (int i = 0; i < 6; ++i) {
            uint32_t raw = pci_config_read_dword(bus, slot, func, 0x10 + (i * 4));
            netlog_write(" b"); netlog_write_hex("", i);
            netlog_write("="); netlog_write_hex("", raw);
        }
        netlog_write(" chosen="); netlog_write_hex("", chosen_index);
        netlog_write(" phys="); netlog_write_hex("", bar_phys);
        netlog_write(" virt="); netlog_write_hex("", (uint32_t)e1000_dev->mem_base);
        netlog_write("\n");

        // Multi-register MMIO probe (identity mapping)
        uint32_t ctrl0 = e1000_read_reg(e1000_dev, E1000_REG_CTRL);
        e1000_write_reg(e1000_dev, E1000_REG_CTRL, ctrl0 | E1000_CTRL_ASDE | E1000_CTRL_FD);
        uint32_t ctrl1 = e1000_read_reg(e1000_dev, E1000_REG_CTRL);
        e1000_write_reg(e1000_dev, E1000_REG_IMASK, 0x00000000);
        uint32_t imask0 = e1000_read_reg(e1000_dev, E1000_REG_IMASK);
        e1000_write_reg(e1000_dev, E1000_REG_IMASK, 0xFFFFFFFF);
        uint32_t imask1 = e1000_read_reg(e1000_dev, E1000_REG_IMASK);
        e1000_write_reg(e1000_dev, E1000_REG_TIPG, 0x0060200A);
        uint32_t tipg_rb = e1000_read_reg(e1000_dev, E1000_REG_TIPG);
        netlog_write("[detect] mmio probe CTRL0="); netlog_write_hex("", ctrl0);
        netlog_write(" CTRL1="); netlog_write_hex("", ctrl1);
        netlog_write(" IM0="); netlog_write_hex("", imask0);
        netlog_write(" IM1="); netlog_write_hex("", imask1);
        netlog_write(" TIPG="); netlog_write_hex("", tipg_rb);
        netlog_write("\n");
        if (ctrl0 == 0 && ctrl1 == 0 && imask0 == 0 && imask1 == 0 && tipg_rb == 0) {
            // Identity mapping failed; attempt relocation mapping at fixed VA
            netlog_write("[warn] identity MMIO inert; trying relocation mapping\n");
            const uint64_t RELOC_VA = 0x00000000E1000000ULL;
            for (uint32_t i = 0; i < pages_id; ++i) {
                uint64_t virt = RELOC_VA + (uint64_t)i * 4096ULL;
                uint64_t phys = (uint64_t)bar_phys + (uint64_t)i * 4096ULL;
                vmm_map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_CACHE);
            }
            e1000_dev->mem_base = (uint32_t)RELOC_VA;
            uint32_t r_ctrl0 = e1000_read_reg(e1000_dev, E1000_REG_CTRL);
            e1000_write_reg(e1000_dev, E1000_REG_CTRL, r_ctrl0 | E1000_CTRL_ASDE);
            uint32_t r_ctrl1 = e1000_read_reg(e1000_dev, E1000_REG_CTRL);
            netlog_write("[detect] reloc probe CTRL0="); netlog_write_hex("", r_ctrl0); netlog_write(" CTRL1="); netlog_write_hex("", r_ctrl1); netlog_write("\n");
            if (r_ctrl0 == 0 && r_ctrl1 == 0) {
                netlog_write("[fatal] mmio inert after relocation; stub only\n");
                e1000_dev->mmio_inert = true;
                strcpy(e1000_dev->net_dev.name, "eth0");
                e1000_dev->net_dev.state = NET_DEV_DOWN; e1000_dev->net_dev.mtu = 1500;
                e1000_dev->net_dev.send_packet = NULL; e1000_dev->net_dev.receive_packet = NULL;
                e1000_dev->net_dev.init = NULL; e1000_dev->net_dev.shutdown = NULL;
                extern int network_register_device(net_device_t* device);
                network_register_device(&e1000_dev->net_dev);
                return true;
            }
        }
    } else {
        // Safe mode: register stub device
        gfx_print("E1000: Safe mode active - registering minimal device without MMIO\n");
        strcpy(e1000_dev->net_dev.name, "eth0");
        e1000_dev->net_dev.state = NET_DEV_DOWN; e1000_dev->net_dev.mtu = 1500;
        e1000_dev->net_dev.send_packet = NULL; e1000_dev->net_dev.receive_packet = NULL; e1000_dev->net_dev.init = NULL; e1000_dev->net_dev.shutdown = NULL;
        extern int network_register_device(net_device_t* device);
        network_register_device(&e1000_dev->net_dev); return true;
    }

    // EEPROM presence (optional)
    e1000_write_reg(e1000_dev, E1000_REG_EEPROM, 0x01);
    for (int i = 0; i < 1000 && !(e1000_read_reg(e1000_dev, E1000_REG_EEPROM) & 0x10); i++);
    e1000_dev->has_eeprom = (e1000_read_reg(e1000_dev, E1000_REG_EEPROM) & 0x10) != 0;
    netlog_write("[init] eeprom present="); netlog_write_hex("", e1000_dev->has_eeprom ? 1 : 0); netlog_write("\n");

    // Read MAC, init rings
    e1000_read_mac_address(e1000_dev);
    e1000_program_mac_address(e1000_dev);
    gfx_print("E1000: MAC address: "); char mac_str[18]; extern void mac_addr_to_string(mac_addr_t* mac, char* buffer); mac_addr_to_string(&e1000_dev->net_dev.mac_address, mac_str); gfx_print(mac_str); gfx_print("\n");
    if (e1000_dev->mem_base) { gfx_print("E1000: Initializing RX/TX rings...\n"); e1000_init_rx(e1000_dev); e1000_init_tx(e1000_dev); gfx_print("E1000: RX/TX rings initialized\n"); }

    // Register device
    strcpy(e1000_dev->net_dev.name, "eth0"); e1000_dev->net_dev.state = NET_DEV_DOWN; e1000_dev->net_dev.mtu = 1500;
    e1000_dev->net_dev.rx_packets = e1000_dev->net_dev.tx_packets = 0; e1000_dev->net_dev.rx_bytes = e1000_dev->net_dev.tx_bytes = 0;
    e1000_dev->net_dev.rx_errors = e1000_dev->net_dev.tx_errors = 0; e1000_dev->net_dev.send_packet = e1000_send_packet; e1000_dev->net_dev.receive_packet = NULL;
    e1000_dev->net_dev.init = e1000_init_device; e1000_dev->net_dev.shutdown = e1000_shutdown_device;
    // Default guest IP for isolated bridge (br0 192.168.100.1/24)
    e1000_dev->net_dev.ip_address.addr[0] = 192;
    e1000_dev->net_dev.ip_address.addr[1] = 168;
    e1000_dev->net_dev.ip_address.addr[2] = 100;
    e1000_dev->net_dev.ip_address.addr[3] = 2;
    // Netmask 255.255.255.0
    e1000_dev->net_dev.netmask.addr[0] = 255;
    e1000_dev->net_dev.netmask.addr[1] = 255;
    e1000_dev->net_dev.netmask.addr[2] = 255;
    e1000_dev->net_dev.netmask.addr[3] = 0;
    // Gateway (host side bridge address)
    e1000_dev->net_dev.gateway.addr[0] = 192;
    e1000_dev->net_dev.gateway.addr[1] = 168;
    e1000_dev->net_dev.gateway.addr[2] = 100;
    e1000_dev->net_dev.gateway.addr[3] = 1;
    extern int network_register_device(net_device_t* device);
    if (network_register_device(&e1000_dev->net_dev) != 0) { gfx_print("E1000: Failed to register network device\n"); return false; }
    if (e1000_dev->mem_base && !e1000_dev->mmio_inert) {
        e1000_dev->net_dev.state = NET_DEV_RUNNING; e1000_init_device(&e1000_dev->net_dev);
    } else if (e1000_dev->mmio_inert) {
        netlog_write("[fatal] skip device init due to inert mmio\n");
    }
    gfx_print("E1000: Device initialized successfully\n");
    return true;
}

// Poll for received packets
void e1000_poll_receive(e1000_device_t* dev) {
    extern void gfx_print(const char*);
    extern void serial_debug(const char*);
    if (!g_e1000_poll_enabled) {
        return; // polling disabled to reduce system load
    }
    
    if (!dev) {
        gfx_print("[RX: dev null]");
        return;
    }
    if (!dev->mem_base) {
        gfx_print("[RX: no mem_base]");
        return;
    }
    if (!dev->rx_descs) {
        gfx_print("[RX: descs null]");
        return;
    }
    
    // Check link status, but do not bail out if down; continue polling
    // Some environments report link down while RX still functions (e.g., early autoneg)
    // uint32_t status = e1000_read_reg(dev, E1000_REG_STATUS);
    // if (!(status & 0x02)) {
    //     gfx_print("[RX: link down]");
    // }
    
    // Periodic ring + link snapshot (throttled)
    static uint32_t poll_count = 0;
    if ((poll_count++ & 0x1F) == 0) { // every 32 polls
        uint32_t rdh_snap = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
        uint32_t rdt_snap = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
        uint32_t rctl_snap = e1000_read_reg(dev, E1000_REG_RCTRL);
        uint32_t tctl_snap = e1000_read_reg(dev, E1000_REG_TCTRL);
        uint32_t stat_snap = e1000_read_reg(dev, E1000_REG_STATUS);
        netlog_write("[rxsnap] H="); netlog_write_hex("", rdh_snap);
        netlog_write(" T="); netlog_write_hex("", rdt_snap);
        netlog_write(" RCTL="); netlog_write_hex("", rctl_snap);
        netlog_write(" TCTL="); netlog_write_hex("", tctl_snap);
        netlog_write(" STATUS="); netlog_write_hex("", stat_snap);
        // First four descriptor status/length
        for (int di = 0; di < 4; ++di) {
            e1000_rx_desc_t* dx = &dev->rx_descs[di];
            netlog_write(" d"); netlog_write_hex("", di);
            netlog_write(" s="); netlog_write_hex("", dx->status);
            netlog_write(" l="); netlog_write_hex("", dx->length);
        }
        netlog_write("\n");
    }
    // Check receive descriptors for new packets
    int checked = 0;
    static uint32_t no_dd_polls = 0;
    while (checked < E1000_NUM_RX_DESC) {
        uint32_t current = dev->rx_current;
        if (current >= E1000_NUM_RX_DESC) {
            dev->rx_current = 0;
            break;
        }
        
        e1000_rx_desc_t* desc = &dev->rx_descs[current];
        
        // Debug on first check
        if (checked == 0) {
            uint32_t rdh = e1000_read_reg(dev, E1000_REG_RXDESCHEAD);
            uint32_t rdt = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
            LOG_TS("[RX: cur=");
            char buf[32];
            buf[0] = (current / 10) + '0';
            buf[1] = (current % 10) + '0';
            buf[2] = ' '; buf[3] = 'H'; buf[4] = '=';
            buf[5] = (rdh / 10) + '0';
            buf[6] = (rdh % 10) + '0';
            buf[7] = ' '; buf[8] = 'T'; buf[9] = '=';
            buf[10] = (rdt / 10) + '0';
            buf[11] = (rdt % 10) + '0';
            buf[12] = ' '; buf[13] = 's'; buf[14] = '=';
            uint8_t s = desc->status;
            buf[15] = (s >> 4) < 10 ? '0' + (s >> 4) : 'A' + (s >> 4) - 10;
            buf[16] = (s & 0xF) < 10 ? '0' + (s & 0xF) : 'A' + (s & 0xF) - 10;
            buf[17] = ']'; buf[18] = '\\'; buf[19] = 'n'; buf[20] = '\0';
            SERIAL_LOG(buf);
            // Mirror to netlog for offline analysis, but throttle spam:
            // Only log if head/tail changed OR every 256 polls
            static uint32_t last_rdh = 0xFFFFFFFF, last_rdt = 0xFFFFFFFF;
            static uint32_t head_tail_log_count = 0;
            head_tail_log_count++;
            if (rdh != last_rdh || rdt != last_rdt || (head_tail_log_count & 0xFF) == 0) {
                netlog_write("[rx] head="); netlog_write_hex("", rdh); netlog_write(" tail="); netlog_write_hex("", rdt);
                netlog_write(" cur="); netlog_write_hex("", current); netlog_write(" st="); netlog_write_hex("", s); netlog_write("\n");
                last_rdh = rdh; last_rdt = rdt;
            }

            // If ring looks idle (H=0,T=0,status=0), re-sequence RX enable and kick tail
            if (rdh == 0 && rdt == 0 && s == 0) {
                uint32_t cur_rctl = e1000_read_reg(dev, E1000_REG_RCTRL);
                // Disable RX momentarily
                e1000_write_reg(dev, E1000_REG_RCTRL, (cur_rctl & ~E1000_RCTL_EN));
                (void)e1000_read_reg(dev, E1000_REG_STATUS);
                // Advertise descriptors
                e1000_write_reg(dev, E1000_REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
                (void)e1000_read_reg(dev, E1000_REG_STATUS);
                // Re-enable RX
                e1000_write_reg(dev, E1000_REG_RCTRL, cur_rctl | E1000_RCTL_EN);
                (void)e1000_read_reg(dev, E1000_REG_STATUS);
                LOG_TS("[RX: kick]\n");
                // Log kick result
                uint32_t new_rdt = e1000_read_reg(dev, E1000_REG_RXDESCTAIL);
                netlog_write("[rx] kick tail="); netlog_write_hex("", new_rdt); netlog_write("\n");
            }
        }
        
        // Check if descriptor has been used by hardware (DD bit set)
        if (!(desc->status & E1000_RXD_STAT_DD)) {
            // If we never see any DD over many polls, emit diagnostic once per threshold
            static uint32_t rx_idle_count = 0;
            rx_idle_count++;
            no_dd_polls++;
            if ((rx_idle_count & 0x3FF) == 0) { // every 1024 idle iterations
                netlog_write("[rx-idle] cur="); netlog_write_hex("", current); netlog_write(" st="); netlog_write_hex("", desc->status); netlog_write("\n");
            }
            // If prolonged no-DD condition, force resequence as a recovery attempt
            if (no_dd_polls != 0 && (no_dd_polls % 2048) == 0) {
                e1000_rx_resequence(dev, "idle-recover");
            }
            break; // No more packets
        }
        
        gfx_print("[RX: pkt!]");
        LOG_TS("[RX: pkt!]\n");
        checked++;
        
        // We have a packet!
        uint16_t length = desc->length;
        if (length > 8192 || length == 0) {
            // Invalid length, skip this packet
            desc->status = 0;
            dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
            continue;
        }
        
        // Use the buffer pointer from our array instead of casting from descriptor
        uint8_t* packet_data = dev->rx_buffers[current];
        if (!packet_data) {
            desc->status = 0;
            dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
            continue;
        }
        
        // Process the packet through the network stack
        extern void ethernet_receive_frame(net_device_t* dev, const uint8_t* data, uint32_t len);
        extern void serial_debug(const char*);
        
        LOG_TS("[RX: process]\n");
        ethernet_receive_frame(&dev->net_dev, packet_data, length);
        LOG_TS("[RX: processed]\n");
        
        // Update statistics
        dev->net_dev.rx_packets++;
        dev->net_dev.rx_bytes += length;
        
        // Reset descriptor for reuse  
        desc->status = 0;
        desc->errors = 0;
        desc->length = 0;
        
        // Move to next descriptor
        dev->rx_current = (current + 1) % E1000_NUM_RX_DESC;
        
        // Return this descriptor to hardware by advancing RDT
        // Simple policy: hand back the descriptor we just consumed
        e1000_write_reg(dev, E1000_REG_RXDESCTAIL, current);
        LOG_TS("[RX: freed]\n");

        // Seen a packet; reset no-DD counter
        no_dd_polls = 0;
    }
}

// Helper function to poll for packets (can be called from commands)
void e1000_check_packets(void) {
    extern void gfx_print(const char*);
    
    if (!e1000_dev) {
        gfx_print("[RX: no dev]");
        return;
    }
    
    if (e1000_dev->net_dev.state != NET_DEV_RUNNING) {
        gfx_print("[RX: not running]");
        return;
    }
    
    if (!e1000_dev->rx_descs) {
        gfx_print("[RX: no descs]");
        return;
    }
    
    // Refresh link STATUS; log changes
    uint32_t st = e1000_read_reg(e1000_dev, E1000_REG_STATUS);
    if (st != e1000_dev->last_status) {
        bool now_up = (st & 0x02) != 0;
        if (now_up != e1000_dev->link_up) {
            e1000_dev->link_up = now_up;
            netlog_write("[link] change "); netlog_write(now_up ? "UP " : "DOWN "); netlog_write("status="); netlog_write_hex("", st); netlog_write("\n");
            e1000_log_status(st, "chg");
            // If link just came UP, attempt a gratuitous ARP broadcast to stimulate cache
            if (now_up) {
                // On link-up, resequence RX to be safe then advertise all descriptors
                e1000_rx_resequence(e1000_dev, "link-up");
                // On link-up, re-advertise all RX descriptors to kick RX engine
                e1000_rx_advertise_all(e1000_dev);
                uint32_t rdt_after = e1000_read_reg(e1000_dev, E1000_REG_RXDESCTAIL);
                netlog_write("[rx] link-up tail adv RDT="); netlog_write_hex("", rdt_after); netlog_write("\n");
                extern int ethernet_send_frame(net_device_t* dev, mac_addr_t* dest_mac, uint16_t ethertype, const uint8_t* payload, uint32_t payload_len);
                extern void arp_retry_pending(net_device_t* dev);
                // Build minimal gratuitous ARP (announce our IP->MAC)
                mac_addr_t bcast = {{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};
                struct __attribute__((packed)) { uint16_t hw_type; uint16_t proto_type; uint8_t hw_len; uint8_t proto_len; uint16_t opcode; mac_addr_t sender_mac; ipv4_addr_t sender_ip; mac_addr_t target_mac; ipv4_addr_t target_ip; } garp;
                garp.hw_type = __builtin_bswap16(1); garp.proto_type = __builtin_bswap16(0x0800); garp.hw_len=6; garp.proto_len=4; garp.opcode = __builtin_bswap16(2); // reply opcode
                memcpy(&garp.sender_mac, &e1000_dev->net_dev.mac_address, sizeof(mac_addr_t));
                memcpy(&garp.sender_ip, &e1000_dev->net_dev.ip_address, sizeof(ipv4_addr_t));
                memcpy(&garp.target_mac, &e1000_dev->net_dev.mac_address, sizeof(mac_addr_t));
                memcpy(&garp.target_ip, &e1000_dev->net_dev.ip_address, sizeof(ipv4_addr_t));
                (void)ethernet_send_frame(&e1000_dev->net_dev, &bcast, 0x0806, (uint8_t*)&garp, sizeof(garp));
                netlog_write("[arp-grat] sent on link-up\n");
                // Retry any pending ARP lookups now that link is up
                arp_retry_pending(&e1000_dev->net_dev);
            }
        }
        e1000_dev->last_status = st;
    }

    // If link remains down, attempt periodic re-kick every 1024 polls
    if (!e1000_dev->link_up) {
        e1000_dev->link_retry_count++;
        if ((e1000_dev->link_retry_count & 0x3FF) == 0) { // every 1024 checks
            netlog_write("[link] retry attempt status="); netlog_write_hex("", st); netlog_write(" count="); netlog_write_hex("", e1000_dev->link_retry_count); netlog_write("\n");
            // Reassert CTRL
            uint32_t ctrl_retry2 = e1000_read_reg(e1000_dev, E1000_REG_CTRL);
            ctrl_retry2 |= (E1000_CTRL_ASDE | E1000_CTRL_SLU | E1000_CTRL_FD);
            e1000_write_reg(e1000_dev, E1000_REG_CTRL, ctrl_retry2);
            netlog_write("[link] ctrl reassert periodic ctrl="); netlog_write_hex("", ctrl_retry2); netlog_write("\n");
            // Double-read PHY status live
            uint16_t phy_lat=0, phy_live=0;
            if (mdic_read(e1000_dev, 1, 1, &phy_lat) && mdic_read(e1000_dev, 1, 1, &phy_live)) {
                netlog_write("[link] periodic phy lat="); netlog_write_hex("", phy_lat); netlog_write(" live="); netlog_write_hex("", phy_live); netlog_write("\n");
                if ((phy_live & (1<<2)) && (phy_live & (1<<5))) {
                    // Update link state immediately
                    e1000_dev->link_up = true;
                    e1000_dev->last_status = st; // STATUS may already reflect LU soon
                    netlog_write("[link] periodic link now UP\n");
                }
            }
        }
    }

    e1000_poll_receive(e1000_dev);
}

void e1000_init(void) {
    extern void gfx_print(const char*);
    
    // Serial debug output
    extern void serial_debug(const char*);
    serial_debug("[E1000] Init starting...\n");
    
    gfx_print("E1000: Scanning PCI bus for Intel NICs...\n");
    
    // Scan bus 0 for a network controller (class 0x02), try all functions
    serial_debug("[E1000] Starting PCI scan of bus 0 (class-filter)\n");
    for (uint8_t slot = 0; slot < 32; slot++) {
        // If no function 0, skip slot
        uint16_t vendor0 = pci_config_read_word(0, slot, 0, 0x00);
        if (vendor0 == 0xFFFF || vendor0 == 0x0000) continue;

        for (uint8_t func = 0; func < 8; func++) {
            uint16_t vendor = pci_config_read_word(0, slot, func, 0x00);
            if (vendor == 0xFFFF || vendor == 0x0000) continue;
            // Read class code (offset 0x0A high byte is base class)
            uint16_t class_sub = pci_config_read_word(0, slot, func, 0x0A);
            uint8_t base_class = (class_sub >> 8) & 0xFF; // 0x02 = Network controller
            if (base_class != 0x02) continue;

            if (e1000_detect_pci(0, slot, func)) {
                serial_debug("[E1000] Network device found and initialized\n");
                gfx_print("E1000: Found and initialized Intel NIC\n");
                return; // Found one
            }
        }
    }
    
    serial_debug("[E1000] No device found, init complete\n");
    gfx_print("E1000: No Intel NIC found on bus 0\n");
}

void e1000_print_info(void) {
    extern void gfx_print(const char*);
    
    if (!e1000_dev) {
        gfx_print("E1000: Not initialized\n");
        return;
    }
    
    gfx_print("E1000 Network Interface:\n");
    gfx_print("  Device: eth0 (E1000)\n");
    gfx_print("  Status: Initialized\n");
    
    if (!e1000_dev->mmio_mapped || !e1000_dev->mem_base) {
        gfx_print("  MMIO: not mapped, skipping status registers\n");
        return;
    }
    uint32_t status = e1000_read_reg(e1000_dev, E1000_REG_STATUS);
    gfx_print("  Link: ");
    gfx_print((status & 0x02) ? "UP\n" : "DOWN\n");
    gfx_print("  Speed: ");
    gfx_print((status & 0x40) ? "1000Mbps\n" : "10/100Mbps\n");
}

// Extended diagnostics: print key registers for bring-up debugging
void e1000_print_diag(void) {
    extern void gfx_print(const char*);
    extern void gfx_print_hex(uint32_t);
    if (!e1000_dev || !e1000_dev->mmio_mapped || !e1000_dev->mem_base) {
        gfx_print("E1000: diag unavailable (no MMIO)\n");
        return;
    }
    const uint16_t regs[] = {
        E1000_REG_CTRL, E1000_REG_STATUS, E1000_REG_RCTRL, E1000_REG_TCTRL,
        E1000_REG_RXDESCHEAD, E1000_REG_RXDESCTAIL, E1000_REG_TXDESCHEAD, E1000_REG_TXDESCTAIL,
        E1000_REG_TIPG, E1000_REG_RDTR, E1000_REG_RXDCTL, E1000_REG_TXDCTL,
        E1000_REG_RAL0, E1000_REG_RAH0
    };
    const char* names[] = {
        "CTRL", "STATUS", "RCTL", "TCTL",
        "RDH", "RDT", "TDH", "TDT",
        "TIPG", "RDTR", "RXDCTL", "TXDCTL",
        "RAL0", "RAH0"
    };
    gfx_print("E1000 diag: \n");
    gfx_print("  RX ring phys: 0x"); extern void gfx_print_hex(uint32_t);
    gfx_print_hex((uint32_t)(e1000_dev->rx_desc_phys & 0xFFFFFFFF)); gfx_print("\n");
    gfx_print("  TX ring phys: 0x");
    gfx_print_hex((uint32_t)(e1000_dev->tx_desc_phys & 0xFFFFFFFF)); gfx_print("\n");
    // Also show ring virtual pointers and RDBAL/RDBAH/RDLEN
    gfx_print("  RX ring va: "); gfx_print_hex((uint32_t)(uintptr_t)e1000_dev->rx_descs); gfx_print("\n");
    gfx_print("  TX ring va: "); gfx_print_hex((uint32_t)(uintptr_t)e1000_dev->tx_descs); gfx_print("\n");
    uint32_t rdbal = e1000_read_reg(e1000_dev, E1000_REG_RDBAL);
    uint32_t rdbah = e1000_read_reg(e1000_dev, E1000_REG_RDBAH);
    uint32_t rdlen = e1000_read_reg(e1000_dev, E1000_REG_RDLEN);
    gfx_print("  RDBAL = "); gfx_print_hex(rdbal); gfx_print(" RDBAH = "); gfx_print_hex(rdbah); gfx_print(" RDLEN = "); gfx_print_hex(rdlen); gfx_print("\n");
    for (unsigned i = 0; i < sizeof(regs)/sizeof(regs[0]); ++i) {
        gfx_print("  "); gfx_print(names[i]); gfx_print(" = ");
        uint32_t v = e1000_read_reg(e1000_dev, regs[i]);
        gfx_print_hex(v); gfx_print("\n");
    }
    // Dump first few RX/TX descriptors
    if (e1000_dev->rx_descs && e1000_dev->tx_descs) {
        for (int i = 0; i < 4; ++i) {
            uint32_t addr_lo = (uint32_t)(e1000_dev->rx_descs[i].addr & 0xFFFFFFFFULL);
            uint32_t addr_hi = (uint32_t)((e1000_dev->rx_descs[i].addr >> 32) & 0xFFFFFFFFULL);
            uint16_t len = e1000_dev->rx_descs[i].length;
            uint8_t st = e1000_dev->rx_descs[i].status;
            gfx_print("  RXD"); gfx_print_decimal(i); gfx_print(": addr_lo="); gfx_print_hex(addr_lo); gfx_print(" addr_hi="); gfx_print_hex(addr_hi);
            gfx_print(" len="); gfx_print_decimal(len); gfx_print(" st="); gfx_print_hex(st); gfx_print("\n");
            // Raw 16-byte view (4 dwords) to validate mapping contents
            uint32_t* w = (uint32_t*)&e1000_dev->rx_descs[i];
            gfx_print("    raw: "); gfx_print_hex(w[0]); gfx_print(" "); gfx_print_hex(w[1]); gfx_print(" "); gfx_print_hex(w[2]); gfx_print(" "); gfx_print_hex(w[3]); gfx_print("\n");
            // Also show stored phys for comparison
            gfx_print("    phys: "); gfx_print_hex((uint32_t)(e1000_dev->rx_buf_phys[i] & 0xFFFFFFFFULL)); gfx_print(" "); gfx_print_hex((uint32_t)(e1000_dev->rx_buf_phys[i] >> 32)); gfx_print("\n");
        }
        for (int i = 0; i < 2; ++i) {
            uint32_t addr_lo = (uint32_t)(e1000_dev->tx_descs[i].addr & 0xFFFFFFFFULL);
            uint32_t addr_hi = (uint32_t)((e1000_dev->tx_descs[i].addr >> 32) & 0xFFFFFFFFULL);
            uint16_t len = e1000_dev->tx_descs[i].length;
            uint8_t st = e1000_dev->tx_descs[i].status;
            gfx_print("  TXD"); gfx_print_decimal(i); gfx_print(": addr_lo="); gfx_print_hex(addr_lo); gfx_print(" addr_hi="); gfx_print_hex(addr_hi);
            gfx_print(" len="); gfx_print_decimal(len); gfx_print(" st="); gfx_print_hex(st); gfx_print("\n");
            uint32_t* w = (uint32_t*)&e1000_dev->tx_descs[i];
            gfx_print("    raw: "); gfx_print_hex(w[0]); gfx_print(" "); gfx_print_hex(w[1]); gfx_print(" "); gfx_print_hex(w[2]); gfx_print(" "); gfx_print_hex(w[3]); gfx_print("\n");
            gfx_print("    phys: "); gfx_print_hex((uint32_t)(e1000_dev->tx_buf_phys[i] & 0xFFFFFFFFULL)); gfx_print(" "); gfx_print_hex((uint32_t)(e1000_dev->tx_buf_phys[i] >> 32)); gfx_print("\n");
        }
    }
}

// Send a simple broadcast Ethernet frame to exercise TX path
void e1000_send_test_broadcast(void) {
    if (!e1000_dev) return;
    if (e1000_dev->net_dev.state != NET_DEV_RUNNING) return;
    // Build minimal 60-byte Ethernet frame (no VLAN, payload padded)
    static uint8_t frame[60];
    // First packet: broadcast destination
    for (int i = 0; i < 6; ++i) frame[i] = 0xFF;
    for (int i = 0; i < 6; ++i) frame[6 + i] = e1000_dev->net_dev.mac_address.addr[i];
    // Ethertype: 0x0800 (IPv4) just placeholder
    frame[12] = 0x08; frame[13] = 0x00;
    // Payload: ASCII pattern "QARMA-TX-TEST" then pad with incrementing bytes
    const char* pat = "QARMA-TX-TEST";
    int pat_len = 13;
    int offset = 14;
    for (int i = 0; i < pat_len && (offset + i) < 60; ++i) frame[offset + i] = (uint8_t)pat[i];
    int fill_start = offset + pat_len;
    for (int i = fill_start; i < 60; ++i) frame[i] = (uint8_t)(i & 0xFF);
    net_packet_t pkt;
    pkt.data = frame;
    pkt.length = 60; // (no FCS included)
    pkt.capacity = 60;
    pkt.protocol = NET_PROTO_ETHERNET;
    pkt.protocol_header = NULL;
    int rc = e1000_send_packet(&e1000_dev->net_dev, &pkt);
    netlog_write("[txtest] bcast result="); netlog_write_hex("", rc == 0 ? 1 : 0); netlog_write(" len="); netlog_write_hex("", pkt.length); netlog_write("\n");

    // Second packet: unicast-to-self to exercise unicast accept path
    for (int i = 0; i < 6; ++i) frame[i] = e1000_dev->net_dev.mac_address.addr[i];
    // keep the rest of the frame payload the same
    rc = e1000_send_packet(&e1000_dev->net_dev, &pkt);
    netlog_write("[txtest] self result="); netlog_write_hex("", rc == 0 ? 1 : 0); netlog_write(" len="); netlog_write_hex("", pkt.length); netlog_write("\n");
}

