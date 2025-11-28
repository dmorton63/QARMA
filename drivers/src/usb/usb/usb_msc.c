#include "usb_msc.h"
#include "uhci.h"
#include "usb.h"
#include "usb_hid.h"
#include "usb_mouse.h"
#include "core/memory/heap.h"
#include "graphics/graphics.h"
#include "config.h"
#include <string.h>

// SCSI / BOT constants
#define CBW_SIGNATURE 0x43425355U
#define CSW_SIGNATURE 0x53425355U

// Helper to send a CBW, data, then read CSW using UHCI bulk transfers
static int usb_msc_send_command(usb_device_t *dev, uint8_t ep_out, uint8_t ep_in, void *cmd, uint8_t cmd_len, void *data, uint32_t data_len, uint8_t flags, uint32_t tag)
{
    if (!dev || !dev->controller) return -1;
    uhci_controller_t *uhci = dev->controller;

    // Build CBW (31 bytes)
    uint8_t cbw[31];
    memset(cbw, 0, sizeof(cbw));
    *(uint32_t *)&cbw[0] = CBW_SIGNATURE;
    *(uint32_t *)&cbw[4] = tag; // dCBWTag
    *(uint32_t *)&cbw[8] = data_len; // dCBWDataTransferLength
    cbw[12] = flags; // bmCBWFlags (0x80 = IN)
    cbw[13] = 0; // bCBWLUN
    cbw[14] = cmd_len; // bCBWCBLength
    // Copy command into CBWCB (offset 15)
    for (int i = 0; i < cmd_len && i < 16; ++i) cbw[15 + i] = ((uint8_t *)cmd)[i];

    // Send CBW (OUT)
    if (uhci_bulk_transfer(uhci, dev, UHCI_TD_PID_OUT, ep_out, cbw, sizeof(cbw)) != 0) {
        SERIAL_LOG("USB-MSC: Failed to send CBW\n");
        return -1;
    }

    // Data stage
    if (data_len > 0) {
        if (flags & 0x80) {
            // IN transfer: read into data
            if (uhci_bulk_transfer(uhci, dev, UHCI_TD_PID_IN, ep_in, data, data_len) != 0) {
                SERIAL_LOG("USB-MSC: Failed IN data stage\n");
                return -1;
            }
        } else {
            // OUT transfer: send data
            if (uhci_bulk_transfer(uhci, dev, UHCI_TD_PID_OUT, ep_out, data, data_len) != 0) {
                SERIAL_LOG("USB-MSC: Failed OUT data stage\n");
                return -1;
            }
        }
    }

    // Read CSW (13 bytes)
    uint8_t csw[13];
    if (uhci_bulk_transfer(uhci, dev, UHCI_TD_PID_IN, ep_in, csw, sizeof(csw)) != 0) {
        SERIAL_LOG("USB-MSC: Failed to read CSW\n");
        return -1;
    }

    uint32_t sig = *(uint32_t *)&csw[0];
    if (sig != CSW_SIGNATURE) {
        SERIAL_LOG_HEX("USB-MSC: Bad CSW signature: ", sig);
        SERIAL_LOG("\n");
        return -1;
    }

    // status is csw[12]
    uint8_t status = csw[12];
    if (status != 0) {
        SERIAL_LOG_HEX("USB-MSC: CSW status=", status);
        SERIAL_LOG("\n");
        return -1;
    }

    return 0;
}

void usb_msc_probe(usb_device_t *device)
{
    if (!device || !device->config_desc) return;
    uint16_t total = device->config_desc->wTotalLength;
    uint8_t *buf = (uint8_t *)device->config_desc;
    uint16_t offset = sizeof(usb_config_descriptor_t);

    SERIAL_LOG_HEX("USB-MSC: config wTotalLength=", total);
    SERIAL_LOG("\n");
    // Dump first 16 bytes for debugging
    for (int i = 0; i < 16 && i < total; ++i) {
        SERIAL_LOG_HEX(" ", buf[i]);
    }
    SERIAL_LOG("\n");

    int found = 0;
    uint8_t ep_in = 0, ep_out = 0;

    while (offset + 2 <= total) {
        uint8_t len = buf[offset];
        uint8_t type = buf[offset + 1];
        if (len < 2) break;

        if (type == USB_DESC_INTERFACE && offset + sizeof(usb_interface_descriptor_t) <= total) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)&buf[offset];
            if (iface->bInterfaceClass == 0x08) {
                SERIAL_LOG("USB-MSC: Found Mass Storage interface\n");
                found = 1;
            }
        } else if (type == USB_DESC_ENDPOINT && offset + sizeof(usb_endpoint_descriptor_t) <= total) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&buf[offset];
            uint8_t dir = ep->bEndpointAddress & 0x80;
            uint8_t attr = ep->bmAttributes & 0x3;
            if (found && attr == USB_TRANSFER_BULK) {
                if (dir == USB_DIR_IN) ep_in = ep->bEndpointAddress & 0x0F;
                else ep_out = ep->bEndpointAddress & 0x0F;
                SERIAL_LOG_HEX("USB-MSC: Endpoint found addr=", ep->bEndpointAddress);
                SERIAL_LOG("\n");
            }
        }

        offset += len;
    }

    if (!found || ep_in == 0 || ep_out == 0) {
        // Nothing to do
        return;
    }

    SERIAL_LOG("USB-MSC: Probing device: issuing INQUIRY\n");

    uint8_t inquiry_cmd[6] = {0x12, 0, 0, 0, 36, 0};
    uint8_t inquiry_data[36];
    memset(inquiry_data, 0, sizeof(inquiry_data));

    // Use tag 0x1
    if (usb_msc_send_command(device, ep_out, ep_in, inquiry_cmd, sizeof(inquiry_cmd), inquiry_data, sizeof(inquiry_data), 0x80, 1) == 0) {
        SERIAL_LOG("USB-MSC: INQUIRY success, vendor/product:\n");
        // Print vendor and product strings from inquiry data
        char vendor[9] = {0}, product[17] = {0};
        for (int i = 8; i < 16; ++i) vendor[i - 8] = inquiry_data[i];
        for (int i = 16; i < 32; ++i) product[i - 16] = inquiry_data[i];
        SERIAL_LOG("  Vendor: "); SERIAL_LOG(vendor); SERIAL_LOG("\n");
        SERIAL_LOG("  Product: "); SERIAL_LOG(product); SERIAL_LOG("\n");
    } else {
        SERIAL_LOG("USB-MSC: INQUIRY failed\n");
    }

    // Try READ(10) of LBA 0 (512 bytes)
    uint8_t read10[10] = {0x28, 0, 0,0,0,0, 0,0,1,0}; // LBA=0, transfer 1 block
    uint8_t lba0[512];
    memset(lba0, 0, sizeof(lba0));
    if (usb_msc_send_command(device, ep_out, ep_in, read10, sizeof(read10), lba0, sizeof(lba0), 0x80, 2) == 0) {
        SERIAL_LOG("USB-MSC: READ(10) LBA 0 success, first 32 bytes:\n");
        for (int i = 0; i < 32; i++) {
            SERIAL_LOG_HEX(" ", lba0[i]);
        }
        SERIAL_LOG("\n");
    } else {
        SERIAL_LOG("USB-MSC: READ(10) failed\n");
    }
}
