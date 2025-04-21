/**
 * @file drivers/usb/usbkbd/kbd.c
 * @brief USB keyboard driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "kbd.h"
#include <kernel/drivers/usb/api.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/loader/driver.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USBKBD", __VA_ARGS__)

/**
 * @brief Initialize keyboard device
 * @param intf The interface
 */
USB_STATUS usbkbd_initializeDevice(USBInterface_t *intf) {
    LOG(DEBUG, "Initializing a keyboard device...\n");
    
    // Configure the device to only send interrupt reports when data changes
    if (usb_requestDevice(intf->dev, USB_RT_H2D | USB_RT_CLASS | USB_RT_INTF, 
                        HID_REQ_SET_IDLE, 0, intf->desc.bInterfaceNumber, 0, NULL) != USB_TRANSFER_SUCCESS) {
        LOG(ERR, "Could not ask HID device to enter idle mode\n");
        return USB_TRANSFER_FAILED;
    }

    // Allocate keyboard structure
    usbkbd_t *kbd = kmalloc(sizeof(usbkbd_t));
    memset(kbd, 0, sizeof(usbkbd_t));

    // Find an endpoint of type "INTERRUPT IN"
    foreach(endp_node, intf->endpoint_list) {
        USBEndpoint_t *endp = (USBEndpoint_t*)endp_node->value;
        if (endp->desc.bmAttributes & USB_ENDP_TRANSFER_INT && endp->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) {
            // INTERRUPT IN endpoint found!
            kbd->transfer.endpoint = endp->desc.bEndpointAddress & USB_ENDP_NUMBER; 
            kbd->endp = endp;
            LOG(DEBUG, "Found proper endpoint %d\n", endp->desc.bEndpointAddress & USB_ENDP_NUMBER);
            break;
        } 
    }  

    if (!kbd->endp) {
        LOG(ERR, "No INTERRUPT IN endpoint found\n");
        kfree(kbd);
        return USB_FAILURE;
    }

    
    // Setup remaining transfer stuff
    usb_kbd_data_t *data = kmalloc(sizeof(usb_kbd_data_t));
    memset(data, 0, sizeof(usb_kbd_data_t));

    kbd->transfer.endp = kbd->endp;
    kbd->transfer.req = 0;
    kbd->transfer.length = 8;
    kbd->transfer.data = (void*)data;
    kbd->transfer.status = USB_TRANSFER_IN_PROGRESS;

    // TODO: Temporary while API adds interrupt 
    intf->dev->interrupt(intf->dev->c, intf->dev, &kbd->transfer);
    while (1) {
        while (kbd->transfer.status == USB_TRANSFER_IN_PROGRESS);
        kbd->transfer.status = USB_TRANSFER_IN_PROGRESS;
        for (uint8_t i = 0; i < 8; i++) {
            printf("%02x ", data->data[i]);
        }

        printf("\n");
        intf->dev->interrupt(intf->dev->c, intf->dev, &kbd->transfer);
    }

    intf->driver->s = (void*)kbd;
    return USB_SUCCESS;
}



/**
 * @brief Deinitialize keyboard device
 * @param intf The interface
 */
USB_STATUS usbkbd_deinitializeDevice(USBInterface_t *intf) {
    return USB_FAILURE;
}

/**
 * @brief Driver init method
 */
int driver_init(int argc, char **argv) {
    // Create driver structure
    USBDriver_t *driver = usb_createDriver();
    if (!driver) {
        LOG(ERR, "Failed to allocate driver");
        return 1;
    }

    // Now setup fields
    driver->name = strdup("Hexahedron USB Keyboard Driver");
    driver->find = kmalloc(sizeof(USBDriverFindParameters_t));
    memset(driver->find, 0, sizeof(USBDriverFindParameters_t));
    driver->find->classcode = KBD_CLASS;
    driver->find->subclasscode = KBD_SUBCLASS;
    driver->find->protocol = KBD_PROTOCOL;

    driver->dev_init = usbkbd_initializeDevice;
    driver->dev_deinit = usbkbd_deinitializeDevice;

    // Register driver
    if (usb_registerDriver(driver)) {
        kfree(driver->name);
        kfree(driver->find);
        kfree(driver);
        LOG(ERR, "Failed to register driver.\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Driver deinit method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "USB Keyboard Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
