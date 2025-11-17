#include "usb_hid.h"
#include "usb_mouse.h"
#include "core/memory/heap.h"
#include "core/stdtools.h"
// #include "graphics/serial_console.h"
#include "config.h"

int usb_hid_init(void) {
    SERIAL_LOG("USB HID: Initializing HID subsystem\n");
    
    // Initialize HID-specific structures and state
    // This would set up report parsing, protocol handling, etc.
    
    return 0;
}

int usb_hid_probe_device(usb_device_t *device) {
    if (!device) {
        return -1;
    }
    
    SERIAL_LOG("USB HID: Probing device for HID interfaces\n");
    
    // This would examine the device's configuration descriptor
    // to find HID interfaces and determine their type (mouse, keyboard, etc.)
    
    // For our mouse implementation, this is handled in usb_mouse_probe()
    return usb_mouse_probe(device);
}

int usb_hid_set_protocol(usb_hid_device_t *hid_dev, uint8_t protocol) {
    if (!hid_dev || !hid_dev->device) {
        return -1;
    }
    
    SERIAL_LOG("USB HID: Setting protocol\n");
    
    // Create setup packet for SET_PROTOCOL request
    usb_setup_packet_t setup;
    setup.bmRequestType = 0x21; // Class, Interface, Host to Device
    setup.bRequest = HID_REQ_SET_PROTOCOL;
    setup.wValue = protocol;    // 0 = Boot Protocol, 1 = Report Protocol
    setup.wIndex = hid_dev->interface_num;
    setup.wLength = 0;
    
    // Send control transfer
    int result = usb_control_transfer(hid_dev->device, &setup, NULL, 0);
    if (result == 0) {
        hid_dev->protocol = protocol;
    }
    
    return result;
}

int usb_hid_set_idle(usb_hid_device_t *hid_dev, uint8_t duration, uint8_t report_id) {
    if (!hid_dev || !hid_dev->device) {
        return -1;
    }
    
    SERIAL_LOG("USB HID: Setting idle rate\n");
    
    // Create setup packet for SET_IDLE request
    usb_setup_packet_t setup;
    setup.bmRequestType = 0x21; // Class, Interface, Host to Device
    setup.bRequest = HID_REQ_SET_IDLE;
    setup.wValue = (duration << 8) | report_id; // Duration in upper byte, report ID in lower
    setup.wIndex = hid_dev->interface_num;
    setup.wLength = 0;
    
    // Send control transfer
    return usb_control_transfer(hid_dev->device, &setup, NULL, 0);
}

int usb_hid_get_report_descriptor(usb_hid_device_t *hid_dev) {
    if (!hid_dev || !hid_dev->device) {
        return -1;
    }
    
    SERIAL_LOG("USB HID: Getting report descriptor\n");
    
    // For boot protocol devices, we often don't need the report descriptor
    // as we know the format. But for completeness:
    
    if (hid_dev->report_desc_length > 0) {
        // Allocate buffer for report descriptor
        hid_dev->report_descriptor = (uint8_t *)heap_alloc(hid_dev->report_desc_length);
        if (!hid_dev->report_descriptor) {
            return -1;
        }
        
        // Create setup packet for GET_DESCRIPTOR request
        usb_setup_packet_t setup;
        setup.bmRequestType = 0x81; // Standard, Interface, Device to Host
        setup.bRequest = USB_REQ_GET_DESCRIPTOR;
        setup.wValue = (USB_DESC_REPORT << 8) | 0; // Report descriptor, index 0
        setup.wIndex = hid_dev->interface_num;
        setup.wLength = hid_dev->report_desc_length;
        
        // Send control transfer
        return usb_control_transfer(hid_dev->device, &setup, 
                                  hid_dev->report_descriptor, 
                                  hid_dev->report_desc_length);
    }
    
    return 0;
}