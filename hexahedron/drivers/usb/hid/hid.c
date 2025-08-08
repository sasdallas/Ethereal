/**
 * @file hexahedron/drivers/usb/hid/hid.c
 * @brief USB human interface device handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/hid/hid.h>
#include <kernel/debug.h>
#include <string.h>

/**
 * @brief Initialize USB device
 * @param intf The interface to initialize on
 */
USB_STATUS hid_initializeDevice(USBInterface_t *intf) {
    // TODO
    return USB_FAILURE;
}

/**
 * @brief Register and initialize HID drivers
 */
void hid_init() {
    USBDriver_t *d = usb_createDriver();
    d->name = "USB HID Driver";
    d->dev_init = hid_initializeDevice;
    d->weak_bind = 0;
}