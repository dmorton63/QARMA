#include "usb_keyboard.h"
#include "usb_hid.h"
#include "usb.h"
#include "usb_vendor_ids.h"
#include "keyboard/keyboard.h"
#include "core/memory/heap.h"
#include "core/sleep.h"
#include "config.h"
#include "graphics/graphics.h"

// Global USB keyboard device
static usb_hid_device_t *g_usb_keyboard = NULL;
static usb_keyboard_report_t last_report = {0};

// USB HID to PS/2 scancode translation table
// Maps USB HID usage IDs to PS/2 Set 1 scancodes
static const uint8_t usb_to_scancode[256] = {
    [USB_KEY_A] = 0x1E,
    [USB_KEY_B] = 0x30,
    [USB_KEY_C] = 0x2E,
    [USB_KEY_D] = 0x20,
    [USB_KEY_E] = 0x12,
    [USB_KEY_F] = 0x21,
    [USB_KEY_G] = 0x22,
    [USB_KEY_H] = 0x23,
    [USB_KEY_I] = 0x17,
    [USB_KEY_J] = 0x24,
    [USB_KEY_K] = 0x25,
    [USB_KEY_L] = 0x26,
    [USB_KEY_M] = 0x32,
    [USB_KEY_N] = 0x31,
    [USB_KEY_O] = 0x18,
    [USB_KEY_P] = 0x19,
    [USB_KEY_Q] = 0x10,
    [USB_KEY_R] = 0x13,
    [USB_KEY_S] = 0x1F,
    [USB_KEY_T] = 0x14,
    [USB_KEY_U] = 0x16,
    [USB_KEY_V] = 0x2F,
    [USB_KEY_W] = 0x11,
    [USB_KEY_X] = 0x2D,
    [USB_KEY_Y] = 0x15,
    [USB_KEY_Z] = 0x2C,
    [USB_KEY_1] = 0x02,
    [USB_KEY_2] = 0x03,
    [USB_KEY_3] = 0x04,
    [USB_KEY_4] = 0x05,
    [USB_KEY_5] = 0x06,
    [USB_KEY_6] = 0x07,
    [USB_KEY_7] = 0x08,
    [USB_KEY_8] = 0x09,
    [USB_KEY_9] = 0x0A,
    [USB_KEY_0] = 0x0B,
    [USB_KEY_ENTER] = 0x1C,
    [USB_KEY_ESCAPE] = 0x01,
    [USB_KEY_BACKSPACE] = 0x0E,
    [USB_KEY_TAB] = 0x0F,
    [USB_KEY_SPACE] = 0x39,
    [USB_KEY_MINUS] = 0x0C,
    [USB_KEY_EQUAL] = 0x0D,
    [USB_KEY_LEFTBRACE] = 0x1A,
    [USB_KEY_RIGHTBRACE] = 0x1B,
    [USB_KEY_BACKSLASH] = 0x2B,
    [USB_KEY_SEMICOLON] = 0x27,
    [USB_KEY_APOSTROPHE] = 0x28,
    [USB_KEY_GRAVE] = 0x29,
    [USB_KEY_COMMA] = 0x33,
    [USB_KEY_DOT] = 0x34,
    [USB_KEY_SLASH] = 0x35,
    [USB_KEY_CAPSLOCK] = 0x3A,
    [USB_KEY_F1] = 0x3B,
    [USB_KEY_F2] = 0x3C,
    [USB_KEY_F3] = 0x3D,
    [USB_KEY_F4] = 0x3E,
    [USB_KEY_F5] = 0x3F,
    [USB_KEY_F6] = 0x40,
    [USB_KEY_F7] = 0x41,
    [USB_KEY_F8] = 0x42,
    [USB_KEY_F9] = 0x43,
    [USB_KEY_F10] = 0x44,
    [USB_KEY_F11] = 0x57,
    [USB_KEY_F12] = 0x58,
    // Extended keys (will need 0xE0 prefix)
    [USB_KEY_HOME] = 0x47,
    [USB_KEY_END] = 0x4F,
    [USB_KEY_PAGEUP] = 0x49,
    [USB_KEY_PAGEDOWN] = 0x51,
    // Arrow keys use extended scancodes (0xE0 prefix)
    [USB_KEY_RIGHT] = 0x4D,  // Will need 0xE0 prefix
    [USB_KEY_LEFT] = 0x4B,   // Will need 0xE0 prefix
    [USB_KEY_DOWN] = 0x50,   // Will need 0xE0 prefix
    [USB_KEY_UP] = 0x48,     // Will need 0xE0 prefix
};

// Modifier key scancodes
#define SCANCODE_LEFT_CTRL    0x1D
#define SCANCODE_LEFT_SHIFT   0x2A
#define SCANCODE_LEFT_ALT     0x38
#define SCANCODE_RIGHT_CTRL   0x1D  // With 0xE0 prefix
#define SCANCODE_RIGHT_SHIFT  0x36
#define SCANCODE_RIGHT_ALT    0x38  // With 0xE0 prefix

int usb_keyboard_init(void) {
    GFX_LOG_MIN("USB Keyboard: Initializing...\n");
    SERIAL_LOG("USB Keyboard: Initializing...\n");
    
    // USB stack should already be initialized by usb_mouse_init()
    // Just mark that we're ready to accept keyboard probes
    g_usb_keyboard = NULL;
    memset(&last_report, 0, sizeof(usb_keyboard_report_t));
    
    GFX_LOG_MIN("USB Keyboard: Ready for device probing\n");
    return USB_KBD_OK;
}

int usb_keyboard_probe(usb_device_t *device) {
    SERIAL_LOG("USB Keyboard: Probing device for keyboard interface\n");
    
    if (!device || !device->config_desc) {
        return USB_KBD_ERR_USB;
    }
    
    // Parse configuration descriptor to find HID keyboard interface
    uint8_t *desc_data = (uint8_t *)device->config_desc;
    uint16_t total_length = device->config_desc->wTotalLength;
    uint16_t offset = sizeof(usb_config_descriptor_t);
    
    while (offset < total_length) {
        uint8_t length = desc_data[offset];
        uint8_t type = desc_data[offset + 1];
        
        if (type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)(desc_data + offset);
            
            // Check for HID class with boot protocol keyboard
            if (iface->bInterfaceClass == USB_CLASS_HID &&
                iface->bInterfaceSubClass == USB_HID_SUBCLASS_BOOT &&
                iface->bInterfaceProtocol == USB_HID_PROTOCOL_KEYBOARD) {
                
                SERIAL_LOG("USB Keyboard: Found HID keyboard interface\n");
                return usb_keyboard_attach_interface(device, iface);
            }
        }
        
        offset += length;
        if (length == 0) break; // Prevent infinite loop
    }
    
    return -1; // No keyboard interface found
}

int usb_keyboard_attach_interface(usb_device_t *device, usb_interface_descriptor_t *interface) {
    SERIAL_LOG("USB Keyboard: Attaching keyboard interface\n");
    
    // Detect SteelSeries and other gaming keyboards
    if (device && device->device_desc.idVendor && device->device_desc.idProduct) {
        uint16_t vendor = device->device_desc.idVendor;
        uint16_t product = device->device_desc.idProduct;
        
        SERIAL_LOG("USB Keyboard: Vendor ID: 0x");
        SERIAL_LOG_HEX("", vendor);
        SERIAL_LOG(" Product ID: 0x");
        SERIAL_LOG_HEX("", product);
        SERIAL_LOG("\n");
        
        if (is_steelseries_apex(vendor, product)) {
            SERIAL_LOG("USB Keyboard: *** STEELSERIES APEX KEYBOARD DETECTED ***\n");
            SERIAL_LOG("USB Keyboard: Using 600us timing for gaming keyboard\n");
        } else if (is_steelseries_keyboard(vendor, product)) {
            SERIAL_LOG("USB Keyboard: SteelSeries gaming keyboard detected\n");
        }
    }
    
    // Allocate HID device structure
    g_usb_keyboard = (usb_hid_device_t *)heap_alloc(sizeof(usb_hid_device_t));
    if (!g_usb_keyboard) {
        SERIAL_LOG("USB Keyboard: Failed to allocate HID device structure\n");
        return -1;
    }
    
    // Initialize HID device structure
    g_usb_keyboard->device = device;
    g_usb_keyboard->interface_num = interface->bInterfaceNumber;
    g_usb_keyboard->protocol = interface->bInterfaceProtocol;
    g_usb_keyboard->is_mouse = false;
    g_usb_keyboard->is_keyboard = true;
    
    // Find interrupt IN endpoint for keyboard reports
    if (usb_keyboard_find_endpoints(interface) != 0) {
        heap_free(g_usb_keyboard);
        g_usb_keyboard = NULL;
        return -1;
    }
    
    // Set boot protocol (simpler than report protocol)
    if (usb_hid_set_protocol(g_usb_keyboard, 0) != 0) { // 0 = boot protocol
        SERIAL_LOG("USB Keyboard: Warning - failed to set boot protocol\n");
    }
    
    // Set idle rate (0 = infinite, only send on change)
    if (usb_hid_set_idle(g_usb_keyboard, 0, 0) != 0) {
        SERIAL_LOG("USB Keyboard: Warning - failed to set idle rate\n");
    }
    
    // Start receiving keyboard reports
    usb_keyboard_start_polling();
    
    GFX_LOG_MIN("USB Keyboard: Successfully attached keyboard device\n");
    SERIAL_LOG("USB Keyboard: Successfully attached keyboard device\n");
    return 0;
}

int usb_keyboard_find_endpoints(usb_interface_descriptor_t *interface) {
    // Parse endpoint descriptors following the provided interface descriptor
    usb_device_t *dev = g_usb_keyboard->device;
    if (!dev || !dev->config_desc) {
        SERIAL_LOG("USB Keyboard: No device config descriptor available, using defaults\n");
        g_usb_keyboard->endpoint_in = 0x81;
        g_usb_keyboard->endpoint_out = 0x00;
        g_usb_keyboard->max_packet_size = 8;
        g_usb_keyboard->interval = 10;
        return 0;
    }

    uint8_t *cfg = (uint8_t *)dev->config_desc;
    uint16_t total = dev->config_desc->wTotalLength;

    // Compute offset of the interface descriptor within the config buffer
    uintptr_t base = (uintptr_t)cfg;
    uintptr_t ifptr = (uintptr_t)interface;
    if (ifptr < base || ifptr >= base + total) {
        // Fallback to defaults
        g_usb_keyboard->endpoint_in = 0x81;
        g_usb_keyboard->endpoint_out = 0x00;
        g_usb_keyboard->max_packet_size = 8;
        g_usb_keyboard->interval = 10;
        SERIAL_LOG("USB Keyboard: Interface pointer outside config buffer, using defaults\n");
        return 0;
    }

    uint16_t offset = (uint16_t)(ifptr - base) + sizeof(usb_interface_descriptor_t);
    g_usb_keyboard->endpoint_in = 0x00;
    g_usb_keyboard->endpoint_out = 0x00;
    g_usb_keyboard->max_packet_size = 0;
    g_usb_keyboard->interval = 0;

    while (offset + 2 <= total) {
        uint8_t bLength = cfg[offset];
        uint8_t bType = cfg[offset + 1];
        if (bLength == 0) break;

        if (bType == USB_DESC_ENDPOINT) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&cfg[offset];
            uint8_t ep_dir = ep->bEndpointAddress & 0x80;
            uint8_t ep_attr = ep->bmAttributes & 0x3;
            if (ep_attr == USB_TRANSFER_INTERRUPT && ep_dir == 0x80) {
                // Found interrupt IN endpoint
                g_usb_keyboard->endpoint_in = ep->bEndpointAddress;
                g_usb_keyboard->max_packet_size = ep->wMaxPacketSize;
                g_usb_keyboard->interval = ep->bInterval;
                SERIAL_LOG("USB Keyboard: Found interrupt IN endpoint ");
                SERIAL_LOG_HEX("", ep->bEndpointAddress);
                break;
            }
        }

        offset += bLength;
        if (offset >= total) break;
    }

    if (g_usb_keyboard->endpoint_in == 0) {
        SERIAL_LOG("USB Keyboard: Failed to find interrupt IN endpoint, using defaults\n");
        g_usb_keyboard->endpoint_in = 0x81;
        g_usb_keyboard->max_packet_size = 8;
        g_usb_keyboard->interval = 10;
    }

    return 0;
}

void usb_keyboard_start_polling(void) {
    SERIAL_LOG("USB Keyboard: Starting keyboard polling\n");
    // Polling will be handled by usb_keyboard_poll() called from timer or main loop
}

void usb_keyboard_poll(void) {
    // For now, USB keyboard polling is disabled
    // The PS/2 keyboard will handle all input
    // TODO: Implement proper asynchronous USB keyboard polling with callbacks
    return;
    
    #if 0
    if (!g_usb_keyboard || !g_usb_keyboard->device) {
        return;
    }
    
    // Read keyboard report from interrupt endpoint
    usb_keyboard_report_t report;
    memset(&report, 0, sizeof(report));
    
    int result = usb_interrupt_transfer(
        g_usb_keyboard->device,
        g_usb_keyboard->endpoint_in,
        &report,
        sizeof(report),
        NULL  // No callback for now, synchronous polling
    );
    
    if (result == 0) {
        // Process the report
        usb_keyboard_process_report(&report);
    }
    #endif
}

void usb_keyboard_process_report(usb_keyboard_report_t *report) {
    if (!report) return;
    
    // Check if report changed from last time
    if (memcmp(report, &last_report, sizeof(usb_keyboard_report_t)) == 0) {
        return; // No change, nothing to do
    }
    
    // Process modifier changes
    uint8_t mod_changes = report->modifiers ^ last_report.modifiers;
    
    if (mod_changes & USB_MOD_LEFT_CTRL) {
        uint8_t scancode = SCANCODE_LEFT_CTRL;
        if (!(report->modifiers & USB_MOD_LEFT_CTRL)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    if (mod_changes & USB_MOD_LEFT_SHIFT) {
        uint8_t scancode = SCANCODE_LEFT_SHIFT;
        if (!(report->modifiers & USB_MOD_LEFT_SHIFT)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    if (mod_changes & USB_MOD_LEFT_ALT) {
        uint8_t scancode = SCANCODE_LEFT_ALT;
        if (!(report->modifiers & USB_MOD_LEFT_ALT)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    if (mod_changes & USB_MOD_RIGHT_CTRL) {
        keyboard_process_scancode(0xE0);
        uint8_t scancode = SCANCODE_RIGHT_CTRL;
        if (!(report->modifiers & USB_MOD_RIGHT_CTRL)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    if (mod_changes & USB_MOD_RIGHT_SHIFT) {
        uint8_t scancode = SCANCODE_RIGHT_SHIFT;
        if (!(report->modifiers & USB_MOD_RIGHT_SHIFT)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    if (mod_changes & USB_MOD_RIGHT_ALT) {
        keyboard_process_scancode(0xE0);
        uint8_t scancode = SCANCODE_RIGHT_ALT;
        if (!(report->modifiers & USB_MOD_RIGHT_ALT)) {
            scancode |= 0x80; // Release
        }
        keyboard_process_scancode(scancode);
    }
    
    // Process key releases (keys in last_report but not in current report)
    for (int i = 0; i < 6; i++) {
        uint8_t old_key = last_report.keys[i];
        if (old_key == 0) continue;
        
        bool found = false;
        for (int j = 0; j < 6; j++) {
            if (report->keys[j] == old_key) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Key was released
            uint8_t scancode = usb_to_scancode[old_key];
            if (scancode != 0) {
                // Check if this is an extended key (arrow keys, PageUp/Down, Home/End)
                if ((old_key >= USB_KEY_HOME && old_key <= USB_KEY_PAGEDOWN) ||
                    (old_key >= USB_KEY_RIGHT && old_key <= USB_KEY_UP)) {
                    keyboard_process_scancode(0xE0);
                }
                keyboard_process_scancode(scancode | 0x80);  // Send release
            }
        }
    }
    
    // Process key presses (keys in current report but not in last_report)
    for (int i = 0; i < 6; i++) {
        uint8_t new_key = report->keys[i];
        if (new_key == 0) continue;
        
        bool found = false;
        for (int j = 0; j < 6; j++) {
            if (last_report.keys[j] == new_key) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Key was pressed
            uint8_t scancode = usb_to_scancode[new_key];
            if (scancode != 0) {
                // Check if this is an extended key (arrow keys, PageUp/Down, Home/End)
                if ((new_key >= USB_KEY_HOME && new_key <= USB_KEY_PAGEDOWN) ||
                    (new_key >= USB_KEY_RIGHT && new_key <= USB_KEY_UP)) {
                    keyboard_process_scancode(0xE0);
                }
                keyboard_process_scancode(scancode);  // Send press
            }
        }
    }
    
    // Save current report for next comparison
    memcpy(&last_report, report, sizeof(usb_keyboard_report_t));
}

uint8_t usb_key_to_scancode(uint8_t usb_key, uint8_t modifiers) {
    if (usb_key >= sizeof(usb_to_scancode)) {
        return 0;
    }
    return usb_to_scancode[usb_key];
}

// XHCI keyboard initialization
typedef struct {
    void* controller;
    uint8_t slot;
    uint8_t padding[59];  // Pad to 64 bytes so report_buffer is aligned
    usb_keyboard_report_t report_buffer __attribute__((aligned(64)));
} xhci_keyboard_t;

static xhci_keyboard_t *g_xhci_keyboard = NULL;
static bool keyboard_initialization_complete = false;

void usb_keyboard_init_xhci(void* controller, uint8_t slot) {
    SERIAL_LOG("USB Keyboard: Initializing XHCI keyboard\n");
    
    // Allocate XHCI keyboard structure with DMA-safe aligned buffer
    extern void* dma_allocator_alloc(size_t size, size_t alignment, size_t boundary);
    g_xhci_keyboard = (xhci_keyboard_t*)dma_allocator_alloc(sizeof(xhci_keyboard_t), 64, 0);
    if (!g_xhci_keyboard) {
        SERIAL_LOG("USB Keyboard: Failed to allocate XHCI keyboard structure\n");
        return;
    }
    
    g_xhci_keyboard->controller = controller;
    g_xhci_keyboard->slot = slot;
    memset(&g_xhci_keyboard->report_buffer, 0, sizeof(usb_keyboard_report_t));
    
    SERIAL_LOG("USB Keyboard: XHCI keyboard initialized on slot ");
    SERIAL_LOG_HEX("", slot);
    SERIAL_LOG("\n");
    
    // Configure endpoint 1 (interrupt IN) for keyboard
    SERIAL_LOG("[USB_KEYBOARD] Configuring endpoint...\n");
    extern int xhci_configure_endpoint(void *xhci, uint8_t slot);
    if (xhci_configure_endpoint(controller, slot) != 0) {
        SERIAL_LOG("[USB_KEYBOARD] ERROR: Failed to configure endpoint\n");
        return;
    }
    SERIAL_LOG("[USB_KEYBOARD] Endpoint configured successfully\n");
    
    // Don't queue transfer yet - wait for init to complete
    SERIAL_LOG("[USB_KEYBOARD] Init complete, buffer at ");
    SERIAL_LOG_HEX("", (uint32_t)&g_xhci_keyboard->report_buffer);
    SERIAL_LOG("\n");
    keyboard_initialization_complete = true;
}

void usb_keyboard_poll_xhci(void) {
    static int poll_count = 0;
    if (poll_count < 5) {
        SERIAL_LOG("[USB_KEYBOARD] Poll called\n");
        poll_count++;
    }
    
    if (!g_xhci_keyboard) return;
    
    // Queue initial transfers on first poll after init complete
    // Use only 2-3 transfers to avoid flooding the system
    static bool transfers_queued = false;
    if (keyboard_initialization_complete && !transfers_queued) {
        extern int xhci_queue_transfer(void *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
        for (int i = 0; i < 2; i++) {
            xhci_queue_transfer(g_xhci_keyboard->controller, g_xhci_keyboard->slot, 1,
                              &g_xhci_keyboard->report_buffer, sizeof(usb_keyboard_report_t));
        }
        SERIAL_LOG("[USB_KEYBOARD] Queued 2 initial transfers after init\n");
        SERIAL_LOG("[USB_KEYBOARD] Buffer address: 0x");
        SERIAL_LOG_HEX("", (uint32_t)&g_xhci_keyboard->report_buffer);
        SERIAL_LOG("\n");
        transfers_queued = true;
    }
    
    // Called periodically from main loop
    extern void xhci_poll_events(void *xhci);
    xhci_poll_events(g_xhci_keyboard->controller);
    
    // USB HID keyboards (like SteelSeries) also benefit from proper timing
    // Use same 600-800µs delay as mice for stable HID communication
    sleep_us(USB_HID_MOUSE_WAIT_MIN_US);
}

void usb_keyboard_process_xhci_data(uint8_t slot) {
    if (!g_xhci_keyboard || g_xhci_keyboard->slot != slot) return;
    
    // Don't requeue during initialization
    if (!keyboard_initialization_complete) {
        return;
    }
    
    uint8_t mod = g_xhci_keyboard->report_buffer.modifiers;
    uint8_t key0 = g_xhci_keyboard->report_buffer.keys[0];
    
    if (mod != 0 || key0 != 0) {
        SERIAL_LOG("USB KB: Report mod=");
        SERIAL_LOG_HEX("", mod);
        SERIAL_LOG(" key0=");
        SERIAL_LOG_HEX("", key0);
        SERIAL_LOG("\n");
    }
    
    // Process the report that was just received
    usb_keyboard_process_report(&g_xhci_keyboard->report_buffer);
    
    // Add microsecond delay for stable USB HID keyboard communication
    // SteelSeries and other gaming keyboards benefit from proper timing
    sleep_us(USB_HID_MOUSE_WAIT_MIN_US);
    
    // Queue next transfer
    extern int xhci_queue_transfer(void *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
    xhci_queue_transfer(g_xhci_keyboard->controller, slot, 1, 
                       &g_xhci_keyboard->report_buffer, sizeof(usb_keyboard_report_t));
}
