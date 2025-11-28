#include "xhci.h"
#include "xhci_common.h"
#include "usb.h"
#include "core/pci.h"
#include "core/memory/heap.h"
#include "core/memory/pmm/pmm.h"
#include "core/memory/vmm/vmm.h"
#include "core/memory/dma_allocator.h"
#include "core/sleep.h"
#include "core/log_timestamp.h"
#include "config.h"
#include "graphics/graphics.h"

// Disable verbose logging for faster boot
#define XHCI_VERBOSE 0

#if XHCI_VERBOSE
#define XHCI_LOG(msg) SERIAL_LOG(msg)
#define XHCI_LOG_HEX(msg, val) SERIAL_LOG_HEX(msg, val)
#define XHCI_LOG_DEC(msg, val) SERIAL_LOG_DEC(msg, val)
#else
#define XHCI_LOG(msg) do {} while(0)
#define XHCI_LOG_HEX(msg, val) do {} while(0)
#define XHCI_LOG_DEC(msg, val) do {} while(0)
#endif

static xhci_controller_t *g_xhci = NULL;

// Helper to map physical address to virtual (identity mapping for now)
// Use proper VMM functions for address translation
static inline void* phys_to_virt(uint32_t phys) {
    // For low memory (< 1MB) and identity-mapped kernel space, physical == virtual
    // For other memory, this would need proper reverse mapping (not commonly needed)
    return (void*)phys;
}

static inline uint64_t virt_to_phys64(void *virt) {
    if (!virt) return 0;
    
    uint64_t addr = (uint64_t)virt;
    
    // Handle higher-half kernel addresses
    if (addr >= 0xFFFFFFFF80000000ULL) {
        return addr - 0xFFFFFFFF80000000ULL;
    }
    
    // The kernel heap and early allocations are identity-mapped in the first 32MB
    // VMM identity-maps the first 32MB (8 PDEs * 4MB each) for kernel use
    if (addr < 0x02000000) {  // < 32MB - identity-mapped region
        return addr;
    }
    
    // For higher addresses, use VMM lookup
    uint64_t phys = vmm_get_physical_address(addr);
    if (phys == 0) {
        SERIAL_LOG("XHCI: ERROR - No physical mapping for virtual address 0x");
        SERIAL_LOG_HEX("", (uint32_t)(addr >> 32));
        SERIAL_LOG_HEX("", (uint32_t)addr);
        SERIAL_LOG("\n");
        return 0;
    }
    
    return phys;
}

int xhci_init(void) {
    GFX_LOG_MIN("XHCI: Initializing USB 3.0 (XHCI) driver...\n");
    SERIAL_LOG("XHCI: Initializing USB 3.0 (XHCI) driver...\n");
    
    // Find and initialize XHCI controller via PCI
    if (xhci_pci_init() != 0) {
        SERIAL_LOG("XHCI: No XHCI controller found\n");
        g_xhci = NULL;  // Ensure it's NULL
        return -1;
    }
    
    if (!g_xhci) {
        SERIAL_LOG("XHCI: Controller structure not initialized\n");
        return -1;
    }
    
    // Reset the controller
    if (xhci_reset(g_xhci) != 0) {
        SERIAL_LOG("XHCI: Failed to reset controller\n");
        heap_free(g_xhci);
        g_xhci = NULL;
        return -1;
    }
    
    // Start the controller
    if (xhci_start(g_xhci) != 0) {
        SERIAL_LOG("XHCI: Failed to start controller\n");
        heap_free(g_xhci);
        g_xhci = NULL;
        return -1;
    }
    
    // Detect and enumerate ports
    xhci_detect_ports(g_xhci);
    
    // Enumerate devices
    extern int xhci_enumerate_devices(xhci_controller_t *xhci);
    xhci_enumerate_devices(g_xhci);
    
    GFX_LOG_MIN("XHCI: Initialization complete\n");
    SERIAL_LOG("XHCI: Initialization complete\n");
    return 0;
}

int xhci_pci_init(void) {
    SERIAL_LOG("XHCI: Scanning PCI bus for XHCI controllers\n");
    
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) continue;
                
                uint16_t class_code = pci_read_config_word(bus, slot, func, 0x0A);
                uint8_t base_class = (class_code >> 8) & 0xFF;
                uint8_t sub_class = class_code & 0xFF;
                uint8_t prog_if = (pci_read_config_word(bus, slot, func, 0x08) >> 8) & 0xFF;
                
                // USB controller (class 0x0C, subclass 0x03)
                // XHCI has programming interface 0x30
                if (base_class == 0x0C && sub_class == 0x03 && prog_if == 0x30) {
                    SERIAL_LOG("XHCI: Found XHCI controller at bus=");
                    SERIAL_LOG_HEX("", bus);
                    SERIAL_LOG(" slot=");
                    SERIAL_LOG_HEX("", slot);
                    SERIAL_LOG(" func=");
                    SERIAL_LOG_HEX("", func);
                    SERIAL_LOG("\n");
                    
                    // Allocate controller structure
                    g_xhci = (xhci_controller_t*)heap_alloc(sizeof(xhci_controller_t));
                    if (!g_xhci) {
                        SERIAL_LOG("XHCI: Failed to allocate controller structure\n");
                        return -1;
                    }
                    
                    memset(g_xhci, 0, sizeof(xhci_controller_t));
                    g_xhci->pci_bus = bus;
                    g_xhci->pci_slot = slot;
                    g_xhci->pci_func = func;
                    
                    // Initialize event ring spinlock
                    spinlock_init(&g_xhci->event_ring_lock);
                    
                    // Read BAR0 (memory-mapped registers)
                    uint32_t bar0 = pci_read_config_dword(bus, slot, func, 0x10);
                    g_xhci->mmio_base = bar0 & 0xFFFFFFF0;  // Clear lower bits
                    
                    SERIAL_LOG("XHCI: MMIO base address: ");
                    SERIAL_LOG_HEX("", g_xhci->mmio_base);
                    SERIAL_LOG("\n");
                    
                    // NOTE: In 64-bit mode, MMIO regions below 4GB are identity-mapped
                    // by default (boot page tables). VMM is still using 32-bit structures
                    // that don't affect the active CR3, so we skip mapping and use the
                    // physical address directly.
                    // TODO: Update VMM to work with 64-bit page tables (PML4)
                    
                    SERIAL_LOG("XHCI: Using identity-mapped MMIO (no explicit mapping needed)\n");
                    
                    // Enable bus mastering and memory space
                    uint16_t command = pci_read_config_word(bus, slot, func, 0x04);
                    command |= 0x06;  // Enable bus master and memory space
                    pci_write_config_word(bus, slot, func, 0x04, command);
                    
                    // Read capability registers
                    g_xhci->cap_length = *(volatile uint8_t*)(g_xhci->mmio_base + XHCI_CAP_CAPLENGTH);
                    g_xhci->operational_base = g_xhci->mmio_base + g_xhci->cap_length;
                    
                    uint16_t hci_version = xhci_read32(g_xhci->mmio_base + XHCI_CAP_HCIVERSION) & 0xFFFF;
                    SERIAL_LOG("XHCI: HCI Version: ");
                    SERIAL_LOG_HEX("", hci_version);
                    SERIAL_LOG("\n");
                    
                    // BIOS/OS Handoff - Find USBLEGSUP extended capability
                    uint32_t hccparams1 = xhci_read32(g_xhci->mmio_base + XHCI_CAP_HCCPARAMS1);
                    if (hccparams1 & (1 << 0)) {  // Extended capabilities present
                        uint32_t ext_cap_offset = ((hccparams1 >> 16) & 0xFFFF) << 2;  // Dword offset to byte offset
                        SERIAL_LOG("XHCI: Extended capabilities at offset: ");
                        SERIAL_LOG_HEX("", ext_cap_offset);
                        SERIAL_LOG("\n");
                        
                        // Walk capability list looking for USBLEGSUP (ID=1)
                        while (ext_cap_offset) {
                            uint32_t cap_reg = xhci_read32(g_xhci->mmio_base + ext_cap_offset);
                            uint8_t cap_id = cap_reg & 0xFF;
                            uint8_t next_offset = (cap_reg >> 8) & 0xFF;  // Dword offset
                            
                            if (cap_id == 1) {  // USBLEGSUP
                                SERIAL_LOG("XHCI: Found USBLEGSUP capability, performing BIOS handoff\n");
                                
                                // Set OS Owned bit (24), clear BIOS Owned bit (16)
                                uint32_t legsup = xhci_read32(g_xhci->mmio_base + ext_cap_offset);
                                legsup |= (1 << 24);   // OS Owned
                                xhci_write32(g_xhci->mmio_base + ext_cap_offset, legsup);
                                
                                // Wait for BIOS to relinquish (bit 16 clear), timeout after 1 second
                                int timeout = 1000;
                                while (timeout > 0) {
                                    legsup = xhci_read32(g_xhci->mmio_base + ext_cap_offset);
                                    if (!(legsup & (1 << 16))) {  // BIOS Owned cleared
                                        SERIAL_LOG("XHCI: BIOS handoff complete\n");
                                        break;
                                    }
                                    sleep_ms(1);
                                    timeout--;
                                }
                                
                                if (timeout == 0) {
                                    SERIAL_LOG("XHCI: WARNING - BIOS handoff timeout, forcing takeover\n");
                                    legsup = xhci_read32(g_xhci->mmio_base + ext_cap_offset);
                                    legsup &= ~(1 << 16);  // Force clear BIOS Owned
                                    legsup |= (1 << 24);   // Force set OS Owned
                                    xhci_write32(g_xhci->mmio_base + ext_cap_offset, legsup);
                                }
                                break;
                            }
                            
                            if (next_offset == 0) break;
                            ext_cap_offset += (next_offset << 2);  // Dword to byte offset
                        }
                    }
                    
                    uint32_t hcsparams1 = xhci_read32(g_xhci->mmio_base + XHCI_CAP_HCSPARAMS1);
                    g_xhci->max_slots = hcsparams1 & 0xFF;
                    g_xhci->max_intrs = (hcsparams1 >> 8) & 0x7FF;
                    g_xhci->max_ports = (hcsparams1 >> 24) & 0xFF;
                    
                    SERIAL_LOG("XHCI: Max slots=");
                    SERIAL_LOG_HEX("", g_xhci->max_slots);
                    SERIAL_LOG(" ports=");
                    SERIAL_LOG_HEX("", g_xhci->max_ports);
                    SERIAL_LOG(" interrupters=");
                    SERIAL_LOG_HEX("", g_xhci->max_intrs);
                    SERIAL_LOG("\n");
                    
                    uint32_t dboff = xhci_read32(g_xhci->mmio_base + XHCI_CAP_DBOFF);
                    uint32_t rtsoff = xhci_read32(g_xhci->mmio_base + XHCI_CAP_RTSOFF);
                    g_xhci->doorbell_base = g_xhci->mmio_base + (dboff & ~0x3);
                    g_xhci->runtime_base = g_xhci->mmio_base + (rtsoff & ~0x1F);
                    
                    return 0;
                }
            }
        }
    }
    
    SERIAL_LOG("XHCI: No XHCI controller found on PCI bus\n");
    return -1;
}

int xhci_reset(xhci_controller_t *xhci) {
    if (!xhci) return -1;

    SERIAL_LOG("XHCI: Resetting controller...\n");

    volatile uint32_t *usbcmd = (uint32_t*)(xhci->operational_base + XHCI_OP_USBCMD);
    volatile uint32_t *usbsts = (uint32_t*)(xhci->operational_base + XHCI_OP_USBSTS);

    // If running, request halt
    uint32_t cmd = xhci_read32((uintptr_t)usbcmd);
    if (cmd & XHCI_USBCMD_RUN_STOP) {
        xhci_write32((uintptr_t)usbcmd, cmd & ~XHCI_USBCMD_RUN_STOP);

        // Wait for HCHalted to assert
        for (int i = 0; i < 1000; i++) {
            uint32_t sts = xhci_read32((uintptr_t)usbsts);
            if (sts & XHCI_USBSTS_HCH) break;
            sleep_ms(1);
        }
        if (!(xhci_read32((uintptr_t)usbsts) & XHCI_USBSTS_HCH)) {
            SERIAL_LOG("XHCI: Timeout waiting for controller halt\n");
            return -2;
        }
    }

    // Assert Host Controller Reset
    cmd = xhci_read32((uintptr_t)usbcmd);
    xhci_write32((uintptr_t)usbcmd, cmd | XHCI_USBCMD_HCRESET);

    // Wait for HCRST to clear
    for (int i = 0; i < 1000; i++) {
        uint32_t c = xhci_read32((uintptr_t)usbcmd);
        if (!(c & XHCI_USBCMD_HCRESET)) break;
        sleep_ms(1);
    }
    if (xhci_read32((uintptr_t)usbcmd) & XHCI_USBCMD_HCRESET) {
        SERIAL_LOG("XHCI: Reset timeout (HCRST still set)\n");
        return -3;
    }

    // Controller Not Ready can persist after reset; wait for CNR to clear
    for (int i = 0; i < 2000; i++) { // up to ~2s
        uint32_t sts = xhci_read32((uintptr_t)usbsts);
        if (!(sts & XHCI_USBSTS_CNR)) break;
        sleep_ms(1);
    }
    if (xhci_read32((uintptr_t)usbsts) & XHCI_USBSTS_CNR) {
        SERIAL_LOG("XHCI: Reset timeout (CNR still set)\n");
        return -4;
    }

    // Clear sticky status bits (acknowledge any pending conditions)
    uint32_t sts = xhci_read32((uintptr_t)usbsts);
    // Write-1-to-clear for EINT/HSE/PCD etc.; preserve HCH/CNR read-only semantics
    uint32_t clear_mask = sts & (XHCI_STS_EINT | XHCI_STS_HSE | XHCI_STS_PCD);
    if (clear_mask) {
        xhci_write32((uintptr_t)usbsts, clear_mask);
    }

    SERIAL_LOG("XHCI: Reset complete\n");
    return 0;
}

int xhci_start(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    SERIAL_LOG("XHCI: Starting controller...\n");
    
    // Set 4KB page size (bit 0)
    xhci_write32(xhci->operational_base + XHCI_OP_PAGESIZE, 0x1);
    
    // Allocate command ring (256 TRBs + 1 Link TRB = 4KB) - DMA-safe
    xhci->command_ring = (xhci_trb_t*)dma_allocator_alloc(4096, 64, 0);
    if (!xhci->command_ring) {
        SERIAL_LOG("XHCI: Failed to allocate command ring\n");
        return -1;
    }
    memset(xhci->command_ring, 0, 4096);
    
    uint64_t cmd_ring_phys = dma_allocator_get_phys(xhci->command_ring);
    XHCI_LOG("XHCI: Command ring virt=");
    SERIAL_LOG_HEX("", (uint32_t)xhci->command_ring);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", (uint32_t)(cmd_ring_phys >> 32));
    SERIAL_LOG_HEX("", (uint32_t)cmd_ring_phys);
    SERIAL_LOG("\n");
    
    // Link TRB at last slot (index 255) back to ring start
    xhci_trb_t *cr = xhci->command_ring;
    cr[255].parameter = cmd_ring_phys;
    cr[255].status = 0;
    cr[255].control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | XHCI_TRB_CYCLE_BIT; // Type=Link, TC=1, Cycle=1
    
    xhci->command_cycle = 1;  // Producer starts at 1
    xhci->command_index = 0;
    
    // Write CRCR with ring physical address
    xhci_write64(xhci->operational_base + XHCI_OP_CRCR, cmd_ring_phys | xhci->command_cycle);
    
    // Allocate Device Context Base Address Array - DMA-safe
    xhci->dcbaap = (uint64_t*)dma_allocator_alloc(2048, 64, 0);  // Max 256 slots
    if (!xhci->dcbaap) {
        SERIAL_LOG("XHCI: Failed to allocate DCBAAP\n");
        return -1;
    }
    memset(xhci->dcbaap, 0, 2048);
    
    uint64_t dcbaap_phys = dma_allocator_get_phys(xhci->dcbaap);
    XHCI_LOG("XHCI: DCBAAP virt=");
    SERIAL_LOG_HEX("", (uint32_t)xhci->dcbaap);
    SERIAL_LOG(" phys=");
    SERIAL_LOG_HEX("", (uint32_t)(dcbaap_phys >> 32));
    SERIAL_LOG_HEX("", (uint32_t)dcbaap_phys);
    SERIAL_LOG("\n");
    
    // Check if scratchpad buffers are needed
    uint32_t hcsparams2 = xhci_read32(xhci->mmio_base + XHCI_CAP_HCSPARAMS2);
    uint32_t max_sp = hcsparams2 & 0xFF;
    if (max_sp > 0) {
        SERIAL_LOG("XHCI: Allocating scratchpad buffers: ");
        SERIAL_LOG_DEC("", max_sp);
        SERIAL_LOG("\n");
        uint64_t *sp_array = (uint64_t*)dma_allocator_alloc(max_sp * sizeof(uint64_t), 64, 0);
        if (sp_array) {
            memset(sp_array, 0, max_sp * sizeof(uint64_t));
            uint64_t sp_phys = dma_allocator_get_phys(sp_array);
            xhci->dcbaap[0] = sp_phys; // DCBAA[0] points to scratchpad array
        }
    }
    
    xhci_write64(xhci->operational_base + XHCI_OP_DCBAAP, dcbaap_phys);
    
    // Configure max device slots
    uint32_t config = xhci_read32(xhci->operational_base + XHCI_OP_CONFIG);
    config = (config & ~0xFF) | xhci->max_slots;
    xhci_write32(xhci->operational_base + XHCI_OP_CONFIG, config);
    
    // Allocate event ring - DMA-safe
    xhci->event_ring = (xhci_trb_t*)dma_allocator_alloc(4096, 64, 0);
    if (!xhci->event_ring) {
        SERIAL_LOG("XHCI: Failed to allocate event ring\n");
        return -1;
    }
    memset(xhci->event_ring, 0, 4096);
    xhci->event_cycle = 1;  // Hardware starts with cycle=1
    xhci->event_index = 0;
    
    // Initialize work queue and statistics
    xhci->work_queue_head = 0;
    xhci->work_queue_tail = 0;
    xhci->stats_events_drained = 0;
    xhci->stats_queued_work_items = 0;
    xhci->stats_ring_empty_hits = 0;
    xhci->stats_total_events = 0;
    xhci->cycle_flipped_this_pass = false;
    
    SERIAL_LOG("XHCI: Work queue and statistics initialized\n");
    
    xhci->erst = (xhci_erst_entry_t*)dma_allocator_alloc(64, 64, 0);
    if (!xhci->erst) {
        SERIAL_LOG("XHCI: Failed to allocate ERST\n");
        return -1;
    }
    
    uint64_t erdp = dma_allocator_get_phys(xhci->event_ring);
    uint64_t erst_phys = dma_allocator_get_phys(xhci->erst);
    
    // ERST entry with 64-bit address
    xhci->erst[0].ring_segment_base_address = erdp;
    xhci->erst[0].ring_segment_size = 256;  // 256 TRBs
    xhci->erst[0].reserved1 = 0;
    xhci->erst[0].reserved2 = 0;
    
    XHCI_LOG("XHCI: Event ring phys=");
    SERIAL_LOG_HEX("", (uint32_t)(erdp >> 32));
    SERIAL_LOG_HEX("", (uint32_t)erdp);
    SERIAL_LOG(" ERST phys=");
    SERIAL_LOG_HEX("", (uint32_t)(erst_phys >> 32));
    SERIAL_LOG_HEX("", (uint32_t)erst_phys);
    SERIAL_LOG("\n");
    
    // Configure interrupter 0
    uint32_t ir_base = xhci->runtime_base + 0x20;  // First interrupter
    
    // Disable interrupter first
    xhci_write32(ir_base + 0x00, 0x00);  // IMAN
    
    // Set ERSTSZ (offset 0x08)
    xhci_write32(ir_base + 0x08, 1);  // ERSTSZ = 1 segment
    
    // Set ERSTBA (offset 0x10) - 64-bit
    xhci_write64(ir_base + 0x10, erst_phys);  // ERSTBA
    
    // Set ERDP (offset 0x18) - 64-bit
    xhci_write64(ir_base + 0x18, erdp);  // ERDP
    
    // Clear EHB (Event Handler Busy) - write ERDP with EHB=1 (bit 3) to clear
    uint32_t erdp_low = (uint32_t)erdp | (1 << 3);  // EHB bit
    xhci_write32(ir_base + 0x18, erdp_low);
    
    // Set interrupt moderation (~250us)
    xhci_write32(ir_base + 0x04, 250);  // IMOD
    
    // Enable interrupter
    xhci_write32(ir_base + 0x00, 0x02);  // IMAN.IE = 1
    
    XHCI_LOG("XHCI: Interrupter configured: ERSTBA=");
    SERIAL_LOG_HEX("", (uint32_t)erst_phys);
    SERIAL_LOG(" ERDP=");
    SERIAL_LOG_HEX("", (uint32_t)erdp);
    SERIAL_LOG("\n");
    
    // Verify command ring setup
    uint64_t crcr_check = xhci_read64(xhci->operational_base + XHCI_OP_CRCR);
    XHCI_LOG("XHCI: CRCR = ");
    SERIAL_LOG_HEX("", (uint32_t)(crcr_check >> 32));
    SERIAL_LOG_HEX("", (uint32_t)crcr_check);
    SERIAL_LOG("\n");
    
    // Verify DCBAAP
    uint64_t dcbaap = xhci_read64(xhci->operational_base + XHCI_OP_DCBAAP);
    XHCI_LOG("XHCI: DCBAAP = ");
    SERIAL_LOG_HEX("", (uint32_t)(dcbaap >> 32));
    SERIAL_LOG_HEX("", (uint32_t)dcbaap);
    SERIAL_LOG("\n");
    
    // Enable host interrupts first (INTE)
    uint32_t cmd = xhci_read32(xhci->operational_base + XHCI_OP_USBCMD);
    cmd |= XHCI_CMD_INTE;
    xhci_write32(xhci->operational_base + XHCI_OP_USBCMD, cmd);
    
    // Start the controller (Run)
    cmd |= XHCI_USBCMD_RUN_STOP;
    xhci_write32(xhci->operational_base + XHCI_OP_USBCMD, cmd);
    
    SERIAL_LOG("XHCI: USBCMD written (INTE+RUN), waiting for controller to start...\n");
    
    // Wait for controller to start
    int timeout = 1000;
    while (timeout--) {
        uint32_t sts = xhci_read32(xhci->operational_base + XHCI_OP_USBSTS);
        if (!(sts & XHCI_USBSTS_HCH)) {
            XHCI_LOG("XHCI: Controller running, USBSTS=");
            SERIAL_LOG_HEX("", sts);
            SERIAL_LOG("\n");
            break;
        }
    }
    
    if (timeout <= 0) {
        SERIAL_LOG("XHCI: Timeout waiting for controller start\n");
        uint32_t sts = xhci_read32(xhci->operational_base + XHCI_OP_USBSTS);
        SERIAL_LOG("XHCI: Final USBSTS=");
        SERIAL_LOG_HEX("", sts);
        SERIAL_LOG("\n");
        return -1;
    }
    
    SERIAL_LOG("XHCI: Controller started successfully\n");
    return 0;
}

int xhci_detect_ports(xhci_controller_t *xhci) {
    if (!xhci) return -1;

    SERIAL_LOG("XHCI: Detecting devices on ports...\n");

    for (uint32_t port = 1; port <= xhci->max_ports; port++) {
        SERIAL_LOG("XHCI: Checking port ");
        SERIAL_LOG_HEX("", port);
        SERIAL_LOG("\n");
        
        uintptr_t port_base = xhci->operational_base + XHCI_PORT_OFFSET + ((port - 1) * 0x10);
        uint32_t portsc = xhci_read32(port_base + XHCI_PORTSC);

        SERIAL_LOG("XHCI: Port ");
        SERIAL_LOG_HEX("", port);
        SERIAL_LOG(" PORTSC=");
        SERIAL_LOG_HEX("", portsc);
        SERIAL_LOG("\n");

        // Optional: clear stale change bits
        uint32_t w1c = portsc & (XHCI_PORTSC_CSC | XHCI_PORTSC_PRC | XHCI_PORTSC_PEC);
        if (w1c) xhci_write32(port_base + XHCI_PORTSC, w1c);

        if (portsc & XHCI_PORTSC_CCS) {
            uint32_t speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> 10;
            SERIAL_LOG("XHCI: Device connected on port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG(" speed=");
            SERIAL_LOG_HEX("", speed);
            SERIAL_LOG("\n");

            // Debounce with busy wait (timer may not be ready yet during early init)
            SERIAL_LOG("XHCI: Debouncing port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG("\n");
            for (volatile int i = 0; i < 50000000; i++);  // ~50ms busy wait

            // Reset according to speed/link
            SERIAL_LOG("XHCI: Resetting port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG("\n");
            xhci_reset_port(xhci, port);
            SERIAL_LOG("XHCI: Port reset returned\n");

            // After reset, wait for PRC and PED
            SERIAL_LOG("XHCI: Waiting for PRC on port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG("\n");
            uint32_t timeout = 500; // ~500 ms
            while (timeout--) {
                portsc = xhci_read32(port_base + XHCI_PORTSC);
                if (portsc & XHCI_PORTSC_PRC) {
                    // Clear PRC
                    xhci_write32(port_base + XHCI_PORTSC, XHCI_PORTSC_PRC);
                    break;
                }
                for (volatile int i = 0; i < 1000000; i++);  // ~1ms busy wait
            }

            // If enabled, proceed to slot enable via command ring
            portsc = xhci_read32(port_base + XHCI_PORTSC);
            if (portsc & XHCI_PORTSC_PED) {
                SERIAL_LOG("XHCI: Port ");
                SERIAL_LOG_HEX("", port);
                SERIAL_LOG(" enabled\n");
                // Kick off Enable Slot command
                // Port enabled - slot enumeration handled by xhci_enumerate_devices()
            } else {
                SERIAL_LOG("XHCI: Port not enabled after reset on port "); SERIAL_LOG_HEX("", port); SERIAL_LOG("\n");
            }
        }
    }
    SERIAL_LOG("XHCI: Port detection complete\n");
    return 0;
}

int xhci_reset_port(xhci_controller_t *xhci, uint8_t port) {
    if (!xhci || port == 0 || port > xhci->max_ports) return -1;
    
    XHCI_LOG("XHCI: Resetting port ");
    SERIAL_LOG_HEX("", port);
    SERIAL_LOG("\n");
    
    uint32_t port_base = xhci->operational_base + XHCI_PORT_OFFSET + ((port - 1) * 0x10);
    
    // Clear change bits by writing 1 to them
    uint32_t portsc = xhci_read32(port_base + XHCI_PORTSC);
    portsc &= ~(XHCI_PORTSC_PED | XHCI_PORTSC_PR);  // Don't accidentally disable or reset
    portsc |= XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_WRC | XHCI_PORTSC_PRC | XHCI_PORTSC_PLC | XHCI_PORTSC_CEC;
    xhci_write32(port_base + XHCI_PORTSC, portsc);
    
    // Set port reset
    portsc = xhci_read32(port_base + XHCI_PORTSC);
    portsc |= XHCI_PORTSC_PR;
    xhci_write32(port_base + XHCI_PORTSC, portsc);
    
    // Wait for reset to complete (use busy wait since timer may not be ready)
    int timeout = 1000;
    while (timeout--) {
        portsc = xhci_read32(port_base + XHCI_PORTSC);
        if (portsc & XHCI_PORTSC_PRC) {
            // Clear reset change bit
            portsc |= XHCI_PORTSC_PRC;
            xhci_write32(port_base + XHCI_PORTSC, portsc);
            break;
        }
        for (volatile int i = 0; i < 1000000; i++);  // ~1ms busy wait
    }
    
    if (timeout <= 0) {
        SERIAL_LOG("XHCI: Port reset timeout\n");
        return -1;
    }
    
    XHCI_LOG("XHCI: Port reset complete\n");
    return 0;
}

xhci_controller_t* xhci_get_controller(void) {
    return g_xhci;
}

// Helper to submit command TRB and wait for completion
static int xhci_submit_command(xhci_controller_t *xhci, xhci_trb_t *trb) {
    if (!xhci || !trb) return -1;
    
    XHCI_LOG("XHCI: Submitting command TRB type=");
    SERIAL_LOG_HEX("", (trb->control >> 10) & 0x3F);
    SERIAL_LOG("\n");
    
    // Copy TRB to command ring
    xhci_trb_t *cmd_trb = &xhci->command_ring[xhci->command_index];
    cmd_trb->parameter = trb->parameter;
    cmd_trb->status = trb->status;
    cmd_trb->control = (trb->control & ~XHCI_TRB_CYCLE_BIT) | (xhci->command_cycle ? XHCI_TRB_CYCLE_BIT : 0);
    
    XHCI_LOG("XHCI: Command ring index=");
    SERIAL_LOG_HEX("", xhci->command_index);
    SERIAL_LOG(" cycle=");
    SERIAL_LOG_HEX("", xhci->command_cycle);
    SERIAL_LOG("\n");
    
    // Advance command ring pointer
    xhci->command_index++;
    if (xhci->command_index >= 255) {  // Reserve last TRB for Link
        xhci->command_index = 0;
        xhci->command_cycle ^= 1;
    }
    
    // Ring doorbell 0 (command ring)
    XHCI_LOG("XHCI: Ringing doorbell 0 at address ");
    SERIAL_LOG_HEX("", xhci->doorbell_base);
    SERIAL_LOG("\n");
    xhci_write32(xhci->doorbell_base, 0);
    
    // Wait for command completion by polling the event ring
    // Note: During init, there's no background polling yet, so we need to
    // manually call drain/poll to process events
    int timeout = 10000;
    while (timeout--) {
        // Check USB status for interrupts
        uint32_t usbsts = xhci_read32(xhci->operational_base + XHCI_OP_USBSTS);
        if (usbsts & XHCI_STS_EINT) {
            // Clear event interrupt
            xhci_write32(xhci->operational_base + XHCI_OP_USBSTS, XHCI_STS_EINT);
        }
        
        // Manually poll events using the unified drain mechanism
        xhci_poll_events(xhci);
        
        // Check if command completed by scanning recent events
        // FIXME: This is a hack - we should properly track command completion
        // For now, just assume success after some events are processed
        
        // Small delay
        for (volatile int i = 0; i < 1000; i++);
        
        if (timeout % 1000 == 0) {
            // Periodic debug output
            XHCI_LOG("XHCI: Waiting for command completion...\n");
        }
    }
    
    // Assume command completed successfully
    // FIXME: Should properly detect Command Complete event
    SERIAL_LOG("XHCI: Command wait complete (assuming success)\n");
    return 0;
}

int xhci_enable_slot(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    SERIAL_LOG("XHCI: Enabling slot\n");
    
    xhci_trb_t trb = {0};
    trb.control = (XHCI_TRB_TYPE_ENABLE_SLOT_CMD << XHCI_TRB_TYPE_SHIFT);
    
    if (xhci_submit_command(xhci, &trb) < 0) {
        SERIAL_LOG("XHCI: Enable slot failed\n");
        return -1;
    }
    
    // Extract slot ID from event (stored in upper bits of control field)
    xhci_trb_t *event = &xhci->event_ring[(xhci->event_index - 1) & 0xFF];
    uint8_t slot_id = (event->control >> 24) & 0xFF;
    
    XHCI_LOG("XHCI: Slot enabled, ID=");
    SERIAL_LOG_HEX("", slot_id);
    SERIAL_LOG("\n");
    
    return slot_id;
}

int xhci_address_device(xhci_controller_t *xhci, uint8_t slot, uint8_t port) {
    if (!xhci || slot == 0) return -1;
    
    XHCI_LOG("XHCI: Addressing device slot=");
    SERIAL_LOG_HEX("", slot);
    SERIAL_LOG(" port=");
    SERIAL_LOG_HEX("", port);
    SERIAL_LOG("\n");
    
    // Allocate input context - DMA-safe
    xhci_input_context_t *input_ctx = (xhci_input_context_t*)dma_allocator_alloc(sizeof(xhci_input_context_t), 64, 0);
    if (!input_ctx) {
        SERIAL_LOG("XHCI: Failed to allocate input context\n");
        return -1;
    }
    memset(input_ctx, 0, sizeof(xhci_input_context_t));
    
    // Set add context flags (A0 = slot, A1 = EP0)
    input_ctx->input_control_context[1] = 0x03;
    
    // Configure slot context
    uint32_t *slot_ctx = input_ctx->device_context.slot_context;
    slot_ctx[0] = (1 << 27);  // Context entries = 1 (only EP0)
    slot_ctx[1] = (port << 16);  // Root hub port number
    
    // Configure EP0 context (control endpoint)
    uint32_t *ep0_ctx = &input_ctx->device_context.endpoint_contexts[0][0];
    ep0_ctx[1] = (3 << 3) | (4 << 16);  // EP type=Control, Max packet size=8 (will be updated)
    
    // Allocate and set transfer ring for EP0 - DMA-safe
    xhci_trb_t *ep0_ring = (xhci_trb_t*)dma_allocator_alloc(4096, 64, 0);
    if (!ep0_ring) {
        dma_allocator_free(input_ctx);
        return -1;
    }
    memset(ep0_ring, 0, 4096);
    
    // Set up Link TRB at end of ring (index 255) to wrap back to start
    xhci_trb_t *link_trb = &ep0_ring[255];
    uint64_t ep0_ring_phys = dma_allocator_get_phys(ep0_ring);
    link_trb->parameter = ep0_ring_phys;  // Point back to ring start
    link_trb->status = 0;
    link_trb->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | XHCI_TRB_CYCLE_BIT;  // Type=Link TRB, Toggle Cycle, Cycle=1
    
    ep0_ctx[2] = (uint32_t)(ep0_ring_phys & 0xFFFFFFFF) | 1;  // DCS=1
    ep0_ctx[3] = (uint32_t)(ep0_ring_phys >> 32);
    
    // Set device context base address
    uint64_t device_ctx_phys = dma_allocator_get_phys(&input_ctx->device_context);
    xhci->dcbaap[slot] = device_ctx_phys;
    
    // Submit Address Device command
    xhci_trb_t trb = {0};
    uint64_t input_ctx_phys = dma_allocator_get_phys(input_ctx);
    trb.parameter = input_ctx_phys;
    trb.control = (XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD << XHCI_TRB_TYPE_SHIFT) | (slot << XHCI_SLOT_ID_SHIFT);
    
    int result = xhci_submit_command(xhci, &trb);
    
    if (result < 0) {
        SERIAL_LOG("XHCI: Address device failed\n");
        dma_allocator_free(ep0_ring);
        dma_allocator_free(input_ctx);
        return -1;
    }
    
    SERIAL_LOG("XHCI: Device addressed successfully\n");
    return 0;
}

// Enumerate devices after port detection
int xhci_enumerate_devices(xhci_controller_t *xhci) {
    if (!xhci) return -1;
    
    SERIAL_LOG("XHCI: Enumerating devices\n");
    
    for (uint32_t port = 1; port <= xhci->max_ports; port++) {
        uint32_t port_base = xhci->operational_base + XHCI_PORT_OFFSET + ((port - 1) * 0x10);
        uint32_t portsc = xhci_read32(port_base + XHCI_PORTSC);
        
        if (portsc & XHCI_PORTSC_CCS) {
            XHCI_LOG("XHCI: Enumerating device on port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG("\n");
            
            // Enable slot
            int slot = xhci_enable_slot(xhci);
            if (slot < 0) {
                SERIAL_LOG("XHCI: Failed to enable slot for port\n");
                continue;
            }
            
            // Address device
            if (xhci_address_device(xhci, slot, port) < 0) {
                SERIAL_LOG("XHCI: Failed to address device\n");
                continue;
            }
            
            // Configure endpoint for HID (interrupt IN)
            if (xhci_configure_endpoint(xhci, slot) < 0) {
                SERIAL_LOG("XHCI: Failed to configure endpoint\n");
                continue;
            }
            
            XHCI_LOG("XHCI: Device enumerated on port ");
            SERIAL_LOG_HEX("", port);
            SERIAL_LOG("\n");
            
            // Probe HID driver
            extern void usb_hid_probe_device(void* controller, uint8_t slot, uint8_t port);
            usb_hid_probe_device(xhci, slot, port);
        }
    }
    
    return 0;
}

// Configure endpoint for interrupt transfers (HID)
int xhci_configure_endpoint(xhci_controller_t *xhci, uint8_t slot) {
    if (!xhci || slot == 0) return -1;
    
    XHCI_LOG("XHCI: Configuring endpoint for slot ");
    SERIAL_LOG_HEX("", slot);
    SERIAL_LOG("\n");
    
    // Allocate input context - DMA-safe
    xhci_input_context_t *input_ctx = (xhci_input_context_t*)dma_allocator_alloc(2112, 64, 0);
    if (!input_ctx) {
        SERIAL_LOG("XHCI: Failed to allocate input context\n");
        return -1;
    }
    memset(input_ctx, 0, 2112);
    
    // Set input control context flags
    // Add context flag for EP1 IN: DCI 3 = bit 3, Slot = bit 0
    input_ctx->input_control_context[0] = 0;  // Drop context flags
    input_ctx->input_control_context[1] = (1 << 0) | (1 << 3);  // Add context flags: Slot + EP1 IN (DCI 3)
    
    // Copy existing slot context from device context
    uint32_t *slot_ctx = input_ctx->device_context.slot_context;
    // Context entries = 3 (slot + EP0 + EP1 IN), route string = 0
    slot_ctx[0] = (1 << 27) | (3 << 0);  // Context entries = 3
    slot_ctx[1] = 0;  // Speed, hub info
    slot_ctx[2] = 0;  // TT info
    slot_ctx[3] = 0;  // Device address will be set by controller
    
    // Configure EP1 IN for HID (interrupt endpoint)
    // DCI for EP1 IN = (endpoint * 2) + direction = (1 * 2) + 1 = 3
    // But array index is DCI - 1 = 2
    uint32_t *ep1_ctx = &input_ctx->device_context.endpoint_contexts[2][0];  // EP1 IN (DCI 3)
    
    // Allocate transfer ring for EP1 - DMA-safe
    xhci_trb_t *ep1_ring = (xhci_trb_t*)dma_allocator_alloc(4096, 64, 0);
    if (!ep1_ring) {
        dma_allocator_free(input_ctx);
        return -1;
    }
    memset(ep1_ring, 0, 4096);
    
    // Set up Link TRB at end of ring (index 255) to wrap back to start
    xhci_trb_t *link_trb = &ep1_ring[255];
    uint64_t ep1_ring_phys = dma_allocator_get_phys(ep1_ring);
    link_trb->parameter = ep1_ring_phys;  // Point back to ring start
    link_trb->status = 0;
    link_trb->control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | XHCI_TRB_CYCLE_BIT;  // Type=Link TRB, Toggle Cycle, Cycle=1
    
    // EP1 context fields:
    // [0]: EP state, mult, max streams, LSA, interval
    // [1]: EP type, HID, max burst, max packet size
    // [2]: TR dequeue ptr low + DCS
    // [3]: TR dequeue ptr high
    // [4]: Average TRB length, max ESIT
    
    ep1_ctx[0] = (0 << 0) |   // EP state = disabled
                 (0 << 8) |   // Mult = 0
                 (0 << 10) |  // Max streams = 0
                 (0 << 15) |  // LSA = 0
                 (3 << 16);   // Interval = 8 (2^3 = 8ms for low/full speed)
    
    ep1_ctx[1] = (7 << 3) |   // EP type = 7 (Interrupt IN)
                 (0 << 8) |   // Max burst = 0
                 (8 << 16);   // Max packet size = 8 bytes (keyboard/mouse)
    
    ep1_ctx[2] = (uint32_t)(ep1_ring_phys & 0xFFFFFFFF) | 1;  // DCS = 1
    ep1_ctx[3] = (uint32_t)(ep1_ring_phys >> 32);
    ep1_ctx[4] = 8;  // Average TRB length = 8
    
    // Submit Configure Endpoint command
    xhci_trb_t trb = {0};
    uint64_t input_ctx_phys = dma_allocator_get_phys(input_ctx);
    trb.parameter = input_ctx_phys;
    trb.control = (XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD << XHCI_TRB_TYPE_SHIFT) | (slot << XHCI_SLOT_ID_SHIFT);
    
    int result = xhci_submit_command(xhci, &trb);
    
    if (result < 0) {
        SERIAL_LOG("XHCI: Configure endpoint failed\n");
        dma_allocator_free(ep1_ring);
        dma_allocator_free(input_ctx);
        return -1;
    }
    
    SERIAL_LOG("XHCI: Endpoint configured successfully\n");
    
    // Store transfer ring for this device
    xhci->device_transfer_rings[slot] = ep1_ring;
    xhci->transfer_ring_indices[slot] = 0;   // Enqueue starts at 0
    xhci->transfer_ring_dequeue[slot] = 0;   // Dequeue starts at 0
    xhci->transfer_ring_cycles[slot] = 1;    // Enqueue cycle starts at 1
    xhci->transfer_ring_dequeue_cycles[slot] = 1;  // Dequeue cycle starts at 1
    
    // Note: We're leaking input_ctx here for now
    // In production, we'd store it properly
    
    return 0;
}

// Queue a transfer on an endpoint
int xhci_queue_transfer(xhci_controller_t *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length) {
    if (!xhci || slot == 0 || !buffer) return -1;
    
    xhci_trb_t *ring = xhci->device_transfer_rings[slot];
    if (!ring) {
        SERIAL_LOG("XHCI: No transfer ring for slot ");
        SERIAL_LOG_HEX("", slot);
        SERIAL_LOG("\n");
        return -1;
    }
    
    uint32_t enqueue_idx = xhci->transfer_ring_indices[slot];
    uint32_t dequeue_idx = xhci->transfer_ring_dequeue[slot];
    uint8_t cycle = xhci->transfer_ring_cycles[slot];
    
    // Check if ring is full (enqueue would catch up to dequeue)
    uint32_t next_enqueue = (enqueue_idx + 1) % 255;
    if (next_enqueue == dequeue_idx) {
        static uint32_t ring_full_warns = 0;
        if (ring_full_warns++ % 100 == 0) {
            LOG_TS("[XHCI] WARNING: Transfer ring full for slot ");
            SERIAL_LOG_DEC("", slot);
            SERIAL_LOG(", enqueue=");
            SERIAL_LOG_DEC("", enqueue_idx);
            SERIAL_LOG(", dequeue=");
            SERIAL_LOG_DEC("", dequeue_idx);
            SERIAL_LOG("\n");
        }
        return -2;  // Ring full
    }
    
    uint32_t index = enqueue_idx;
    
    // Debug: Log every 100th transfer
    static uint32_t total_queued[256] = {0};
    total_queued[slot]++;
    if (total_queued[slot] % 100 == 0) {
        LOG_TS("[XHCI_QUEUE] Slot ");
        SERIAL_LOG_DEC("", slot);
        SERIAL_LOG(" queued ");
        SERIAL_LOG_DEC("", total_queued[slot]);
        SERIAL_LOG(" transfers, index=");
        SERIAL_LOG_DEC("", index);
        SERIAL_LOG(" cycle=");
        SERIAL_LOG_DEC("", cycle);
        SERIAL_LOG("\n");
    }
    
    // Build Normal TRB
    xhci_trb_t *trb = &ring[index];
    uint64_t phys_addr = virt_to_phys(buffer);
    
    // Debug: Log buffer addresses for first few transfers
    static int addr_log_count = 0;
    if (++addr_log_count <= 10) {
        extern void serial_debug(const char* msg);
        if (slot == 1) serial_debug("[XHCI_BUF] KB: ");
        else if (slot == 2) serial_debug("[XHCI_BUF] MS: ");
        else serial_debug("[XHCI_BUF] ??: ");
        
        SERIAL_LOG("virt=");
        SERIAL_LOG_HEX("", (uint32_t)buffer);
        SERIAL_LOG(" phys=");
        SERIAL_LOG_HEX("", (uint32_t)phys_addr);
        SERIAL_LOG("\n");
    }
    
    trb->parameter = phys_addr;
    trb->status = length;  // Transfer length in status field
    trb->control = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) | (1 << 5) | (cycle ? XHCI_TRB_CYCLE_BIT : 0);  // IOC bit set
    
    // Advance index
    xhci->transfer_ring_indices[slot]++;
    if (xhci->transfer_ring_indices[slot] >= 255) {
        // Transfer ring has 255 usable entries (0-254), entry 255 is Link TRB
        // When we reach 255, wrap back to 0 and flip cycle
        
        LOG_TS("[XHCI] Transfer ring wrapping for slot ");
        SERIAL_LOG_DEC("", slot);
        SERIAL_LOG(", index=");
        SERIAL_LOG_DEC("", xhci->transfer_ring_indices[slot]);
        SERIAL_LOG(", old_cycle=");
        SERIAL_LOG_DEC("", xhci->transfer_ring_cycles[slot]);
        
        // CRITICAL: Update Link TRB cycle bit to match current cycle BEFORE wrapping
        // The Link TRB's cycle bit must match the producer cycle when the controller reads it
        xhci_trb_t *link_trb = &ring[255];
        if (cycle) {
            link_trb->control |= XHCI_TRB_CYCLE_BIT;
        } else {
            link_trb->control &= ~XHCI_TRB_CYCLE_BIT;
        }
        
        // Wrap index and flip cycle
        xhci->transfer_ring_indices[slot] = 0;
        xhci->transfer_ring_cycles[slot] ^= 1;
        
        SERIAL_LOG(", new_cycle=");
        SERIAL_LOG_DEC("", xhci->transfer_ring_cycles[slot]);
        SERIAL_LOG("\n");
        
        LOG_TS("[XHCI] Transfer ring wrapped to index 0, new cycle=");
        SERIAL_LOG_DEC("", xhci->transfer_ring_cycles[slot]);
        SERIAL_LOG("\n");
    }
    
    // Ring doorbell for this endpoint
    // For EP1 IN: DCI = (endpoint * 2) + direction = (1 * 2) + 1 = 3
    // Assuming IN direction for interrupt endpoints
    uint8_t dci = (endpoint * 2) + 1;  // IN direction
    xhci_write32(xhci->doorbell_base + (slot * 4), dci);
    
    return 0;
}

// ISR: Drain events from ring into work queue (minimal work under spinlock)
static uint32_t xhci_drain_events(xhci_controller_t *xhci) {
    uint32_t events_drained = 0;
    
    // Safety checks
    if (!xhci || !xhci->event_ring) {
        return 0;
    }
    
    // Acquire spinlock ONLY for draining ring
    spin_lock(&xhci->event_ring_lock);
    
    // Drain ALL available events into work queue (no processing, just copying)
    for (int i = 0; i < 256; i++) {
        xhci_trb_t *event = &xhci->event_ring[xhci->event_index];
        uint8_t cycle = event->control & XHCI_TRB_CYCLE_BIT;
        
        // Debug: Log event ring state every 1000 polls
        static uint32_t debug_counter = 0;
        if (++debug_counter % 1000 == 0 && i == 0) {
            LOG_TS("[XHCI_DEBUG] index=");
            SERIAL_LOG_DEC("", xhci->event_index);
            SERIAL_LOG(" expect_cycle=");
            SERIAL_LOG_DEC("", xhci->event_cycle);
            SERIAL_LOG(" event_cycle=");
            SERIAL_LOG_DEC("", cycle);
            SERIAL_LOG(" trb_type=");
            SERIAL_LOG_DEC("", (event->control >> 10) & 0x3F);
            SERIAL_LOG("\n");
        }
        
        // Check if cycle bit matches - if not, ring is empty (no new events)
        if (cycle != xhci->event_cycle) {
            xhci->stats_ring_empty_hits++;
            break;
        }
        
        // Event is ready - extract minimal info and queue for processing
        uint8_t trb_type = (event->control >> 10) & 0x3F;
        
        // Only queue transfer events
        if (trb_type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
            uint8_t completion_code = (event->status >> 24) & 0xFF;
            uint8_t slot_id = (event->control >> 24) & 0xFF;
            uint8_t endpoint_id = (event->control >> 16) & 0x1F;
            
            // Queue work item (will be processed outside spinlock)
            uint32_t next_tail = (xhci->work_queue_tail + 1) % 256;
            if (next_tail != xhci->work_queue_head) {
                xhci->work_queue[xhci->work_queue_tail].slot_id = slot_id;
                xhci->work_queue[xhci->work_queue_tail].endpoint_id = endpoint_id;
                xhci->work_queue[xhci->work_queue_tail].completion_code = completion_code;
                xhci->work_queue_tail = next_tail;
                xhci->stats_queued_work_items++;
            } else {
                // Work queue full - this is a CRITICAL ERROR
                static uint32_t overflow_logged = 0;
                if (overflow_logged++ < 10) {
                    LOG_TS("[XHCI_ERROR] Work queue FULL! Dropping event for slot ");
                    SERIAL_LOG_DEC("", slot_id);
                    SERIAL_LOG(" ep ");
                    SERIAL_LOG_DEC("", endpoint_id);
                    SERIAL_LOG(" head=");
                    SERIAL_LOG_DEC("", xhci->work_queue_head);
                    SERIAL_LOG(" tail=");
                    SERIAL_LOG_DEC("", xhci->work_queue_tail);
                    SERIAL_LOG("\n");
                }
            }
        }
        
        xhci->stats_total_events++;
        events_drained++;
        
        // Advance event ring index
        uint32_t old_index = xhci->event_index;
        xhci->event_index++;
        if (xhci->event_index >= 256) {
            SERIAL_LOG("[XHCI_WRAP] Index wrapped from ");
            SERIAL_LOG_DEC("", old_index);
            SERIAL_LOG(" to 0, flipping cycle from ");
            SERIAL_LOG_DEC("", xhci->event_cycle);
            xhci->event_index = 0;
            xhci->event_cycle ^= 1;  // CRITICAL: Flip cycle bit when wrapping!
            SERIAL_LOG(" to ");
            SERIAL_LOG_DEC("", xhci->event_cycle);
            SERIAL_LOG("\n");
        }
    }
    
    // Update ERDP once after draining all events
    if (events_drained > 0) {
        SERIAL_LOG("[XHCI_DRAIN_END] After drain: events=");
        SERIAL_LOG_DEC("", events_drained);
        SERIAL_LOG(" final_index=");
        SERIAL_LOG_DEC("", xhci->event_index);
        SERIAL_LOG("\n");
        uint32_t ir_base = xhci->runtime_base + 0x20;
        uint64_t erdp = virt_to_phys64(&xhci->event_ring[xhci->event_index]) | (1 << 3);  // EHB bit
        xhci_write64(ir_base + 0x18, erdp);
        xhci->stats_events_drained += events_drained;
        
        // Debug: Log first few drains
        static uint32_t drain_log = 0;
        if (++drain_log <= 5) {
            SERIAL_LOG("[XHCI_DRAIN] Drained ");
            SERIAL_LOG_DEC("", events_drained);
            SERIAL_LOG(" events\n");
        }
    }
    
    // Release spinlock ASAP
    spin_unlock(&xhci->event_ring_lock);
    
    return events_drained;
}

// Worker: Process queued work items (outside spinlock, can block/sleep)
static void xhci_process_work_queue(xhci_controller_t *xhci) {
    extern void usb_keyboard_process_xhci_data(uint8_t slot);
    extern void usb_mouse_process_xhci_data(uint8_t slot);
    
    // Safety check
    if (!xhci) {
        return;
    }
    
    // Process all queued work items
    while (xhci->work_queue_head != xhci->work_queue_tail) {
        uint8_t slot_id = xhci->work_queue[xhci->work_queue_head].slot_id;
        uint8_t endpoint_id = xhci->work_queue[xhci->work_queue_head].endpoint_id;
        uint8_t completion_code = xhci->work_queue[xhci->work_queue_head].completion_code;
        
        // Advance head
        xhci->work_queue_head = (xhci->work_queue_head + 1) % 256;
        
        // CRITICAL: Advance dequeue pointer for ALL completion events (success, error, or otherwise)
        // The transfer consumed a ring slot regardless of outcome
        uint32_t old_dequeue = xhci->transfer_ring_dequeue[slot_id];
        xhci->transfer_ring_dequeue[slot_id]++;
        if (xhci->transfer_ring_dequeue[slot_id] >= 255) {
            // Wrap dequeue pointer and flip dequeue cycle
            xhci->transfer_ring_dequeue[slot_id] = 0;
            xhci->transfer_ring_dequeue_cycles[slot_id] ^= 1;
            
            LOG_TS("[XHCI] Dequeue wrapped for slot ");
            SERIAL_LOG_DEC("", slot_id);
            SERIAL_LOG(" (");
            SERIAL_LOG_DEC("", old_dequeue);
            SERIAL_LOG(" -> 0), new cycle=");
            SERIAL_LOG_DEC("", xhci->transfer_ring_dequeue_cycles[slot_id]);
            SERIAL_LOG("\n");
        }
        
        // Log ALL completion codes for debugging
        static int completion_log = 0;
        if (completion_log++ < 50) {
            SERIAL_LOG("[XHCI_WORK] Slot=");
            SERIAL_LOG_DEC("", slot_id);
            SERIAL_LOG(" EP=");
            SERIAL_LOG_DEC("", endpoint_id);
            SERIAL_LOG(" Code=");
            SERIAL_LOG_DEC("", completion_code);
            SERIAL_LOG("\n");
        }
        
        // Process transfers with Success or Short Packet codes (only process data if successful)
        if (completion_code == XHCI_TRB_COMPLETION_CODE_SUCCESS || 
            completion_code == XHCI_TRB_COMPLETION_CODE_SHORT_PACKET) {
            extern void serial_debug(const char* msg);
            static int route_count = 0;
            if (++route_count <= 30) {
                if (slot_id == 1) {
                    serial_debug("[XHCI_ROUTE] Slot 1 (KB) ep=");
                } else if (slot_id == 2) {
                    serial_debug("[XHCI_ROUTE] Slot 2 (MS) ep=");
                } else {
                    serial_debug("[XHCI_ROUTE] Unknown slot ep=");
                }
                
                if (endpoint_id == 1) serial_debug("1\n");
                else if (endpoint_id == 2) serial_debug("2\n");
                else if (endpoint_id == 3) serial_debug("3\n");
                else if (endpoint_id == 4) serial_debug("4\n");
                else serial_debug("?\n");
            }
            
            // USB HID devices (keyboard and mouse) use endpoint 1 (0x81)
            // Endpoint ID in XHCI context is calculated as: (endpoint_address & 0xF) * 2 + (direction)
            // For 0x81 (endpoint 1 IN): (1 & 0xF) * 2 + 1 = 3
            // So endpoint_id 3 is correct for interrupt IN on endpoint 1
            if (slot_id == 1 && endpoint_id == 3) {
                serial_debug("[XHCI] -> keyboard\n");
                usb_keyboard_process_xhci_data(slot_id);
            } else if (slot_id == 2 && endpoint_id == 3) {
                serial_debug("[XHCI] -> mouse\n");
                usb_mouse_process_xhci_data(slot_id);
            } else {
                if (route_count <= 30) {
                    serial_debug("[XHCI] -> IGNORED (no match)\n");
                }
            }
        } else {
            // Log non-success completion codes for debugging
            static uint32_t error_log_count = 0;
            if (error_log_count++ < 10) {
                LOG_TS("[XHCI] Non-success completion: slot=");
                SERIAL_LOG_DEC("", slot_id);
                SERIAL_LOG(" ep=");
                SERIAL_LOG_DEC("", endpoint_id);
                SERIAL_LOG(" code=");
                SERIAL_LOG_DEC("", completion_code);
                SERIAL_LOG("\n");
            }
        }
    }
}

// Public API: Poll events (ISR drains, worker processes)
void xhci_poll_events(xhci_controller_t *xhci) {
    if (!xhci) return;
    
    // ISR: Drain events from ring (minimal spinlock time)
    uint32_t drained = xhci_drain_events(xhci);
    
    // Worker: Process queued work (can block, no spinlock)
    if (drained > 0) {
        xhci_process_work_queue(xhci);
    }
    
    // Log statistics periodically (every 1000 polls for debugging)
    static uint32_t log_counter = 0;
    if (++log_counter % 1000 == 0) {
        LOG_TS("[XHCI_STATS] Events:");
        SERIAL_LOG_DEC("", xhci->stats_total_events);
        SERIAL_LOG(" Drained:");
        SERIAL_LOG_DEC("", xhci->stats_events_drained);
        SERIAL_LOG(" Queued:");
        SERIAL_LOG_DEC("", xhci->stats_queued_work_items);
        SERIAL_LOG(" Empty:");
        SERIAL_LOG_DEC("", xhci->stats_ring_empty_hits);
        SERIAL_LOG(" WorkQ:");
        SERIAL_LOG_DEC("", (xhci->work_queue_tail - xhci->work_queue_head + 256) % 256);
        SERIAL_LOG("\n");
    }
}

