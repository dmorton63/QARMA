#ifndef USB_H
#define USB_H

#include "core/stdtools.h"

// USB Standard Request Codes
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIG        0x08
#define USB_REQ_SET_CONFIG        0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B

// USB Descriptor Types
#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIG           0x02
#define USB_DESC_STRING           0x03
#define USB_DESC_INTERFACE        0x04
#define USB_DESC_ENDPOINT         0x05
#define USB_DESC_HID              0x21
#define USB_DESC_REPORT           0x22

// USB Device Classes
#define USB_CLASS_HID             0x03

// HID Subclass Codes
#define USB_HID_SUBCLASS_BOOT     0x01

// HID Protocol Codes
#define USB_HID_PROTOCOL_KEYBOARD 0x01
#define USB_HID_PROTOCOL_MOUSE    0x02

// USB Transfer Types
#define USB_TRANSFER_CONTROL      0x00
#define USB_TRANSFER_ISOCHRONOUS  0x01
#define USB_TRANSFER_BULK         0x02
#define USB_TRANSFER_INTERRUPT    0x03

// USB Device Speeds
#define USB_SPEED_LOW             0x00
#define USB_SPEED_FULL            0x01
#define USB_SPEED_HIGH            0x02

// USB Endpoint Directions
#define USB_DIR_OUT               0x00
#define USB_DIR_IN                0x80

// USB Device States
typedef enum {
    USB_STATE_DETACHED = 0,
    USB_STATE_ATTACHED,
    USB_STATE_POWERED,
    USB_STATE_DEFAULT,
    USB_STATE_ADDRESS,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED
} usb_device_state_t;

// USB Device Descriptor
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_descriptor_t;

// USB Configuration Descriptor
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_config_descriptor_t;

// USB Interface Descriptor
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

// USB Endpoint Descriptor
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_descriptor_t;

// USB Setup Packet
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

// USB Device Structure
typedef struct usb_device {
    uint8_t address;
    uint8_t port;
    usb_device_state_t state;
    usb_device_descriptor_t device_desc;
    usb_config_descriptor_t *config_desc;
    uint8_t speed;
    struct usb_device *next;
    struct uhci_controller *controller; // Pointer to the host controller managing this device
} usb_device_t;

// USB Transfer Structure
typedef struct usb_transfer {
    usb_device_t *device;
    uint8_t endpoint;
    uint8_t type;
    uint8_t direction;
    void *buffer;
    uint32_t length;
    uint32_t actual_length;
    int status;
    void (*callback)(struct usb_transfer *transfer);
    void *context;
} usb_transfer_t;

// Function prototypes
int usb_init(void);
int usb_host_controller_init(void);
int usb_enumerate_devices(void);
usb_device_t* usb_enumerate_device(struct uhci_controller *uhci, int port);
usb_device_t* usb_create_mock_mouse_device(void);
usb_device_t* usb_find_device(uint16_t vendor_id, uint16_t product_id);
int usb_control_transfer(usb_device_t *device, usb_setup_packet_t *setup, void *data, uint16_t length);
int usb_interrupt_transfer(usb_device_t *device, uint8_t endpoint, void *data, uint16_t length, void (*callback)(usb_transfer_t *));
int usb_get_descriptor(usb_device_t *device, uint8_t desc_type, uint8_t desc_index, void *buffer, uint16_t length);
int usb_set_configuration(usb_device_t *device, uint8_t config);

#endif // USB_H