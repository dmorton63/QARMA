#include "usb.h"
#include "usb_hid.h"
#include "usb_mouse.h"
#include "usb_msc.h"
#include "uhci.h"
#include "core/memory/heap.h"
#include "config.h"
#include "graphics/graphics.h"

// Global USB device list
static usb_device_t *usb_device_list = NULL;
static uint8_t next_address = 1;

// USB Host Controller (simplified - would need actual OHCI/EHCI/XHCI implementation)
static bool usb_host_initialized = false;

int usb_init(void) {
    GFX_LOG_MIN("USB: Starting USB subsystem initialization\n");
    
    // Initialize host controller
    if (usb_host_controller_init() != 0) {
        GFX_LOG_MIN("USB: Failed to initialize host controller\n");
        return -1;
    }
    
    usb_host_initialized = true;
    GFX_LOG_MIN("USB: USB subsystem initialized successfully\n");
    return 0;
}

int usb_host_controller_init(void) {
    GFX_LOG_MIN("USB: Host controller initialization\n");

    int count = uhci_pci_init();
    if (count <= 0) {
        GFX_LOG_MIN("USB: No UHCI controllers found\n");
        return -1;
    }

    return 0;
}


//static uint8_t usb_next_address = 1;

int usb_enumerate_devices(void) {
    SERIAL_LOG("USB: Starting device enumeration with crash detection\n");
    //uhci_log_ts();  // This function may not exist in this context
    
    // Try to enumerate devices on USB ports - this is where the crash happens
    for (int port = 0; port < 2; port++) {  // UHCI typically has 2 ports
        SERIAL_LOG("USB: Attempting to enumerate port ");
        //SERIAL_LOG_DEC("", port);  // Use simple logging
        SERIAL_LOG("0\n");
        
        usb_device_t *device = usb_enumerate_device(&g_uhci_controllers[0], port);
        if (device) {
            SERIAL_LOG("USB: Device found on port\n");
            // We don't store the device, just test enumeration
        }
        
        // Only try port 0 for crash testing
        break;
    }
    
    return 0;
}

usb_device_t* usb_enumerate_device(uhci_controller_t *uhci, int port) {
    // Immediate parameter validation with detailed logging
    SERIAL_LOG_HEX("USB: usb_enumerate_device called with uhci=", (uint32_t)uhci);
    SERIAL_LOG_DEC(" port=", port);
    SERIAL_LOG("\n");
    
    if (!uhci) {
        SERIAL_LOG("USB: ERROR - uhci controller is NULL\n");
        return NULL;
    }
    if (port > 1 || port < 0) {
        SERIAL_LOG_DEC("USB: ERROR - invalid port number ", port);
        SERIAL_LOG("\n");
        return NULL;
    }

    // Reset and enable port
    if (!uhci_reset_port(uhci, port)) {
        SERIAL_LOG("USB: Port reset/enable failed - skipping enumeration\n");
        return NULL;
    }
    if (!uhci_port_device_connected(uhci, port)) {
        SERIAL_LOG("USB: Device disconnected after reset\n");
        return NULL;
    }
    SERIAL_LOG_DEC("USB: Starting enumeration on UHCI port ",port);
    SERIAL_LOG("\n");
    // Allocate device and zero memory
    usb_device_t *device = heap_alloc(sizeof(usb_device_t));
    if (!device) {
        SERIAL_LOG("USB: ERROR - heap_alloc failed\n");
        return NULL;
    }
    
    // Zero the structure to prevent garbage values
    for (int i = 0; i < sizeof(usb_device_t); i++) {
        ((uint8_t*)device)[i] = 0;
    }

    device->controller = uhci;
    device->port = port;
    device->address = next_address++;
    device->state = USB_STATE_DEFAULT;
    
    // Immediate validation after assignment
    SERIAL_LOG_DEC("USB: Assigned port=", device->port);
    SERIAL_LOG_DEC(" address=", device->address);
    SERIAL_LOG_DEC(" state=", device->state);
    SERIAL_LOG_HEX(" controller=", (uint32_t)device->controller);
    SERIAL_LOG("\n");
    
    // Sanity check - if any of these fail, memory is corrupted
    if (device->port != port || device->state != USB_STATE_DEFAULT || device->controller != uhci) {
        SERIAL_LOG("USB: CRITICAL - Device structure corrupted immediately after assignment!\n");
        SERIAL_LOG_DEC("USB: Expected port=", port);
        SERIAL_LOG_DEC(" got port=", device->port);
        SERIAL_LOG("\n");
        heap_free(device);
        return NULL;
    }
    
    // Detect device speed from port status  
    uint16_t port_reg = uhci->io_base + UHCI_PORTSC1 + port * 2;
    uint16_t port_status;
    __asm__ volatile ("inw %1, %0" : "=a"(port_status) : "Nd"(port_reg));
    device->speed = (port_status & UHCI_PORT_LSDA) ? USB_SPEED_LOW : USB_SPEED_FULL;
    
    SERIAL_LOG_HEX("USB: Port status = ", port_status);
    if (device->speed == USB_SPEED_LOW) {
        SERIAL_LOG("USB: Detected LOW-SPEED device\n");
    } else {
        SERIAL_LOG("USB: Detected FULL-SPEED device\n");
    }

    // Get device descriptor (address 0) with retries — some devices need extra time after reset
    usb_device_t temp = *device;
    temp.address = 0;
    SERIAL_LOG("USB: Starting two-stage device descriptor enumeration\n");
    
    // Stage 1: Get first 8 bytes to determine bMaxPacketSize0
    uint8_t partial_desc[8];
    const int max_attempts = 3;
    int desc_ok = -1;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        SERIAL_LOG("USB: Attempting partial device descriptor (8 bytes)\n");
        if (usb_get_descriptor(&temp, USB_DESC_DEVICE, 0, partial_desc, 8) == 0) {
            desc_ok = 0;
            SERIAL_LOG("USB: Partial device descriptor succeeded\n");
            break;
        }
        SERIAL_LOG("USB: Partial device descriptor failed, retrying\n");
        // Wait a bit before retrying; increase the wait slightly per attempt
        uhci_delay_ms(20 * (attempt + 1));
    }
    if (desc_ok < 0) {
        SERIAL_LOG("USB: Failed to get partial device descriptor\n");
        heap_free(device);
        return NULL;
    }
    
    // Parse bMaxPacketSize0 from byte 7
    uint8_t max_packet_size = partial_desc[7];
    SERIAL_LOG_HEX("USB: Device bMaxPacketSize0=", max_packet_size);
    
    // Stage 2: Get full device descriptor
    desc_ok = -1;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        SERIAL_LOG("USB: Attempting full device descriptor (18 bytes)\n");
        if (usb_get_descriptor(&temp, USB_DESC_DEVICE, 0, &device->device_desc, sizeof(usb_device_descriptor_t)) == 0) {
            desc_ok = 0;
            SERIAL_LOG("USB: Full device descriptor succeeded\n");
            break;
        }
        SERIAL_LOG("USB: Full device descriptor failed, retrying\n");
        // Wait a bit before retrying; increase the wait slightly per attempt
        uhci_delay_ms(20 * (attempt + 1));
    }
    if (desc_ok < 0) {
        heap_free(device);
        return NULL;
    }

    /* Dump device descriptor for diagnostics (use message_box_logf for reliable output) */
    /* Also emit descriptor summary to serial for capture in QEMU logs */
    SERIAL_LOG_HEX("USB: Device bcdUSB=", device->device_desc.bcdUSB);
    SERIAL_LOG_HEX("USB: idVendor=", device->device_desc.idVendor);
    SERIAL_LOG_HEX("USB: idProduct=", device->device_desc.idProduct);
    SERIAL_LOG_HEX("USB: Device class=", device->device_desc.bDeviceClass);
    SERIAL_LOG_HEX("USB: subClass=", device->device_desc.bDeviceSubClass);
    SERIAL_LOG_HEX("USB: protocol=", device->device_desc.bDeviceProtocol);
    /* Keep the message box output as-is for on-screen diagnostics */
    message_box_logf("USB: Device Descriptor bcdUSB=0x%x idVendor=0x%x idProduct=0x%x\n",
                     device->device_desc.bcdUSB,
                     device->device_desc.idVendor,
                     device->device_desc.idProduct);
    message_box_logf("USB: Device class=0x%x subClass=0x%x protocol=0x%x\n",
                     device->device_desc.bDeviceClass,
                     device->device_desc.bDeviceSubClass,
                     device->device_desc.bDeviceProtocol);

    // Enable the port only after the device successfully responded to the initial descriptor request
    // This follows UHCI guidance: set PortEnable after reset completes and device responds.
    uhci_enable_port(uhci, port);
    SERIAL_LOG_DEC("USB: Assigned address ",device->address);
    SERIAL_LOG("\n");
    // Set address
    usb_setup_packet_t set_addr = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_ADDRESS,
        .wValue = device->address,
        .wIndex = 0,
        .wLength = 0
    };
    if (usb_control_transfer(&temp, &set_addr, NULL, 0) < 0) {
        heap_free(device);
        return NULL;
    }

    device->state = USB_STATE_ADDRESS;

    // Get configuration descriptor
    device->config_desc = heap_alloc(256);
    if (!device->config_desc) {
        heap_free(device);
        return NULL;
    }

    if (usb_get_descriptor(device, USB_DESC_CONFIG, 0, device->config_desc, 256) < 0) {
        heap_free(device->config_desc);
        heap_free(device);
        return NULL;
    }

    /* Dump configuration and contained descriptors */
    if (device->config_desc) {
        uint8_t *buf = (uint8_t *)device->config_desc;
        uint16_t total = device->config_desc->wTotalLength;
       /* Mirror config summary to serial so it appears in the QEMU serial log */
       SERIAL_LOG_DEC("USB: Config wTotalLength=", total);
       SERIAL_LOG_DEC("USB: bNumInterfaces=", device->config_desc->bNumInterfaces);
       SERIAL_LOG_DEC("USB: bConfigurationValue=", device->config_desc->bConfigurationValue);
       /* Keep message box output too */
       message_box_logf("USB: Config wTotalLength=%u bNumInterfaces=%u bConfigurationValue=%u\n",
           total,
           device->config_desc->bNumInterfaces,
           device->config_desc->bConfigurationValue);

        uint16_t offset = 0;
        while (offset + 2 <= total) {
            uint8_t bLength = buf[offset];
            uint8_t bType = buf[offset + 1];
            if (bLength == 0) break;

            /* Print a brief line to serial as well as message box */
            SERIAL_LOG_DEC("USB: Descriptor offset=", offset);
            SERIAL_LOG_HEX(" USB: type=", bType);
            SERIAL_LOG_DEC(" USB: len=", bLength);
            message_box_logf("USB: Descriptor at offset=%u type=0x%x len=%u\n", offset, bType, bLength);

            switch (bType) {
                case USB_DESC_INTERFACE: {
                    usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&buf[offset];
                    SERIAL_LOG_DEC(" USB: Interface number=", iface->bInterfaceNumber);
                    SERIAL_LOG_HEX(" class=", iface->bInterfaceClass);
                    SERIAL_LOG_HEX(" subClass=", iface->bInterfaceSubClass);
                    SERIAL_LOG_HEX(" protocol=", iface->bInterfaceProtocol);
                    SERIAL_LOG_DEC(" numEndpoints=", iface->bNumEndpoints);
                    message_box_logf(" USB: Interface number=%u class=0x%x subClass=0x%x protocol=0x%x numEndpoints=%u\n",
                                     iface->bInterfaceNumber,
                                     iface->bInterfaceClass,
                                     iface->bInterfaceSubClass,
                                     iface->bInterfaceProtocol,
                                     iface->bNumEndpoints);
                } break;
                case USB_DESC_ENDPOINT: {
                    usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&buf[offset];
                    SERIAL_LOG_HEX(" USB: Endpoint addr=", ep->bEndpointAddress);
                    SERIAL_LOG_HEX(" attr=", ep->bmAttributes);
                    SERIAL_LOG_DEC(" maxpkt=", ep->wMaxPacketSize);
                    SERIAL_LOG_DEC(" interval=", ep->bInterval);
                    message_box_logf(" USB: Endpoint addr=0x%x attr=0x%x maxpkt=%u interval=%u\n",
                                     ep->bEndpointAddress,
                                     ep->bmAttributes,
                                     ep->wMaxPacketSize,
                                     ep->bInterval);
                } break;
                default:
                    /* Other descriptor types: just print header */
                    break;
            }

            offset += bLength;
        }
    }

    // Set configuration
    if (usb_set_configuration(device, device->config_desc->bConfigurationValue) < 0) {
        heap_free(device->config_desc);
        heap_free(device);
        return NULL;
    }

    
    device->state = USB_STATE_CONFIGURED;
    // Probe for class drivers (MSC, HID, etc.)
    usb_msc_probe(device);
    return device;
}



// usb_device_t* usb_create_mock_mouse_device(void) {
//     // Create a mock USB mouse device for testing
//     usb_device_t *device = (usb_device_t *)heap_alloc(sizeof(usb_device_t));
//     if (!device) {
//         return NULL;
//     }
    
//     // Initialize device structure
//     device->address = next_address++;
//     device->port = 1;
//     device->state = USB_STATE_CONFIGURED;
//     device->speed = USB_SPEED_FULL;
//     device->next = NULL;
//     device->controller = NULL;  // Mock device - no real controller
    
//     // Mock device descriptor (typical USB mouse values)
//     device->device_desc.bLength = sizeof(usb_device_descriptor_t);
//     device->device_desc.bDescriptorType = USB_DESC_DEVICE;
//     device->device_desc.bcdUSB = 0x0200; // USB 2.0
//     device->device_desc.bDeviceClass = 0x00;
//     device->device_desc.bDeviceSubClass = 0x00;
//     device->device_desc.bDeviceProtocol = 0x00;
//     device->device_desc.bMaxPacketSize0 = 64;
//     device->device_desc.idVendor = 0x1532;   // Razer
//     device->device_desc.idProduct = 0x0042;  // Generic mouse ID
//     device->device_desc.bcdDevice = 0x0100;
//     device->device_desc.iManufacturer = 1;
//     device->device_desc.iProduct = 2;
//     device->device_desc.iSerialNumber = 0;
//     device->device_desc.bNumConfigurations = 1;
    
//     // Create mock configuration descriptor
//     device->config_desc = (usb_config_descriptor_t *)heap_alloc(sizeof(usb_config_descriptor_t) + 64);
//     if (!device->config_desc) {
//         heap_free(device);
//         return NULL;
//     }
    
//     device->config_desc->bLength = sizeof(usb_config_descriptor_t);
//     device->config_desc->bDescriptorType = USB_DESC_CONFIG;
//     device->config_desc->wTotalLength = sizeof(usb_config_descriptor_t) + 
//                                        sizeof(usb_interface_descriptor_t) +
//                                        sizeof(usb_hid_descriptor_t) +
//                                        sizeof(usb_endpoint_descriptor_t);
//     device->config_desc->bNumInterfaces = 1;
//     device->config_desc->bConfigurationValue = 1;
//     device->config_desc->iConfiguration = 0;
//     device->config_desc->bmAttributes = 0x80; // Bus powered
//     device->config_desc->bMaxPower = 50;      // 100mA
    
//     // Add mock interface descriptor after config descriptor
//     uint8_t *desc_ptr = (uint8_t *)device->config_desc + sizeof(usb_config_descriptor_t);
//     usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)desc_ptr;
    
//     iface->bLength = sizeof(usb_interface_descriptor_t);
//     iface->bDescriptorType = USB_DESC_INTERFACE;
//     iface->bInterfaceNumber = 0;
//     iface->bAlternateSetting = 0;
//     iface->bNumEndpoints = 1;
//     iface->bInterfaceClass = USB_CLASS_HID;
//     iface->bInterfaceSubClass = USB_HID_SUBCLASS_BOOT;
//     iface->bInterfaceProtocol = USB_HID_PROTOCOL_MOUSE;
//     iface->iInterface = 0;
    
//     SERIAL_LOG("USB: Created mock mouse device\n");
//     return device;
// }

usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id) {
    usb_device_t *device = usb_device_list;
    
    while (device) {
        if (device->device_desc.idVendor == vendor_id &&
            device->device_desc.idProduct == product_id) {
            return device;
        }
        device = device->next;
    }
    
    return NULL;
}

int usb_control_transfer(usb_device_t *device, usb_setup_packet_t *setup, void *data, uint16_t length) {
    SERIAL_LOG("USB: IMMEDIATE ENTRY TO usb_control_transfer\n");
    SERIAL_LOG("USB: About to call uhci_control_transfer\n");
    return uhci_control_transfer(device->controller, device, setup, data, length);
}

int usb_interrupt_transfer(usb_device_t *device,
                           uint8_t endpoint,
                           void *data,
                           uint16_t length,
                           void (*callback)(usb_transfer_t *)) {
    if (!device) {
        SERIAL_LOG("USB: Invalid device for interrupt transfer\n");
        return -1;
    }

    // For mock devices or when no UHCI controller is assigned, simulate success
    if (!device->controller) {
        SERIAL_LOG("USB: Starting interrupt transfer (mock)\n");
        return 0;
    }

    // Forward to UHCI implementation
    return uhci_interrupt_transfer(device->controller,
                                   device,
                                   endpoint,
                                   data,
                                   length,
                                   callback);
}



int usb_get_descriptor(usb_device_t *device, uint8_t type, uint8_t index, void *buffer, uint16_t length) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (type << 8) | index,
        .wIndex = 0,
        .wLength = length
    };
    return usb_control_transfer(device, &setup, buffer, length);
}

int usb_set_configuration(usb_device_t *device, uint8_t config) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x00,
        .bRequest = USB_REQ_SET_CONFIG,
        .wValue = config,
        .wIndex = 0,
        .wLength = 0
    };
    int result = usb_control_transfer(device, &setup, NULL, 0);
    if (result == 0) device->state = USB_STATE_CONFIGURED;
    return result;
}