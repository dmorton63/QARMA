#include "uhci.h"
#include "usb.h"
#include "core/memory/heap.h"
#include "core/timer.h"
#include "core/memory/vmm/vmm.h"
#include "core/memory/pmm/pmm.h"
#include "graphics/graphics.h"
#include "config.h"
#include "core/pci.h"

/* Debug helper: when enabled, force an immediate diagnostic dump after
 * enqueueing descriptors so emulator captures contain controller-visible
 * state without waiting for the normal timeout. Temporary; remove when
 * diagnostics are complete. */
#ifndef UHCI_FORCE_DUMP_AFTER_ENQUEUE
#define UHCI_FORCE_DUMP_AFTER_ENQUEUE 1
#endif

// Prototype for vaddr -> phys helper (defined later)
static inline uint32_t vaddr_to_phys(void *vaddr);
/* Forward I/O prototypes (definitions appear later) */
static inline uint16_t uhci_inw(uint16_t port);
static inline void uhci_outw(uint16_t port, uint16_t value);

/* Port manipulation helpers that properly handle RWC (Read-Write-Clear) bits.
 * RWC bits (CSC, PEC) are cleared when you write 1 to them. */
static inline void uhci_port_set_bits(uint16_t port_reg, uint16_t bits) {
    uint16_t status = uhci_inw(port_reg);
    status &= ~UHCI_PORT_RWC;  // Preserve RWC bits (don't clear them)
    status |= bits;             // Set the desired bits
    uhci_outw(port_reg, status);
}

static inline void uhci_port_clr_bits(uint16_t port_reg, uint16_t bits) {
    uint16_t status = uhci_inw(port_reg);
    status &= ~UHCI_PORT_RWC;  // Preserve RWC bits (don't clear them)
    status &= ~bits;            // Clear the desired bits
    status |= (UHCI_PORT_RWC & bits);  // If clearing RWC bits, set them to clear
    uhci_outw(port_reg, status);
}

// Print a compact timestamp (system tick count) for UHCI diagnostics
static inline void uhci_log_ts(void) {
    SERIAL_LOG_HEX("UHCI: ticks=", get_ticks());
}

/* Detect devices on root hub ports and perform reset/enable sequence
 * We keep this small and observable for diagnostics; the reset/enable
 * helpers already log before/after values and perform readbacks. */
void uhci_detect_ports(uhci_controller_t *uhci) {
    if (!uhci) return;

    uint16_t port1_status = uhci_inw(uhci->io_base + UHCI_PORTSC1);
    if (port1_status & UHCI_PORT_CCS) {
        GFX_LOG_MIN("UHCI: Device connected on port 1\n");
        SERIAL_LOG_HEX("UHCI: Port 1 status: ", port1_status);
        uhci_reset_port(uhci, 1);
        uhci_enable_port(uhci, 1);
    }

    uint16_t port2_status = uhci_inw(uhci->io_base + UHCI_PORTSC2);
    if (port2_status & UHCI_PORT_CCS) {
        GFX_LOG_MIN("UHCI: Device connected on port 2\n");
        SERIAL_LOG_HEX("UHCI: Port 2 status: ", port2_status);
        uhci_reset_port(uhci, 2);
        uhci_enable_port(uhci, 2);
    }
}

int uhci_pci_init(void) {
    SERIAL_LOG("UHCI: Scanning PCI bus for UHCI controllers\n");

    for (int bus = 0; bus < 2; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                uint16_t vendor = pci_read_config_word(bus, slot, func, 0x00);
                if (vendor == 0xFFFF) continue;

                uint8_t class_code = pci_read_config_word(bus, slot, func, 0x0A) >> 8;
                uint8_t subclass   = pci_read_config_word(bus, slot, func, 0x0A) & 0xFF;
                uint8_t prog_if    = pci_read_config_word(bus, slot, func, 0x08) >> 8;

                // Log all USB controllers found
                if (class_code == 0x0C && subclass == 0x03) {
                    SERIAL_LOG("UHCI: Found USB controller - Bus ");
                    SERIAL_LOG_HEX("", bus);
                    SERIAL_LOG(" Slot ");
                    SERIAL_LOG_HEX("", slot);
                    SERIAL_LOG(" Func ");
                    SERIAL_LOG_HEX("", func);
                    SERIAL_LOG(" ProgIF ");
                    SERIAL_LOG_HEX("", prog_if);
                    SERIAL_LOG("\n");
                }

                if (class_code == 0x0C && subclass == 0x03 && (prog_if == 0x00 || prog_if == 0x01)) {
                    uint32_t bar4 = pci_read_config_dword(bus, slot, func, 0x20);
                    uint16_t io_base = bar4 & 0xFFF0;

                    SERIAL_LOG("UHCI: Attempting to initialize controller at I/O base ");
                    SERIAL_LOG_HEX("", io_base);
                    SERIAL_LOG("\n");

                    if (uhci_init_controller(bus, slot, func, io_base) == 0) {
                        SERIAL_LOG("UHCI: Controller initialized successfully\n");
                    } else {
                        SERIAL_LOG("UHCI: Controller initialization failed\n");
                    }
                }
            }
        }
    }

    SERIAL_LOG("UHCI: Scan complete, found ");
    SERIAL_LOG_DEC("", g_uhci_count);
    SERIAL_LOG(" controllers\n");

    return g_uhci_count;
}

// Global controller array
static uhci_controller_t uhci_controllers[8]; // Support up to 8 UHCI controllers
uhci_controller_t *g_uhci_controllers = uhci_controllers;
int g_uhci_count = 0;

// I/O port access helpers
static inline void uhci_outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t uhci_inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t uhci_inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void uhci_outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

/* Test toggle: if enabled, schedule TDs directly into the frame list
 * instead of inserting a QH. This helps determine whether the controller
 * will process TDs when referenced directly. Disable for normal ops. */
#ifndef UHCI_TEST_DIRECT_TD
/* Disable direct TD placement into the frame list for this run so we
 * place a proper QH into the frame list. This better matches real
 * UHCI usage and may help the controller process the descriptors. */
#define UHCI_TEST_DIRECT_TD 0
#endif

int uhci_init_controller(uint8_t bus, uint8_t slot, uint8_t func, uint16_t io_base) {
    if (g_uhci_count >= 8) {
        GFX_LOG_MIN("UHCI: Too many controllers, ignoring\n");
        return -1;
    }
    
    uhci_controller_t *uhci = &uhci_controllers[g_uhci_count];
    uhci->io_base = io_base;
    uhci->bus = bus;
    uhci->slot = slot;  
    uhci->func = func;
    
    GFX_LOG_MIN("UHCI: Initializing controller at I/O base ");
    GFX_LOG_HEX("", io_base);
    GFX_LOG_MIN("\n");
    
    // Reset the controller
    uhci_reset_controller(uhci);
    
    // Allocate frame list in low physical memory (identity-mapped region)
    // Use pmm_alloc_page() so we can place the frame list at a low physical
    // page which is already identity-mapped by vmm_init. This helps rule out
    // DMA address windowing issues in the UHCI emulation/hardware.
    uint32_t fl_phys_page = pmm_alloc_page();
    if (fl_phys_page == 0) {
        GFX_LOG_MIN("UHCI: Failed to allocate physical page for frame list\n");
        return -1;
    }
    uhci->frame_list = (uint32_t *)((uintptr_t)fl_phys_page); // identity mapped virtual == physical
    // Initialize frame list to terminate (all entries point to nothing)
    for (int i = 0; i < 1024; i++) {
        uhci->frame_list[i] = 1; // UHCI_TD_TERMINATE
    }
    /* UHCI requires the Frame List Base to be a page-aligned physical address */
    uint32_t fl_base_page = fl_phys_page & ~0xFFFU;
    uhci_outl(uhci->io_base + UHCI_FLBASEADD, fl_base_page);

    /* Allocate TD and QH pools in low physical pages as well so descriptors
     * live in identity-mapped low memory. Allocate one page for each pool
     * (more than enough: 4KB can hold 64 TDs or 16 QHs comfortably). */
    uint32_t td_pool_phys = pmm_alloc_page();
    uint32_t qh_pool_phys = pmm_alloc_page();
    if (!td_pool_phys || !qh_pool_phys) {
        GFX_LOG_MIN("UHCI: Failed to allocate physical pages for TD/QH pools\n");
        return -1;
    }
    uhci->td_pool = (uhci_td_t *)((uintptr_t)td_pool_phys);
    uhci->qh_pool = (uhci_qh_t *)((uintptr_t)qh_pool_phys);
    
    // Initialize pools with 0xFFFFFFFF as free marker (not 1, which conflicts with TD_PTR_TERMINATE)
    for (int i = 0; i < 64; i++) {
        uhci->td_pool[i].hw.link_ptr = 0xFFFFFFFF; // Mark as free
        uhci->td_pool[i].link_ptr = 0xFFFFFFFF;    // Also set software link_ptr
    }
    for (int i = 0; i < 16; i++) {
        uhci->qh_pool[i].link_ptr = 0xFFFFFFFF; // Mark as free  
    }
    
    // Start the controller
    if (uhci_start_controller(uhci) != 0) {
        GFX_LOG_MIN("UHCI: Failed to start controller\n");
        return -1;
    }
    
    // Detect connected devices
    uhci_detect_ports(uhci);
    
    g_uhci_count++;
    GFX_LOG_MIN("UHCI: Controller initialized successfully\n");
    return 0;
}

// Helper: convert kernel virtual pointer to full physical address (page base + offset)
static inline uint32_t vaddr_to_phys(void *vaddr) {
    if (!vaddr) return 0;
    uint32_t v = (uint32_t)vaddr;
    uint32_t base = vmm_get_physical_address(v);
    if (base == 0) return 0;
    return base + (v & 0xFFF);
}

void uhci_reset_controller(uhci_controller_t *uhci) {
    GFX_LOG_MIN("UHCI: Resetting controller\n");
    
    // Stop the controller
    uhci_outw(uhci->io_base + UHCI_USBCMD, 0);
    
    // Wait for halt
    int timeout = 1000;
    while (!(uhci_inw(uhci->io_base + UHCI_USBSTS) & UHCI_STS_HCH) && timeout--) {
        // Small delay - in real OS would use proper timer
        for (volatile int i = 0; i < 1000; i++);
    }
    
    // Global reset
    uhci_outw(uhci->io_base + UHCI_USBCMD, UHCI_CMD_GRESET);
    
    // Wait 50ms for reset (simplified delay)
    for (volatile int i = 0; i < 50000; i++);
    
    // Clear global reset
    uhci_outw(uhci->io_base + UHCI_USBCMD, 0);
    
    // Host controller reset
    uhci_outw(uhci->io_base + UHCI_USBCMD, UHCI_CMD_HCRESET);
    
    // Wait for reset to complete
    timeout = 1000;
    while ((uhci_inw(uhci->io_base + UHCI_USBCMD) & UHCI_CMD_HCRESET) && timeout--) {
        for (volatile int i = 0; i < 1000; i++);
    }
    
    // Clear status register
    uhci_outw(uhci->io_base + UHCI_USBSTS, 0xFFFF);
}

int uhci_start_controller(uhci_controller_t *uhci) {
    GFX_LOG_MIN("UHCI: Starting controller\n");
    
    // Set frame number to 0
    uhci_outw(uhci->io_base + UHCI_FRNUM, 0);
    
    // Enable interrupts
    uhci_outw(uhci->io_base + UHCI_USBINTR,
              UHCI_STS_USBINT | UHCI_STS_ERROR | UHCI_STS_RD | UHCI_STS_HSE | UHCI_STS_HCPE);

    /* Start the host controller (set Run/Stop) */
    uhci_outw(uhci->io_base + UHCI_USBCMD, UHCI_CMD_RS);

    /* Wait for the controller to clear the HCH (Host Controller Halt) bit */
    int timeout = 1000;
    while ((uhci_inw(uhci->io_base + UHCI_USBSTS) & UHCI_STS_HCH) && timeout--) {
        for (volatile int i = 0; i < 1000; i++);
    }

    if (timeout <= 0) {
        GFX_LOG_MIN("UHCI: Controller failed to start\n");
        return -1;
    }

    GFX_LOG_MIN("UHCI: Controller started successfully\n");
    return 0;
}

uhci_td_t *uhci_alloc_td(uhci_controller_t *uhci) {
    SERIAL_LOG("UHCI: CRASH TEST TD-1 - uhci_alloc_td entry\n");
    
    if (!uhci) {
        SERIAL_LOG("UHCI: CRASH TEST TD-2 - NULL uhci controller\n");
        return NULL;
    }
    
    SERIAL_LOG("UHCI: CRASH TEST TD-3 - Before TD pool search\n");
    for (int i = 0; i < 64; i++) {
        SERIAL_LOG("UHCI: CRASH TEST TD-4 - Checking TD pool entry\n");
        if (uhci->td_pool[i].hw.link_ptr == 0xFFFFFFFF) { // Free marker
            SERIAL_LOG("UHCI: CRASH TEST TD-5 - Found free TD, clearing\n");
            uhci->td_pool[i].hw.link_ptr = 0;
            uhci->td_pool[i].hw.control = 0;
            uhci->td_pool[i].hw.token = 0;
            uhci->td_pool[i].hw.buffer = 0;
            uhci->td_pool[i].next = NULL;
            uhci->td_pool[i].callback = NULL;
            uhci->td_pool[i].callback_data = NULL;
            SERIAL_LOG("UHCI: CRASH TEST TD-6 - TD cleared, returning\n");
            return &uhci->td_pool[i];
        }
    }
    SERIAL_LOG("UHCI: CRASH TEST TD-7 - TD pool exhausted\n");
    return NULL; // Pool exhausted
}

void uhci_free_td(uhci_controller_t *uhci, uhci_td_t *td) {
    if (td >= uhci->td_pool && td < uhci->td_pool + 64) {
        td->hw.link_ptr = 0xFFFFFFFF; // Mark as free with unique value
        td->link_ptr = 0xFFFFFFFF;    // Also mark software link_ptr for consistency
    }
}

uhci_qh_t *uhci_alloc_qh(uhci_controller_t *uhci) {
    for (int i = 0; i < 16; i++) {
        if (uhci->qh_pool[i].link_ptr == 0xFFFFFFFF) { // Free marker
            uhci->qh_pool[i].link_ptr = 1; // Terminate
            uhci->qh_pool[i].element_ptr = 1; // Terminate
            uhci->qh_pool[i].next = NULL;
            uhci->qh_pool[i].first_td = NULL;
            return &uhci->qh_pool[i];
        }
    }
    return NULL; // Pool exhausted
}

void uhci_free_qh(uhci_controller_t *uhci, uhci_qh_t *qh) {
    if (qh >= uhci->qh_pool && qh < uhci->qh_pool + 16) {
        qh->link_ptr = 0xFFFFFFFF; // Mark as free with unique value
    }
}

// Helper function to clean up allocated TDs
static void uhci_cleanup_control_tds(uhci_controller_t *uhci, uhci_td_t *setup_td, 
                                    uhci_td_t *data_td, uhci_td_t *status_td) {
    if (setup_td) uhci_free_td(uhci, setup_td);
    // Free a chain of data TDs if present
    while (data_td) {
        uhci_td_t *next = data_td->next;
        uhci_free_td(uhci, data_td);
        data_td = next;
    }
    if (status_td) uhci_free_td(uhci, status_td);
}

// Helper function to setup TD common fields
static void uhci_setup_td_common(uhci_td_t *td, usb_device_t *device, uint32_t control_flags) {
    /* Do not set ACTIVE here — activation must be the last visible write
     * so the controller never sees a partially-initialized TD. The ACTIVE
     * bit will be set after the TD/QH/frame_list updates with an
     * ordered store (see uhci_td_activate_chain). */
    td->hw.control = (3 << 27) | control_flags; // 3 errors + flags
    
    // Add low-speed flag for low-speed devices
    if (device->speed == USB_SPEED_LOW) {
        td->hw.control |= UHCI_TD_LS;
    }
}

/* Default compile-time CLFLUSH behavior. Can be overridden at build time. */
#ifndef UHCI_USE_CLFLUSH
#define UHCI_USE_CLFLUSH 0
#endif

/* Runtime toggle to enable CLFLUSH; set to 1 for diagnostic runs where the
 * emulator or hardware requires explicit cache line flushes for DMA
 * visibility. Change to 0 to disable. */
static int uhci_enable_clflush = 0; /* disabled to prevent hangs */

/* Allow external code (boot parser) to enable/disable CLFLUSH at boot */
void uhci_set_clflush_enabled(int enabled) {
    uhci_enable_clflush = enabled ? 1 : 0;
    SERIAL_LOG_HEX("UHCI: CLFLUSH runtime set to=", uhci_enable_clflush);
}

/* Ensure descriptor stores are visible to the host and then set the
 * ACTIVE bit on the provided TD chain. This uses an MFENCE to order
 * stores, does a PCI posted-write drain by reading USBSTS, and then
 * sets the ACTIVE bit on each TD. */
static void uhci_td_activate_chain(uhci_controller_t *uhci, uhci_qh_t *qh, uhci_td_t *setup_td,
                                   uhci_td_t *data_head, uhci_td_t *status_td)
{
    /* Optionally flush CPU cache lines for the descriptor memory so that
     * a bus-mastering UHCI controller reads fresh data. Some emulators/hw
     * require explicit cache line flushes on x86. */
    if (uhci_enable_clflush) {
        /* Flush the frame list page (4KB) - step by 64 bytes (typical cacheline) */
        if (uhci->frame_list) {
            uint8_t *fp = (uint8_t *)uhci->frame_list;
            uint8_t * fend = fp + 4096;
            for (; fp < fend; fp += 64) {
                __asm__ volatile ("clflush (%0)" :: "r"(fp) : "memory");
            }
        }

        /* Flush TD/QH descriptors involved */
        if (setup_td) {
            uint8_t *b = (uint8_t *)setup_td;
            uint8_t *end = b + sizeof(uhci_td_t);
            for (; b < end; b += 64) __asm__ volatile ("clflush (%0)" :: "r"(b) : "memory");
        }
        /* Flush QH if provided */
        if (qh) {
            uint8_t *qb = (uint8_t *)qh;
            uint8_t *qend = qb + sizeof(uhci_qh_t);
            for (; qb < qend; qb += 64) __asm__ volatile ("clflush (%0)" :: "r"(qb) : "memory");
        }
        if (status_td) {
            uint8_t *b = (uint8_t *)status_td;
            uint8_t *end = b + sizeof(uhci_td_t);
            for (; b < end; b += 64) __asm__ volatile ("clflush (%0)" :: "r"(b) : "memory");
        }
        uhci_td_t *tt = data_head;
        int flush_count = 0;
        while (tt && flush_count++ < 100) {
            uint8_t *b = (uint8_t *)tt;
            uint8_t *end = b + sizeof(uhci_td_t);
            for (; b < end; b += 64) __asm__ volatile ("clflush (%0)" :: "r"(b) : "memory");
            tt = tt->next;
        }
    }
    /* Order prior stores */
    __asm__ volatile ("mfence" ::: "memory");

    /* Force posted writes to complete by reading a UHCI register */
    (void)uhci_inw(uhci->io_base + UHCI_USBSTS);

    /* Set ACTIVE on setup TD */
    if (setup_td) setup_td->hw.control |= UHCI_TD_ACTIVE;

    /* Walk and activate data TDs */
    uhci_td_t *t = data_head;
    int activate_count = 0;
    while (t && activate_count++ < 100) {
        t->hw.control |= UHCI_TD_ACTIVE;
        t = t->next;
    }

    /* Activate status TD */
    if (status_td) status_td->hw.control |= UHCI_TD_ACTIVE;

    /* Ensure the ACTIVE writes are globally visible */
    __asm__ volatile ("mfence" ::: "memory");
}

// Helper function to calculate max packet size based on device address and speed
static uint16_t uhci_get_control_max_packet_size(usb_device_t *device) {
    // For address 0 (during enumeration), use 8 bytes
    if (device->address == 0) {
        return 8;
    }
    
    // For low-speed devices, max control packet size is 8
    if (device->speed == USB_SPEED_LOW) {
        return 8;
    }
    
    // For full-speed devices, default to 64 (can be 8, 16, 32, or 64)
    return 64;
}

// Helper function to create token field
static uint32_t uhci_create_token(uint8_t pid, uint8_t device_addr, uint8_t endpoint, uint16_t max_len, bool data_toggle) {
    uint32_t token = pid | 
                     ((device_addr & 0x7F) << 8) |
                     ((endpoint & 0xF) << 15) |
                     (((max_len - 1) & 0x7FF) << 21);
    
    if (data_toggle) {
        token |= (1 << 19);  // Set DATA1
    }
    // DATA0 is default (bit 19 = 0)
    
    return token;
}

/* Dump first N bytes (rounded to dwords) of a virtual buffer alongside its
 * physical address for diagnostics. Prints as 32-bit hex words. If virt is
 * NULL we skip the dump (we can't reverse-map from phys to virt here). */
static void uhci_dump_mem(const char *label, void *virt, uint32_t phys, size_t bytes)
{
    if (!virt) {
        SERIAL_LOG_HEX("UHCI: ", (uint32_t)label);
        SERIAL_LOG_HEX(" UHCI: phys=", phys);
        SERIAL_LOG(" UHCI: virt not available for dump\n");
        return;
    }

    SERIAL_LOG_HEX("UHCI: ", (uint32_t)label);
    SERIAL_LOG_HEX(" UHCI: phys=", phys);
    SERIAL_LOG_HEX(" UHCI: virt=", (uint32_t)virt);

    size_t to_read = (bytes + 3) & ~3U; // round up to dword
    uint8_t *p = (uint8_t *)virt;
    for (size_t off = 0; off < to_read; off += 4) {
        uint32_t v = *(uint32_t *)(p + off);
        SERIAL_LOG_HEX("UHCI: +offset=", (uint32_t)off);
        SERIAL_LOG_HEX(" UHCI: dword=", v);
    }
}

/* Decode and log a UHCI token field (useful to verify device/endpoint/length) */
static void uhci_decode_and_log_token(uint32_t token, const char *label) {
    uint8_t pid = token & 0xFF;
    uint8_t device = (token >> 8) & 0x7F;
    uint8_t endpoint = (token >> 15) & 0xF;
    uint32_t maxlen = ((token >> 21) & 0x7FF) + 1;
    SERIAL_LOG_HEX("UHCI: ", (uint32_t)label);
    SERIAL_LOG_HEX(" UHCI: raw_token=", token);
    SERIAL_LOG_DEC(" UHCI: pid=", pid);
    SERIAL_LOG_DEC(" UHCI: dev=", device);
    SERIAL_LOG_DEC(" UHCI: ep=", endpoint);
    SERIAL_LOG_DEC(" UHCI: maxlen=", maxlen);
}

/* Compact dump of QH and TD chain structures (control/token/link/buffer)
 * This is used immediately after writing descriptors to memory so we can
 * capture the exact controller-visible descriptor contents before
 * activation. Keeps format consistent with existing SERIAL_LOG_* helpers.
 */
static void uhci_dump_qh_td_chain(uhci_controller_t *uhci, uhci_qh_t *qh,
                                  uhci_td_t *setup_td, uhci_td_t *data_head,
                                  uhci_td_t *status_td)
{
    if (qh) {
        uint32_t qh_p = vaddr_to_phys(qh);
        SERIAL_LOG_HEX("UHCI: DIAG QH virt=", (uint32_t)qh);
        SERIAL_LOG_HEX(" UHCI: DIAG QH phys=", qh_p);
        SERIAL_LOG_HEX(" UHCI: DIAG qh.link_ptr=", qh->link_ptr);
        SERIAL_LOG_HEX(" UHCI: DIAG qh.element_ptr=", qh->element_ptr);
    }

    if (setup_td) {
        uint32_t s_p = vaddr_to_phys(setup_td);
        SERIAL_LOG_HEX("UHCI: DIAG setup_td.virt=", (uint32_t)setup_td);
        SERIAL_LOG_HEX(" UHCI: DIAG setup_td.phys=", s_p);
        SERIAL_LOG_HEX(" UHCI: DIAG setup_td.control=", setup_td->hw.control);
        SERIAL_LOG_HEX(" UHCI: DIAG setup_td.token=", setup_td->hw.token);
        SERIAL_LOG_HEX(" UHCI: DIAG setup_td.link_ptr=", setup_td->hw.link_ptr);
        SERIAL_LOG_HEX(" UHCI: DIAG setup_td.buffer=", setup_td->hw.buffer);
        uhci_decode_and_log_token(setup_td->hw.token, "DIAG setup_td token");
    }

    if (data_head) {
        uhci_td_t *t = data_head; int di = 0;
        while (t && di < 32) { /* limit to avoid huge logs */
            uint32_t t_p = vaddr_to_phys(t);
            SERIAL_LOG_HEX("UHCI: DIAG data_td[", (uint32_t)di);
            SERIAL_LOG_HEX("] .virt=", (uint32_t)t);
            SERIAL_LOG_HEX(" UHCI: DIAG data_td[].phys=", t_p);
            SERIAL_LOG_HEX(" UHCI: DIAG data_td[].control=", t->hw.control);
            SERIAL_LOG_HEX(" UHCI: DIAG data_td[].token=", t->hw.token);
            SERIAL_LOG_HEX(" UHCI: DIAG data_td[].link_ptr=", t->hw.link_ptr);
            SERIAL_LOG_HEX(" UHCI: DIAG data_td[].buffer=", t->hw.buffer);
            uhci_decode_and_log_token(t->hw.token, "DIAG data_td token");
            t = t->next; di++;
        }
    }

    if (status_td) {
        uint32_t st_p = vaddr_to_phys(status_td);
        SERIAL_LOG_HEX("UHCI: DIAG status_td.virt=", (uint32_t)status_td);
        SERIAL_LOG_HEX(" UHCI: DIAG status_td.phys=", st_p);
        SERIAL_LOG_HEX(" UHCI: DIAG status_td.control=", status_td->hw.control);
        SERIAL_LOG_HEX(" UHCI: DIAG status_td.token=", status_td->hw.token);
        SERIAL_LOG_HEX(" UHCI: DIAG status_td.link_ptr=", status_td->hw.link_ptr);
        SERIAL_LOG_HEX(" UHCI: DIAG status_td.buffer=", status_td->hw.buffer);
        uhci_decode_and_log_token(status_td->hw.token, "DIAG status_td token");
    }
}

/* Print human-readable TD control/status bits */
static void uhci_print_td_bits(uint32_t control)
{
    SERIAL_LOG("UHCI: TD bits:");
    if (control & UHCI_TD_ACTIVE) SERIAL_LOG(" ACTIVE");
    if (control & UHCI_TD_IOC) SERIAL_LOG(" IOC");
    if (control & UHCI_TD_SPD) SERIAL_LOG(" SPD");
    if (control & UHCI_TD_LS) SERIAL_LOG(" LS");
    /* Error counter bits (bits 27-28?) printed as number */
    uint32_t errs = (control >> 27) & 0x3;
    SERIAL_LOG_HEX(" UHCI: TD errs=", errs);
    SERIAL_LOG("\n");
}

/* Print USBCMD and USBSTS registers with basic decoding */
static void uhci_print_controller_status(uhci_controller_t *uhci)
{
    uint16_t usbcmd = uhci_inw(uhci->io_base + UHCI_USBCMD);
    uint16_t usbsts = uhci_inw(uhci->io_base + UHCI_USBSTS);
    SERIAL_LOG_HEX("UHCI: USBCMD=", usbcmd);
    if (usbcmd & UHCI_CMD_RS) SERIAL_LOG(" UHCI: RUN/START=1"); else SERIAL_LOG(" UHCI: RUN/START=0");
    SERIAL_LOG_HEX(" UHCI: USBSTS=", usbsts);
    if (usbsts & UHCI_STS_USBINT) SERIAL_LOG(" UHCI: INT");
    if (usbsts & UHCI_STS_ERROR) SERIAL_LOG(" UHCI: ERROR");
    if (usbsts & UHCI_STS_RD) SERIAL_LOG(" UHCI: RESUME_DET");
    if (usbsts & UHCI_STS_HSE) SERIAL_LOG(" UHCI: HOST_SYS_ERR");
    if (usbsts & UHCI_STS_HCPE) SERIAL_LOG(" UHCI: CPE");
    SERIAL_LOG("\n");
}

int uhci_control_transfer(uhci_controller_t *uhci, usb_device_t *device,
                         usb_setup_packet_t *setup, void *data, uint16_t length)
{
    SERIAL_LOG("UHCI: CRASH TEST 1 - Function entry\n");
    uhci_log_ts();
    SERIAL_LOG("UHCI: === CONTROL TRANSFER START ===\n");
    SERIAL_LOG_HEX("UHCI: device addr=", device->address);
    SERIAL_LOG_HEX(" port=", device->port);
    SERIAL_LOG_HEX(" setup req=", setup->bRequest);
    SERIAL_LOG_HEX(" len=", length);
    SERIAL_LOG("\n");
    
    SERIAL_LOG("UHCI: CRASH TEST 2 - Before parameter validation\n");
    // Simplified, balanced implementation preserving fragmentation and retry behavior.
    if (!uhci || !device || !setup) return -1;

    SERIAL_LOG("UHCI: CRASH TEST 3 - Before max packet size calc\n");
    uint16_t max_pkt = uhci_get_control_max_packet_size(device);
    bool has_data = (data && length > 0);
    const int max_retries = 3;

    SERIAL_LOG("UHCI: CRASH TEST 4 - Before retry loop\n");
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        SERIAL_LOG_DEC("UHCI: CRASH TEST 5 - Attempt ", attempt);
        SERIAL_LOG("\n");
        
        SERIAL_LOG("UHCI: CRASH TEST 6 - Before QH/TD allocation\n");
        uhci_qh_t *qh = uhci_alloc_qh(uhci);
        
        SERIAL_LOG("UHCI: CRASH TEST 7 - After QH alloc, before TD alloc\n");
        uhci_td_t *setup_td = uhci_alloc_td(uhci);
        uhci_td_t *status_td = uhci_alloc_td(uhci);
        uhci_td_t *data_head = NULL, *data_tail = NULL;
        if (!qh || !setup_td || !status_td) {
            if (qh) uhci_free_qh(uhci, qh);
            uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
            return -1;
        }

        SERIAL_LOG("UHCI: CRASH TEST 8 - Before setup TD configuration\n");
        // Setup TD - SETUP packets are always exactly 8 bytes
        uhci_setup_td_common(setup_td, device, 0);
        setup_td->hw.token = uhci_create_token(UHCI_TD_PID_SETUP, device->address, 0, 8, false); // maxlen=8 bytes
        
        SERIAL_LOG("UHCI: CRASH TEST 9 - After token setup, before decode\n");
    uhci_decode_and_log_token(setup_td->hw.token, "setup_td");
    
        SERIAL_LOG("UHCI: CRASH TEST 10 - Before vaddr_to_phys\n");
        /* Buffer must be a physical address for DMA */
        uint32_t setup_buf_phys = vaddr_to_phys(setup);
        SERIAL_LOG("UHCI: CRASH TEST 11 - After vaddr_to_phys, before phys check\n");
        if (setup_buf_phys == 0) {
            SERIAL_LOG("UHCI: Missing physical mapping for setup buffer\n");
            uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
            uhci_free_qh(uhci, qh);
            return -1;
        }
        SERIAL_LOG("UHCI: CRASH TEST 12 - Before buffer assignment\n");
        setup_td->hw.buffer = setup_buf_phys;
        
        // Dump setup packet contents for verification
        uint8_t *setup_bytes = (uint8_t*)setup;
        SERIAL_LOG("UHCI: Setup packet bytes: ");
        for (int i = 0; i < 8; i++) {
            SERIAL_LOG_HEX("", setup_bytes[i]);
            SERIAL_LOG(" ");
        }
        SERIAL_LOG("\n");

        SERIAL_LOG("UHCI: CRASH TEST 13 - After buffer assignment, before data phase\n");
        // Data phase fragmentation
        if (has_data) {
            SERIAL_LOG("UHCI: CRASH TEST 14 - Inside data phase\n");
            uint8_t *ptr = (uint8_t *)data;
            uint32_t remaining = length;
            uint32_t pid = (setup->bmRequestType & 0x80) ? UHCI_TD_PID_IN : UHCI_TD_PID_OUT;
            bool data_toggle = true;  // Start with DATA1 for control transfer data phase

            while (remaining) {
                SERIAL_LOG("UHCI: CRASH TEST 15 - Data TD loop iteration\n");
                uint32_t chunk = remaining > max_pkt ? max_pkt : remaining;
                uhci_td_t *t = uhci_alloc_td(uhci);
                if (!t) break;
                
                SERIAL_LOG("UHCI: CRASH TEST 16 - Before data TD setup\n");
                uhci_setup_td_common(t, device, 0);
                t->hw.token = uhci_create_token(pid, device->address, 0, chunk, data_toggle);
                uhci_decode_and_log_token(t->hw.token,"data_td");
                
                SERIAL_LOG("UHCI: CRASH TEST 17 - Before data vaddr_to_phys\n");
                uint32_t data_buf_phys = vaddr_to_phys(ptr);
                if (data_buf_phys == 0) {
                    SERIAL_LOG("UHCI: Missing physical mapping for data buffer\n");
                    uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
                    uhci_free_qh(uhci, qh);
                    return -1;
                }
                t->hw.buffer = data_buf_phys;
                t->next = NULL;
                t->hw.link_ptr = TD_PTR_TERMINATE; // terminate until chained

                SERIAL_LOG("UHCI: CRASH TEST 18 - Before data TD linking\n");
                if (!data_head) data_head = t;
                if (data_tail) {
                    data_tail->next = t;
                    data_tail->link_ptr = (uint32_t)t;
                }
                data_tail = t;

                ptr += chunk;
                remaining -= chunk;
                data_toggle = !data_toggle;  // Alternate DATA1/DATA0 for subsequent TDs
            }
        }

        SERIAL_LOG("UHCI: CRASH TEST 19 - After data phase, before status TD\n");

        // Status TD
        SERIAL_LOG("UHCI: CRASH TEST 20 - Before status TD setup\n");
        uhci_setup_td_common(status_td, device, UHCI_TD_IOC);
        status_td->hw.token = uhci_create_token(has_data ? ((setup->bmRequestType & 0x80) ? UHCI_TD_PID_OUT : UHCI_TD_PID_IN) : UHCI_TD_PID_IN,
                                             device->address, 0, max_pkt, true);
        
        SERIAL_LOG("UHCI: CRASH TEST 21 - After status TD token, before decode\n");
    uhci_decode_and_log_token(status_td->hw.token, "status_td");
    status_td->hw.buffer = 0; /* status stage has no data buffer */
        status_td->hw.link_ptr = TD_PTR_TERMINATE; // terminate

        SERIAL_LOG("UHCI: CRASH TEST 22 - Before TD physical translation\n");
        // Translate descriptors to physical addresses and chain
    uint32_t setup_phys = vaddr_to_phys(setup_td);
    uint32_t status_phys = vaddr_to_phys(status_td);
        SERIAL_LOG("UHCI: CRASH TEST 23 - After physical translation, before check\n");
        if (setup_phys == 0 || status_phys == 0) {
            SERIAL_LOG("UHCI: Missing physical mapping for setup/status TD\n");
            uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
            uhci_free_qh(uhci, qh);
            return -1;
        }

        SERIAL_LOG("UHCI: CRASH TEST 24 - Before data chain processing\n");
        if (data_head) {
            SERIAL_LOG("UHCI: CRASH TEST 25 - Inside data chain walk\n");
            // Walk data chain and set physical link_ptrs
            uhci_td_t *tmp = data_head;
            int chain_count = 0;
            SERIAL_LOG("UHCI: Starting data chain walk\n");
            while (tmp) {
                if (chain_count++ >= 10) {
                    SERIAL_LOG("UHCI: >>>>>> INFINITE LOOP DETECTED - BREAKING <<<<<<\n");
                    break;
                }
                SERIAL_LOG("UHCI: About to call vaddr_to_phys\n");
                uint32_t this_phys = vaddr_to_phys(tmp);
                SERIAL_LOG("UHCI: vaddr_to_phys returned\n");
                if (this_phys == 0) {
                    SERIAL_LOG("UHCI: Missing physical mapping for data TD\n");
                    uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
                    uhci_free_qh(uhci, qh);
                    return -1;
                }
                if (tmp->next) {
                    uint32_t next_phys = vaddr_to_phys(tmp->next);
                    if (next_phys == 0) {
                        SERIAL_LOG("UHCI: Missing physical mapping for next data TD\n");
                        uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
                        uhci_free_qh(uhci, qh);
                        return -1;
                    }
                        /* Link to next TD with DEPTH flag for breadth-first */
                    tmp->hw.link_ptr = next_phys | TD_PTR_DEPTH;
                } else {
                    // Last data TD points to status TD (physical)
                    tmp->hw.link_ptr = status_phys | TD_PTR_DEPTH;
                }
                tmp = tmp->next;
            }

        SERIAL_LOG("UHCI: CRASH TEST 27 - After data chain setup\n");

            SERIAL_LOG("UHCI: CRASH TEST 28 - Before data head phys mapping\n");
            uint32_t data_head_phys = vaddr_to_phys(data_head);
            if (data_head_phys == 0) {
                SERIAL_LOG("UHCI: Missing physical mapping for data head TD\n");
                uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
                uhci_free_qh(uhci, qh);
                return -1;
            }
                /* point setup -> data chain (HARDWARE FIELD!) with DEPTH flag */
            setup_td->hw.link_ptr = data_head_phys | TD_PTR_DEPTH;
        } else {
            setup_td->hw.link_ptr = status_phys | TD_PTR_DEPTH;
        }

        SERIAL_LOG("UHCI: CRASH TEST 29 - Before frame number read\n");
        // Place QH into a few upcoming frames for immediate processing (use physical QH pointer)
        uint16_t cur = uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF;
        
        SERIAL_LOG("UHCI: CRASH TEST 30 - After frame number read, before frame backup\n");
        uint32_t orig[8];
        for (int i = 0; i < 8; ++i) orig[i] = uhci->frame_list[(cur + i) & 0x3FF];

        SERIAL_LOG("UHCI: CRASH TEST 31 - After frame backup, before QH phys mapping\n");
    uint32_t qh_phys = vaddr_to_phys(qh);
        SERIAL_LOG("UHCI: CRASH TEST 32 - After QH phys mapping, before check\n");
        if (qh_phys == 0) {
            SERIAL_LOG("UHCI: Missing physical mapping for QH\n");
            uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
            uhci_free_qh(uhci, qh);
            return -1;
        }

    SERIAL_LOG("UHCI: CRASH TEST 33 - Before QH link/element setup\n");
    qh->link_ptr = 1; // keep pool marker/terminate state
    qh->element_ptr = setup_phys;
    
    SERIAL_LOG("UHCI: CRASH TEST 34 - Before frame list assignment\n");
    uint32_t qh_entry = qh_phys | 0x2; // set QH-type bit when putting into frame list
#if UHCI_TEST_DIRECT_TD
    /* Test mode: schedule the setup TD directly into the frame list so
     * the controller points at a TD rather than a QH. This is useful to
     * determine whether UHCI processes TDs in-place. */
    SERIAL_LOG("UHCI: CRASH TEST 35 - Direct TD mode frame assignment\n");
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = setup_phys;
#else
    SERIAL_LOG("UHCI: CRASH TEST 36 - QH mode frame assignment\n");
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = qh_entry;

    SERIAL_LOG("UHCI: CRASH TEST 37 - After frame assignment, before diagnostic\n");
    /* Extra diagnostic: print the QH frame-list entry value we wrote and
     * read it back via a temporary mapping of the FL physical page so we
     * can verify the exact controller-visible 32-bit value at the current
     * frame index (this mirrors the timeout mapping but runs pre-activation). */
    uhci_log_ts();
    SERIAL_LOG_HEX("UHCI: qh_entry_written=", qh_entry);
    
#endif

        // Debug: show virtual -> physical translations
        SERIAL_LOG_HEX("UHCI: QH virt=", (uint32_t)qh);
        SERIAL_LOG_HEX(" UHCI: QH phys=", qh_phys);
        SERIAL_LOG_HEX(" UHCI: setup_td virt=", (uint32_t)setup_td);
        SERIAL_LOG_HEX(" UHCI: setup_td phys=", setup_phys);
        if (data_head) {
            SERIAL_LOG_HEX("UHCI: data_head virt=", (uint32_t)data_head);
            SERIAL_LOG_HEX(" UHCI: data_head phys=", vaddr_to_phys(data_head));
        }

        /* Print the SETUP packet contents to help correlate host requests
         * with TD scheduling. */
        SERIAL_LOG_HEX("UHCI: SETUP bmRequestType=", setup->bmRequestType);
        SERIAL_LOG_HEX(" UHCI: SETUP bRequest=", setup->bRequest);
        SERIAL_LOG_HEX(" UHCI: SETUP wValue=", setup->wValue);
        SERIAL_LOG_HEX(" UHCI: SETUP wIndex=", setup->wIndex);
        SERIAL_LOG_HEX(" UHCI: SETUP wLength=", setup->wLength);

    /* Now activate the transfer */
    SERIAL_LOG("UHCI: Activating USB transfer\n");
    SERIAL_LOG_HEX("UHCI: FRNUM pre-activate=", uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF);
    
    /* Verify frame list entries */
    for (int i = 0; i < 4; ++i) {
        SERIAL_LOG_HEX("UHCI: frame_list[", (uint32_t)((cur + i) & 0x3FF));
        SERIAL_LOG_HEX("] = ", uhci->frame_list[(cur + i) & 0x3FF]);
    }
    SERIAL_LOG_HEX("UHCI: pre-activate QH virt=", (uint32_t)qh);
    SERIAL_LOG_HEX(" UHCI: pre-activate QH phys=", qh_phys);
    SERIAL_LOG_HEX("UHCI: pre-activate setup_td virt=", (uint32_t)setup_td);
    SERIAL_LOG_HEX(" UHCI: pre-activate setup_td phys=", setup_phys);
    
    /* Skip problematic memory dumps - they cause system hangs */
    /* Skip problematic QH/TD chain dump - causes system hangs */

    /* Re-write FLBASEADD before activation for safety */
    uint32_t fl_page_write = vaddr_to_phys(uhci->frame_list) & ~0xFFFU;
    uhci_outl(uhci->io_base + UHCI_FLBASEADD, fl_page_write);
    SERIAL_LOG_HEX("UHCI: FLBASEADD wrote=", fl_page_write);
    uint32_t hw_flbase_pre = uhci_inl(uhci->io_base + UHCI_FLBASEADD);
    SERIAL_LOG_HEX(" readback=", hw_flbase_pre);
    /* Activate the TD chain as the final visible write so the controller
     * never sees a partially-initialized descriptor. */
    
    /* Skip problematic controller status print - causes system hangs */
    
    /* Confirm QH element_ptr points to the setup TD physical */
    SERIAL_LOG_HEX("UHCI: qh.element_ptr=", qh->element_ptr);
    SERIAL_LOG_HEX("UHCI: expected setup_phys=", setup_phys);
    if ((qh->element_ptr & ~0x7U) == setup_phys) {
        SERIAL_LOG("UHCI: QH->element_ptr matches setup TD phys\n");
    } else {
        SERIAL_LOG("UHCI: QH->element_ptr DOES NOT match setup TD phys\n");
    }
    
    SERIAL_LOG("UHCI: Activating TD chain...\n");

    uhci_td_activate_chain(uhci, qh, setup_td, data_head, status_td);

    /* Check basic TD activation status */
    SERIAL_LOG("UHCI: POST-ACTIVATE TD Status\n");
    SERIAL_LOG_HEX("UHCI: setup_td.control=", setup_td->hw.control);
    if (setup_td->hw.control & (1 << 23)) {
        SERIAL_LOG("UHCI: Setup TD is ACTIVE\n");
    } else {
        SERIAL_LOG("UHCI: Setup TD is NOT active\n");
    }
    SERIAL_LOG_HEX("UHCI: status_td.control=", status_td->hw.control);
    if (status_td->hw.control & (1 << 23)) {
        SERIAL_LOG("UHCI: Status TD is ACTIVE\n");
    } else {
        SERIAL_LOG("UHCI: Status TD is NOT active\n");
    }

    /* Skip problematic controller status print immediately after activation */
    // uhci_print_controller_status(uhci);

    /* Every second, print TD status bits and USBSTS for up to 15s or until
     * TDs complete. This gives us time-correlated diagnostics of status
     * transitions from ACTIVE -> complete/stalled. */
    uhci_log_ts();
    SERIAL_LOG("UHCI: === POLLING LOOP START ===\n");
    SERIAL_LOG("UHCI: Waiting for TDs to complete (max 15s)\n");
    for (int s = 0; s < 15; ++s) {
        uhci_delay_ms(1000);
        uhci_log_ts();
        SERIAL_LOG_DEC("UHCI: === POLLING SECOND ", s+1);
        SERIAL_LOG(" OF 15 ===\n");
        SERIAL_LOG_HEX("UHCI: setup_td.control=", setup_td->hw.control);
        uhci_print_td_bits(setup_td->hw.control);
        if (data_head) {
            uhci_td_t *t2 = data_head; int di = 0;
            while (t2 && di < 8) {
                SERIAL_LOG_HEX("UHCI: data_td[", (uint32_t)di);
                SERIAL_LOG_HEX("] .control=", t2->hw.control);
                uhci_print_td_bits(t2->hw.control);
                t2 = t2->next; di++;
            }
        }
        SERIAL_LOG_HEX("UHCI: status_td.control=", status_td->hw.control);
        uhci_print_td_bits(status_td->hw.control);
        /* Skip problematic controller status print - causes system hangs */
        // uhci_print_controller_status(uhci);

        /* If all TDs have cleared ACTIVE, consider completed */
        bool all_done = !(setup_td->hw.control & UHCI_TD_ACTIVE);
        if (data_head) {
            uhci_td_t *tmp = data_head;
            int check_count = 0;
            while (tmp && check_count++ < 100) { if (tmp->hw.control & UHCI_TD_ACTIVE) { all_done = false; break; } tmp = tmp->next; }
        }
        if (!(status_td->hw.control & UHCI_TD_ACTIVE) && all_done) {
            uhci_log_ts();
            SERIAL_LOG_DEC("UHCI: === SUCCESS - TDs completed at second ", s+1);
            SERIAL_LOG(" ===\n");
            break;
        }
    }

    /* Check if we exited due to timeout vs completion */
    bool final_all_done = !(setup_td->hw.control & UHCI_TD_ACTIVE);
    if (data_head) {
        uhci_td_t *tmp = data_head;
        int final_check_count = 0;
        while (tmp && final_check_count++ < 100) { if (tmp->hw.control & UHCI_TD_ACTIVE) { final_all_done = false; break; } tmp = tmp->next; }
    }
    final_all_done = final_all_done && !(status_td->hw.control & UHCI_TD_ACTIVE);
    
    if (!final_all_done) {
        uhci_log_ts();
        SERIAL_LOG("UHCI: === TIMEOUT - TDs still ACTIVE after 15s ===\\n");
        SERIAL_LOG_HEX("UHCI: setup_td.control=", setup_td->hw.control);
        if (data_head) {
            SERIAL_LOG_HEX("UHCI: data_td.control=", data_head->hw.control);
        }
        SERIAL_LOG_HEX("UHCI: status_td.control=", status_td->hw.control);
    }

    /* DEBUG: Forced dump immediately after enqueue (makes captures reliable
     * in emulator runs). This is a debug-only hook that prints the same
     * diagnostic information as the timeout path so we can capture the
     * controller-visible layout without waiting for the full timeout. To
     * enable, define UHCI_FORCE_DUMP_AFTER_ENQUEUE in the build. */
#ifdef UHCI_FORCE_DUMP_AFTER_ENQUEUE
    SERIAL_LOG("UHCI: DEBUG FORCE DUMP AFTER ENQUEUE\n");
    SERIAL_LOG_HEX("UHCI: FRNUM at enqueue=", uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF);
    uint16_t uhci_status_dbg = uhci_inw(uhci->io_base + UHCI_USBSTS);
    SERIAL_LOG_HEX("UHCI: USBSTS=", uhci_status_dbg);
    uint16_t usbcmd_dbg = uhci_inw(uhci->io_base + UHCI_USBCMD);
    SERIAL_LOG_HEX("UHCI: USBCMD=", usbcmd_dbg);
    uint16_t usbintr_dbg = uhci_inw(uhci->io_base + UHCI_USBINTR);
    SERIAL_LOG_HEX("UHCI: USBINTR=", usbintr_dbg);
    uint16_t p1_dbg = uhci_inw(uhci->io_base + UHCI_PORTSC1);
    SERIAL_LOG_HEX("UHCI: PORTSC1=", p1_dbg);
    uint16_t p2_dbg = uhci_inw(uhci->io_base + UHCI_PORTSC2);
    SERIAL_LOG_HEX("UHCI: PORTSC2=", p2_dbg);
    uint32_t hw_flbase_dbg = uhci_inl(uhci->io_base + UHCI_FLBASEADD);
    SERIAL_LOG_HEX("UHCI: FLBASEADD (reg)=", hw_flbase_dbg);
    uint32_t fl_page_dbg = hw_flbase_dbg & ~0xFFFU;
    /* Dump frame list window and descriptors (first 8 entries) */
    for (int i = 0; i < 8; ++i) {
        SERIAL_LOG_HEX("UHCI: frame_list[", (uint32_t)((cur + i) & 0x3FF));
        SERIAL_LOG_HEX("] = ", uhci->frame_list[(cur + i) & 0x3FF]);
    }
    uint32_t fl_phys_dbg = vaddr_to_phys(uhci->frame_list);
    uhci_dump_mem("frame_list", uhci->frame_list, fl_phys_dbg, 32);
    /* Dump QH/TD objects we just created */
    uhci_dump_mem("qh", qh, qh_phys, 32);
    uhci_dump_mem("setup_td", setup_td, setup_phys, 32);
    if (data_head) {
        uhci_td_t *t_dbg = data_head; int idx_dbg = 0;
        while (t_dbg && idx_dbg < 8) {
            uint32_t t_phys_dbg = vaddr_to_phys(t_dbg);
            SERIAL_LOG_HEX("UHCI: data_td[", (uint32_t)idx_dbg);
            SERIAL_LOG_HEX("] .virt=", (uint32_t)t_dbg);
            SERIAL_LOG_HEX(" UHCI: data_td[].phys=", t_phys_dbg);
            uhci_dump_mem("data_td", t_dbg, t_phys_dbg, 32);
            t_dbg = t_dbg->next; idx_dbg++;
        }
    }
    uhci_dump_mem("status_td", status_td, status_phys, 32);
#endif

        // Wait for completion (simple polling)
    /* Short timeout for debug: trigger diagnostic dump quickly if TDs remain ACTIVE */
    int timeout = 1000; /* original: 50000 */
        int poll_ctr = 0;
        while (timeout--) {
            bool s_done = !(setup_td->hw.control & UHCI_TD_ACTIVE);
            bool d_done = true;
            if (data_head) {
                uhci_td_t *tmp = data_head;
                while (tmp) {
                    if (tmp->hw.control & UHCI_TD_ACTIVE) { d_done = false; break; }
                    tmp = tmp->next;
                }
            }
            bool st_done = !(status_td->hw.control & UHCI_TD_ACTIVE);
            /* Periodically print TD status while waiting so we get output
             * even if the guest halts or we get an early shutdown. */
            if ((poll_ctr++ % 500) == 0) {
                SERIAL_LOG("UHCI: Poll status: setup_td.control=");
                SERIAL_LOG_HEX("", setup_td->hw.control);
                SERIAL_LOG(" UHCI: status_td.control=");
                SERIAL_LOG_HEX("", status_td->hw.control);
                if (data_head) {
                    uhci_td_t *ttmp = data_head; int di = 0;
                    while (ttmp && di < 4) { /* print up to first 4 data TDs */
                        SERIAL_LOG_HEX("UHCI: data_td[", (uint32_t)di);
                        SERIAL_LOG_HEX("] .control=", ttmp->hw.control);
                        ttmp = ttmp->next; di++;
                    }
                }
            }
            if (s_done && d_done && st_done) {
                /* Successful completion - print compact trace */
                SERIAL_LOG("UHCI: Control transfer completed successfully\n");
                SERIAL_LOG_HEX("UHCI: final FRNUM=", uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF);
                SERIAL_LOG_HEX("UHCI: setup_td.control=", setup_td->hw.control);
                SERIAL_LOG_HEX("UHCI: setup_td.token=", setup_td->hw.token);
                if (data_head) {
                    uhci_td_t *t2 = data_head; int di = 0;
                    while (t2) {
                        SERIAL_LOG_HEX("UHCI: data_td[", di);
                        SERIAL_LOG_HEX("].control=", t2->hw.control);
                        SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)di);
                        SERIAL_LOG_HEX("] .token=", t2->hw.token);
                        t2 = t2->next; di++;
                    }
                }
                SERIAL_LOG_HEX("UHCI: status_td.control=", status_td->hw.control);
                SERIAL_LOG_HEX("UHCI: status_td.token=", status_td->hw.token);
                // restore frame list
                for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = orig[i];
                uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
                uhci_free_qh(uhci, qh);
                return 0;
            }
        }

    /* Timeout: dump detailed TD/frame state to help diagnose stuck ACTIVE
     * or NAK behavior. Also read controller status and port status
     * registers so we can see hardware-level errors reported by UHCI. */
    uhci_log_ts();
    SERIAL_LOG("UHCI: Control transfer TIMEOUT, dumping TD/frame state\n");
    SERIAL_LOG_HEX("UHCI: FRNUM at timeout=", uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF);
    
    /* CRITICAL: Clean up TDs on timeout to prevent pool exhaustion */
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = orig[i];
    uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
    uhci_free_qh(uhci, qh);
    SERIAL_LOG("UHCI: TD cleanup completed after timeout\n");

    /* Read UHCI status register and port status registers for diagnostics */
    uint16_t uhci_status = uhci_inw(uhci->io_base + UHCI_USBSTS);
    SERIAL_LOG_HEX("UHCI: USBSTS=", uhci_status);
    /* Also show command and interrupt enable registers for further clues */
    uint16_t usbcmd = uhci_inw(uhci->io_base + UHCI_USBCMD);
    SERIAL_LOG_HEX("UHCI: USBCMD=", usbcmd);
    uint16_t usbintr = uhci_inw(uhci->io_base + UHCI_USBINTR);
    SERIAL_LOG_HEX("UHCI: USBINTR=", usbintr);
    uint16_t port1 = uhci_inw(uhci->io_base + UHCI_PORTSC1);
    SERIAL_LOG_HEX("UHCI: PORTSC1=", port1);
    uint16_t port2 = uhci_inw(uhci->io_base + UHCI_PORTSC2);
    SERIAL_LOG_HEX("UHCI: PORTSC2=", port2);
        for (int i = 0; i < 8; ++i) {
            SERIAL_LOG_HEX("UHCI: frame_list[", (uint32_t)((cur + i) & 0x3FF));
            SERIAL_LOG_HEX("] = ", uhci->frame_list[(cur + i) & 0x3FF]);
        }
        /* Dump raw memory bytes for the frame list and QH/TD descriptors so we
         * can verify the exact controller-visible layout (first 32 bytes). */
        uint32_t fl_phys = vaddr_to_phys(uhci->frame_list);
        uhci_dump_mem("frame_list", uhci->frame_list, fl_phys, 32);
        /* Read FLBASEADD from the controller and map that physical page to
         * a temporary virtual page so we can verify the controller-seen
         * contents (rules out a mismatch between the physical address we
         * think we wrote and what the controller actually reads). */
        uint32_t hw_flbase = uhci_inl(uhci->io_base + UHCI_FLBASEADD);
        SERIAL_LOG_HEX("UHCI: FLBASEADD (reg) = ", hw_flbase);
        uint32_t fl_page = hw_flbase & ~0xFFFU;
        /* Allocate one scratch virtual page and remap it to the frame_list's
         * physical page so we can read the controller-visible bytes. */
        void *scratch = vmm_alloc_pages(1);
        if (scratch) {
            /* Map the scratch virtual page to the FL physical page */
            vmm_map_page((uint32_t)scratch, fl_page, PAGE_WRITE);
            /* Dump the first 32 bytes at page base for reference */
            uhci_dump_mem("frame_list_mapped_by_flbase", scratch, hw_flbase, 32);
            /* Also dump the 8-frame window starting at the current FRNUM so we
             * can directly compare what the controller will read at this frame
             * index (offset = cur * 4). */
            uint32_t cur_off = (cur & 0x3FF) * 4;
            uhci_dump_mem("frame_list_mapped_by_flbase+cur", (void *)((uint8_t *)scratch + cur_off), hw_flbase + cur_off, 32);
        } else {
            SERIAL_LOG("UHCI: Failed to allocate scratch page for FLBASEADD mapping\n");
        }
        /* QH and setup/status TD physicals are available via qh_phys/setup_phys */
        uhci_dump_mem("qh", qh, qh_phys, 32);
        uhci_dump_mem("setup_td", setup_td, setup_phys, 32);
        /* Setup TD */
        SERIAL_LOG_HEX("UHCI: setup_td.virt=", (uint32_t)setup_td);
        SERIAL_LOG_HEX(" UHCI: setup_td.phys=", setup_phys);
        SERIAL_LOG_HEX(" UHCI: setup_td.control=", setup_td->hw.control);
        SERIAL_LOG_HEX(" UHCI: setup_td.token=", setup_td->hw.token);
        SERIAL_LOG_HEX(" UHCI: setup_td.link_ptr=", setup_td->hw.link_ptr);
        SERIAL_LOG_HEX(" UHCI: setup_td.buffer=", setup_td->hw.buffer);

        /* Data TDs */
        if (data_head) {
            uhci_td_t *t = data_head; int idx = 0;
            while (t) {
                uint32_t t_phys = vaddr_to_phys(t);
                SERIAL_LOG_HEX("UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .virt=", (uint32_t)t);
                SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .phys=", t_phys);
                SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .control=", t->hw.control);
                SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .token=", t->hw.token);
                SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .link_ptr=", t->hw.link_ptr);
                SERIAL_LOG_HEX(" UHCI: data_td[", (uint32_t)idx);
                SERIAL_LOG_HEX("] .buffer=", t->hw.buffer);
                /* Dump first 32 bytes of this data TD structure for verification */
                uhci_dump_mem("data_td", t, t_phys, 32);
                t = t->next; idx++;
            }
        }

        /* Status TD */
        SERIAL_LOG_HEX("UHCI: status_td.virt=", (uint32_t)status_td);
        SERIAL_LOG_HEX(" UHCI: status_td.phys=", status_phys);
        SERIAL_LOG_HEX(" UHCI: status_td.control=", status_td->hw.control);
        SERIAL_LOG_HEX(" UHCI: status_td.token=", status_td->hw.token);
        SERIAL_LOG_HEX(" UHCI: status_td.link_ptr=", status_td->hw.link_ptr);
        SERIAL_LOG_HEX(" UHCI: status_td.buffer=", status_td->hw.buffer);
    uhci_dump_mem("status_td", status_td, status_phys, 32);

        // timeout: restore and retry if attempts remain
        for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = orig[i];
        uhci_cleanup_control_tds(uhci, setup_td, data_head, status_td);
        uhci_free_qh(uhci, qh);
        if (attempt < max_retries - 1) uhci_delay_ms(5 * (attempt + 1));
    }

    /* If we get here, control transfer failed repeatedly. As a last-ditch
     * diagnostic, schedule a single TD (SETUP/IN) with a very small buffer
     * and IOC set to see if a minimal descriptor ever completes. This can
     * help determine whether the controller will ever process a single TD
     * referenced directly from the frame list. */
    uhci_log_ts();
    SERIAL_LOG("UHCI: Running single-TD diagnostic test\n");
    uhci_td_t *single = uhci_alloc_td(uhci);
    if (single) {
        uint32_t buf_phys = pmm_alloc_page();
        if (!buf_phys) SERIAL_LOG("UHCI: pmm_alloc_page failed for single-TD buffer\n");
        single->hw.control = UHCI_TD_IOC | (3 << 27); /* IOC + 3 errs */
        single->hw.token = uhci_create_token(UHCI_TD_PID_SETUP, device->address, 0, 8, false);
        single->hw.buffer = buf_phys;
        single->hw.link_ptr = 1;
        uint32_t single_phys = vaddr_to_phys(single);
        SERIAL_LOG_HEX("UHCI: single TD virt=", (uint32_t)single);
        SERIAL_LOG_HEX(" UHCI: single TD phys=", single_phys);
    /* Place the single TD into a small window of upcoming frames */
    uint16_t cur2 = uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF;
    uint32_t saved[8];
    for (int i = 0; i < 8; ++i) saved[i] = uhci->frame_list[(cur2 + i) & 0x3FF];
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur2 + i) & 0x3FF] = single_phys;
        uhci_dump_qh_td_chain(uhci, NULL, single, NULL, NULL);
        /* Activate it */
        uhci_td_activate_chain(uhci, NULL, single, NULL, NULL);
    /* Poll for up to 10s printing status every 1s */
        for (int s = 0; s < 10; ++s) {
            uhci_delay_ms(1000);
            uhci_log_ts();
            SERIAL_LOG("UHCI: single-TD periodic status\n");
            SERIAL_LOG_HEX("UHCI: single.control=", single->hw.control);
            uhci_print_td_bits(single->hw.control);
            /* Skip problematic controller status print - causes system hangs */
            // uhci_print_controller_status(uhci);
            if (!(single->hw.control & UHCI_TD_ACTIVE)) { 
                SERIAL_LOG("UHCI: single TD cleared ACTIVE\n");
                // Check for errors in the TD status
                if (single->hw.control & (1 << 22)) SERIAL_LOG("UHCI: TD ERROR - Stalled\n");
                if (single->hw.control & (1 << 21)) SERIAL_LOG("UHCI: TD ERROR - Data Buffer Error\n");
                if (single->hw.control & (1 << 20)) SERIAL_LOG("UHCI: TD ERROR - Babble\n");
                if (single->hw.control & (1 << 19)) SERIAL_LOG("UHCI: TD ERROR - NAK Received\n");
                if (single->hw.control & (1 << 18)) SERIAL_LOG("UHCI: TD ERROR - CRC/Timeout\n");
                if (single->hw.control & (1 << 17)) SERIAL_LOG("UHCI: TD ERROR - Bitstuff Error\n");
                uint16_t actual_len = (single->hw.control & 0x7FF) + 1;
                SERIAL_LOG_DEC("UHCI: TD actual length transferred: ", actual_len);
                SERIAL_LOG("\n");
                break; 
            }
        }
        /* restore original frame list window */
        for (int i = 0; i < 8; ++i) uhci->frame_list[(cur2 + i) & 0x3FF] = saved[i];
    } else {
        SERIAL_LOG("UHCI: Could not allocate single TD for diagnostic\n");
    }

    return -1;
}

int uhci_interrupt_transfer(uhci_controller_t *uhci, usb_device_t *device, uint8_t endpoint, void *data, uint16_t length, void (*callback)(usb_transfer_t *)) 
{
    
    SERIAL_LOG("UHCI: Setting up interrupt transfer\n");
    
    // Allocate TD
    uhci_td_t *td = uhci_alloc_td(uhci);
    if (!td) {
        SERIAL_LOG("UHCI: Failed to allocate TD\n");
        return -1;
    }
    
    // Set up TD for interrupt transfer
    td->hw.link_ptr = 1; // Terminate
    /* Do not set ACTIVE here; activate after putting TD into frame list */
    td->hw.control = UHCI_TD_IOC | UHCI_TD_SPD | (3 << 27); // IOC, 3 errors
    if (device->speed == USB_SPEED_LOW) {
        td->hw.control |= UHCI_TD_LS;
    }
    // Token: device address, endpoint, packet size
    td->hw.token = (UHCI_TD_PID_IN) | 
                ((device->address & 0x7F) << 8) |
                ((endpoint & 0xF) << 15) |
                (((length - 1) & 0x7FF) << 21);
    uhci_decode_and_log_token(td->hw.token, "interrupt_td");
    
    /* Buffer must be physical for DMA */
    uint32_t int_buf_phys = vaddr_to_phys(data);
    if (int_buf_phys == 0 && data != NULL) {
        SERIAL_LOG("UHCI: Missing physical mapping for interrupt TD buffer\n");
        uhci_free_td(uhci, td);
        return -1;
    }
    td->hw.buffer = int_buf_phys;
    td->callback = callback;
    
    // For now, just add to frame 0 for simplicity
    // In a real implementation, you'd distribute across multiple frames
    uint32_t td_phys = vaddr_to_phys(td);
    if (td_phys == 0) {
        SERIAL_LOG("UHCI: Missing physical mapping for interrupt TD\n");
        uhci_free_td(uhci, td);
        return -1;
    }
    // Debug: report current frame number and existing slot content
    uint16_t cur = uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF;
    SERIAL_LOG_HEX("UHCI: FRNUM=", cur);
    SERIAL_LOG_HEX(" UHCI: scheduling into frame_list[", cur);
    SERIAL_LOG_HEX("] previous=", uhci->frame_list[cur]);
    uhci->frame_list[cur] = td_phys;
    /* Activate the single interrupt TD now that it's visible to the host */
    uhci_td_activate_chain(uhci, NULL, td, NULL, NULL);
    SERIAL_LOG_HEX("UHCI: Interrupt TD virt=", (uint32_t)td);
    SERIAL_LOG_HEX(" UHCI: Interrupt TD phys=", td_phys);
    SERIAL_LOG_HEX(" UHCI: frame_index=", cur);
    SERIAL_LOG(" UHCI: Interrupt transfer scheduled\n");
    return 0;
}

void uhci_interrupt_handler(uhci_controller_t *uhci) {
    uint16_t status = uhci_inw(uhci->io_base + UHCI_USBSTS);

    if (!(status & (UHCI_STS_USBINT | UHCI_STS_ERROR))) return;

    if (status & UHCI_STS_USBINT) SERIAL_LOG("UHCI: USB interrupt occurred\n");
    if (status & UHCI_STS_ERROR) SERIAL_LOG("UHCI: USB error interrupt\n");

    /* Walk the frame list around the current frame and attempt to resolve
     * QH -> TD chains back into our virtual pool so we can print per-TD
     * status (control/token/link/buffer). This helps determine whether the
     * controller ever updates TD.control (clears ACTIVE / sets error bits).
     */
    uint16_t cur = uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF;
    SERIAL_LOG_HEX("UHCI: Interrupt FRNUM=", cur);

    for (int i = 0; i < 8; ++i) {
        uint32_t idx = (cur + i) & 0x3FF;
        uint32_t entry = uhci->frame_list[idx];
        SERIAL_LOG_HEX("UHCI: frame_list[", idx);
        SERIAL_LOG_HEX("] = ", entry);
        if (entry == 1) continue; /* terminate */

        /* QH entries are inserted with low-bit pattern 0x2 in our code */
        if (entry & 0x2) {
            uint32_t qh_phys = entry & ~0x3U;
            SERIAL_LOG_HEX("UHCI: frame contains QH phys=", qh_phys);

            /* Find the corresponding virtual QH in our pool (if any) */
            uhci_qh_t *matched_qh = NULL;
            for (int qi = 0; qi < 16; ++qi) {
                uhci_qh_t *qh = &uhci->qh_pool[qi];
                uint32_t qh_vphys = vaddr_to_phys(qh);
                if (qh_vphys == qh_phys) { matched_qh = qh; break; }
            }
            if (!matched_qh) {
                SERIAL_LOG_HEX("UHCI: QH phys not found in pool=", qh_phys);
                continue;
            }

            SERIAL_LOG_HEX("UHCI: matched QH virt=", (uint32_t)matched_qh);
            SERIAL_LOG_HEX(" UHCI: qh.element_ptr=", matched_qh->element_ptr);

            /* Walk the TD chain pointed to by the QH->element_ptr */
            uint32_t next_phys = matched_qh->element_ptr & ~0x7U;
            int td_index = 0;
            while (next_phys && next_phys != 1) {
                /* Try to locate the TD in our td_pool by physical address */
                uhci_td_t *matched_td = NULL;
                for (int ti = 0; ti < 64; ++ti) {
                    uhci_td_t *td = &uhci->td_pool[ti];
                    uint32_t td_vphys = vaddr_to_phys(td);
                    if (td_vphys == next_phys) { matched_td = td; break; }
                }

                if (!matched_td) {
                    SERIAL_LOG_HEX("UHCI: TD phys not in pool=", next_phys);
                    break;
                }

                SERIAL_LOG_HEX("UHCI: td[", (uint32_t)td_index);
                SERIAL_LOG_HEX("] virt=", (uint32_t)matched_td);
                SERIAL_LOG_HEX(" UHCI: td.phys=", vaddr_to_phys(matched_td));
                SERIAL_LOG_HEX(" UHCI: td.control=", matched_td->hw.control);
                SERIAL_LOG_HEX(" UHCI: td.token=", matched_td->hw.token);
                SERIAL_LOG_HEX(" UHCI: td.link_ptr=", matched_td->hw.link_ptr);
                SERIAL_LOG_HEX(" UHCI: td.buffer=", matched_td->hw.buffer);
                uhci_decode_and_log_token(matched_td->hw.token, "interrupt_walk_td");

                /* Follow the physical link (mask out low control bits) */
                uint32_t raw_link = matched_td->link_ptr;
                next_phys = raw_link & ~0x7U;
                if (next_phys == 0 || next_phys == 1) break;
                td_index++;
            }
        } else {
            /* Frame directly points to a TD (no QH) */
            uint32_t td_phys = entry & ~0x7U;
            SERIAL_LOG_HEX("UHCI: frame points to TD phys=", td_phys);
            /* locate in td_pool */
            uhci_td_t *matched_td = NULL;
            for (int ti = 0; ti < 64; ++ti) {
                uhci_td_t *td = &uhci->td_pool[ti];
                if (vaddr_to_phys(td) == td_phys) { matched_td = td; break; }
            }
            if (matched_td) {
                SERIAL_LOG_HEX("UHCI: direct td virt=", (uint32_t)matched_td);
                SERIAL_LOG_HEX(" UHCI: td.control=", matched_td->hw.control);
                SERIAL_LOG_HEX(" UHCI: td.token=", matched_td->hw.token);
                SERIAL_LOG_HEX(" UHCI: td.link_ptr=", matched_td->hw.link_ptr);
                SERIAL_LOG_HEX(" UHCI: td.buffer=", matched_td->hw.buffer);
                uhci_decode_and_log_token(matched_td->hw.token, "interrupt_walk_td");
            } else {
                SERIAL_LOG_HEX("UHCI: direct TD phys not in pool=", td_phys);
            }
        }
    }

    /* Clear interrupt/status bits we've observed */
    if (status & UHCI_STS_USBINT) uhci_outw(uhci->io_base + UHCI_USBSTS, UHCI_STS_USBINT);
    if (status & UHCI_STS_ERROR) uhci_outw(uhci->io_base + UHCI_USBSTS, UHCI_STS_ERROR);
}

// Bulk transfer implementation
int uhci_bulk_transfer(uhci_controller_t *uhci, usb_device_t *device, uint8_t pid, uint8_t endpoint, void *buffer, uint32_t length)
{
    if (!uhci || !device || !buffer) return -1;

    // Use 64 as default full-speed bulk max packet, low-speed devices use 8
    uint16_t max_pkt = (device->speed == USB_SPEED_LOW) ? 8 : 64;

    // Fragment into TDs
    uint8_t *ptr = (uint8_t *)buffer;
    uint32_t remaining = length;
    uhci_td_t *head = NULL, *tail = NULL;

    while (remaining > 0) {
        uint32_t chunk = remaining > max_pkt ? max_pkt : remaining;
        uhci_td_t *t = uhci_alloc_td(uhci);
        if (!t) {
            SERIAL_LOG("UHCI: Failed to allocate TD for bulk transfer\n");
            // free chain
            while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
            return -1;
        }
        uhci_setup_td_common(t, device, 0);
    t->hw.token = uhci_create_token(pid, device->address, endpoint, chunk, false);
    uhci_decode_and_log_token(t->hw.token, "bulk_td");
        uint32_t bulk_buf_phys = vaddr_to_phys(ptr);
        if (bulk_buf_phys == 0) {
            SERIAL_LOG("UHCI: Missing physical mapping for bulk TD buffer\n");
            // free chain
            while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
            return -1;
        }
        t->hw.buffer = bulk_buf_phys;
        t->next = NULL;
        t->hw.link_ptr = 1; // terminate until linked physically

        if (!head) head = t;
        if (tail) {
            tail->next = t;
        }
        tail = t;

        ptr += chunk;
        remaining -= chunk;
    }

    if (!head) {
        // Zero-length transfer: allocate a single TD with zero buffer
        uhci_td_t *t = uhci_alloc_td(uhci);
        if (!t) return -1;
    uhci_setup_td_common(t, device, 0);
    t->hw.token = uhci_create_token(pid, device->address, endpoint, 0, false);
    uhci_decode_and_log_token(t->hw.token, "bulk_td_zero_len");
        t->hw.buffer = 0;
        t->next = NULL;
        t->hw.link_ptr = 1;
        head = tail = t;
    }

    // Translate TD chain link_ptrs to physical addresses
    uhci_td_t *tmp = head;
    while (tmp) {
        uint32_t this_phys = vaddr_to_phys(tmp);
        if (this_phys == 0) {
            SERIAL_LOG("UHCI: Missing physical mapping for bulk TD\n");
            // cleanup
            while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
            return -1;
        }
        if (tmp->next) {
            uint32_t next_phys = vaddr_to_phys(tmp->next);
            if (next_phys == 0) {
                SERIAL_LOG("UHCI: Missing physical mapping for bulk next TD\n");
                while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
                return -1;
            }
            /* Link to next TD (no special depth-first bit) */
            tmp->link_ptr = next_phys;
        } else {
            tmp->link_ptr = 1; // terminate
        }
        tmp = tmp->next;
    }

    // Allocate a QH to point to the TD chain and place it into the frame list
    uhci_qh_t *qh = uhci_alloc_qh(uhci);
    if (!qh) {
        SERIAL_LOG("UHCI: Failed to allocate QH for bulk transfer\n");
        while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
        return -1;
    }

    uint32_t head_phys = vaddr_to_phys(head);
    uint32_t qh_phys = vaddr_to_phys(qh);
    if (head_phys == 0 || qh_phys == 0) {
        SERIAL_LOG("UHCI: Missing physical mapping for QH/TD chain (bulk)\n");
        uhci_free_qh(uhci, qh);
        while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
        return -1;
    }

    qh->link_ptr = 1;
    qh->element_ptr = head_phys;

    uint16_t cur = uhci_inw(uhci->io_base + UHCI_FRNUM) & 0x3FF;
    uint32_t orig[8];
    for (int i = 0; i < 8; ++i) orig[i] = uhci->frame_list[(cur + i) & 0x3FF];

    uint32_t qh_entry = qh_phys | 0x2; // QH type bit
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = qh_entry;

    SERIAL_LOG_HEX("UHCI: Bulk QH virt=", (uint32_t)qh);
    SERIAL_LOG_HEX(" UHCI: Bulk QH phys=", qh_phys);
    SERIAL_LOG_HEX(" UHCI: Bulk head virt=", (uint32_t)head);
    SERIAL_LOG_HEX(" UHCI: Bulk head phys=", head_phys);

    /* Compact post-write diagnostic for bulk transfers: dump QH/TD fields
     * so captures show the controller-visible descriptor contents. */
    uhci_dump_qh_td_chain(uhci, qh, head, head ? head->next : NULL, NULL);

    /* Activate the bulk TD chain now that the descriptors and frame list
     * entries are visible to the controller. Pass head as setup_td and
     * head->next as the data chain tail for activation traversal. */
    /* Print controller status pre-activate and confirm QH -> TD mapping */
    uhci_log_ts();
    SERIAL_LOG("UHCI: CONTROLLER STATUS PRE-ACTIVATE (bulk)\n");
    /* Skip problematic controller status print - causes system hangs */
    // uhci_print_controller_status(uhci);
    SERIAL_LOG_HEX("UHCI: bulk qh.element_ptr=", qh->element_ptr);
    SERIAL_LOG_HEX("UHCI: bulk head_phys=", head_phys);
    if ((qh->element_ptr & ~0x7U) == head_phys) SERIAL_LOG("UHCI: Bulk QH->element_ptr matches head phys\n"); else SERIAL_LOG("UHCI: Bulk QH->element_ptr DOES NOT match head phys\n");

    /* Skip problematic QH/TD chain dump - causes system hangs */
    // uhci_dump_qh_td_chain(uhci, qh, head, head ? head->next : NULL, NULL);

    uhci_td_activate_chain(uhci, qh, head, head ? head->next : NULL, NULL);

    /* Skip problematic controller status print immediately after activation */
    // uhci_print_controller_status(uhci);

    /* Periodic 1s TD status polling for bulk transfers (short window) */
    for (int s = 0; s < 15; ++s) {
        uhci_delay_ms(1000);
        uhci_log_ts();
        SERIAL_LOG("UHCI: BULK 1s-periodic status\n");
        uhci_td_t *tt = head; int bi = 0;
        while (tt && bi < 8) {
            SERIAL_LOG_HEX("UHCI: bulk_td[", (uint32_t)bi);
            SERIAL_LOG_HEX("] .control=", tt->hw.control);
            uhci_print_td_bits(tt->hw.control);
            tt = tt->next; bi++;
        }
        /* Skip problematic controller status print - causes system hangs */
        // uhci_print_controller_status(uhci);
        bool done = true; tt = head; while (tt) { if (tt->hw.control & UHCI_TD_ACTIVE) { done = false; break; } tt = tt->next; }
        if (done) { SERIAL_LOG("UHCI: Bulk TDs cleared ACTIVE during periodic checks\n"); break; }
    }

    // Poll for completion
    // Polling timeout (original production value).
    /* Shorter bulk timeout for debugging so we get dumps quickly if bulk TDs hang */
    int timeout = 20000; /* original: 1000000 */
    int poll_ctr = 0;
    while (timeout--) {
        bool done = true;
        uhci_td_t *t = head;
        while (t) {
            if (t->hw.control & UHCI_TD_ACTIVE) { done = false; break; }
            t = t->next;
        }
    /* Periodic poll output to ensure we capture TD state */
    if ((poll_ctr++ % 5000) == 0) {
            SERIAL_LOG("UHCI: Bulk poll status: head.control=");
            SERIAL_LOG_HEX("", head->hw.control);
            uhci_td_t *tt = head; int bi = 0;
            while (tt && bi < 4) {
                SERIAL_LOG_HEX("UHCI: bulk_td[", (uint32_t)bi);
                SERIAL_LOG_HEX("] .control=", tt->hw.control);
                tt = tt->next; bi++;
            }
        }
        if (done) {
            for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = orig[i];
            // free TDs and QH
            while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
            uhci_free_qh(uhci, qh);
            return 0;
        }
    }

    // Timeout
    for (int i = 0; i < 8; ++i) uhci->frame_list[(cur + i) & 0x3FF] = orig[i];
    while (head) { uhci_td_t *n = head->next; uhci_free_td(uhci, head); head = n; }
    uhci_free_qh(uhci, qh);
    return -1;
}

bool uhci_port_device_connected(uhci_controller_t *uhci, int port) {
    uint16_t port_status = uhci_inw(uhci->io_base + 0x10 + port * 2);
    return port_status & (1 << 0); // Bit 0 = CurrentConnectStatus
}

bool uhci_reset_port(uhci_controller_t *uhci, int port) {
    uint16_t port_reg = uhci->io_base + 0x10 + port * 2;
    SERIAL_LOG_DEC("UHCI: Resetting port ", port);

    uint16_t status_before = uhci_inw(port_reg);
    SERIAL_LOG_HEX("UHCI: Status before reset: ", status_before);
    
    /* Check if device is connected */
    if (!(status_before & UHCI_PORT_CCS)) {
        SERIAL_LOG("UHCI: No device connected\n");
        return false;
    }

    /* Set Reset bit using RWC-safe helper */
    uhci_port_set_bits(port_reg, UHCI_PORT_PR);
    SERIAL_LOG("UHCI: Port reset asserted\n");
    uhci_delay_ms(50); // Wait 50ms for port reset

    /* Clear Reset bit using RWC-safe helper */
    uhci_port_clr_bits(port_reg, UHCI_PORT_PR);
    SERIAL_LOG("UHCI: Port reset cleared\n");
    
    /* Wait for port to become enabled - up to 100ms */
    for (int i = 0; i < 10; i++) {
        uhci_delay_ms(10);
        uint16_t status = uhci_inw(port_reg);
        
        /* Check if device disconnected */
        if (!(status & UHCI_PORT_CCS)) {
            SERIAL_LOG("UHCI: Device disconnected during reset\n");
            return false;
        }
        
        /* Clear status change bits if set */
        if (status & UHCI_PORT_RWC) {
            uhci_port_clr_bits(port_reg, UHCI_PORT_RWC);
            SERIAL_LOG_HEX("UHCI: Cleared status change bits: ", status);
        }
        
        /* Check if port auto-enabled */
        if (status & UHCI_PORT_PE) {
            SERIAL_LOG_HEX("UHCI: Port auto-enabled, status: ", status);
            return true;
        }
        
        /* Try to explicitly enable */
        uhci_port_set_bits(port_reg, UHCI_PORT_PE);
    }
    
    uint16_t status_final = uhci_inw(port_reg);
    if (status_final & UHCI_PORT_PE) {
        SERIAL_LOG_HEX("UHCI: Port enabled after explicit set, status: ", status_final);
        return true;
    } else {
        SERIAL_LOG_HEX("UHCI: ERROR - Port enable failed, status: ", status_final);
        return false;
    }
}

void uhci_enable_port(uhci_controller_t *uhci, int port) {
    uint16_t port_reg = uhci->io_base + 0x10 + port * 2;
    
    uint16_t status = uhci_inw(port_reg);
    if (status & UHCI_PORT_PE) {
        SERIAL_LOG("UHCI: Port already enabled\n");
        return;
    }
    
    SERIAL_LOG("UHCI: Port enable check - not needed (reset handles it)\n");
}

void uhci_disable_port(uhci_controller_t *uhci, int port) {
    uint16_t port_reg = uhci->io_base + 0x10 + port * 2;
    uint16_t val = uhci_inw(port_reg);
    val &= ~(1 << 2); // clear PE
    uhci_outw(port_reg, val);
}

void uhci_delay_ms(int ms) {
    // Use PIT, TSC, or a busy loop depending on your platform
    for (volatile int i = 0; i < ms * 1000; i++) {
        __asm__ __volatile__("nop");
    }
}

 
