// Minimal USB Mass Storage (MSC) probe header
#ifndef USB_MSC_H
#define USB_MSC_H

#include "usb.h"

// Probe a device for MSC interfaces and run a small verification if found.
void usb_msc_probe(usb_device_t *device);

#endif // USB_MSC_H
