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
#include <kernel/fs/periphfs.h>
#include <kernel/debug.h>
#include <stdio.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USBKBD", __VA_ARGS__)

#define SCANCODE_INVALID -1

/* Scancode conversion */
static char usbkbd_scancode_table_lower[] = {
    SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID,
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
    'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '\n', SCANCODE_ESC, '\b', '\t', ' ', '-', '=', '[', ']',
    '\\', SCANCODE_INVALID, ';', '\'', '`', ',', '.', '/', SCANCODE_INVALID, // "..." and Caps Lock ???
    SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5,
    SCANCODE_F6, SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
    SCANCODE_F11, SCANCODE_F12, SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID,
    // TODO: I do not care about the rest
    [225] = SCANCODE_LEFT_SHIFT,
    [229] = SCANCODE_RIGHT_SHIFT,
};

/* Scancode conversion */
static char usbkbd_scancode_table_upper[] = {
    SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID,
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '\n', SCANCODE_ESC, '\b', '\t', ' ', '_', '+', '{', '}',
    '|', SCANCODE_INVALID, ':', '\"', '~', '<', '>', '?', SCANCODE_INVALID, // "..." and Caps Lock ???
    SCANCODE_F1, SCANCODE_F2, SCANCODE_F3, SCANCODE_F4, SCANCODE_F5,
    SCANCODE_F6, SCANCODE_F7, SCANCODE_F8, SCANCODE_F9, SCANCODE_F10,
    SCANCODE_F11, SCANCODE_F12, SCANCODE_INVALID, SCANCODE_INVALID, SCANCODE_INVALID,
    // TODO: I do not care about the rest
    [225] = SCANCODE_LEFT_SHIFT,
    [229] = SCANCODE_RIGHT_SHIFT,
};

/**
 * @brief Keyboard device loop thread
 * @param ctx The @c usbkbd_t structure
 */
void usbkbd_thread(void *ctx) {
    usbkbd_t *kbd = (usbkbd_t*)ctx;
    
    // TODO: Temporary while API adds interrupt 
    kbd->intf->dev->interrupt(kbd->intf->dev->c, kbd->intf->dev, &kbd->transfer);
    while (1) {
        while (kbd->transfer.status == USB_TRANSFER_IN_PROGRESS) arch_pause();
        kbd->transfer.status = USB_TRANSFER_IN_PROGRESS;
        
        int auto_repeat_flag = 0;

        // Let's print!
        for (int i = 2; i < 8; i++) {
            char ch = usbkbd_scancode_table_lower[kbd->data.data[i]];        
            if (kbd->data.data[0] & KBD_MOD_LEFT_SHIFT || kbd->data.data[0] & KBD_MOD_RIGHT_SHIFT) {
                // Use upper scancodes
                ch = usbkbd_scancode_table_upper[kbd->data.data[i]];
            }

            if (ch == SCANCODE_INVALID) continue;

            if (memchr(kbd->last_data.data + 2, kbd->data.data[i], 6)) {
                // Duplicate
                auto_repeat_flag = 1;
                if (kbd->auto_repeat_wait) continue; 
            }

            periphfs_sendKeyboardEvent(EVENT_KEY_PRESS, ch);
        }      

        if (!auto_repeat_flag) {
            kbd->auto_repeat_wait = KBD_DEFAULT_WAIT;
        } else if (kbd->auto_repeat_wait && auto_repeat_flag) {
            kbd->auto_repeat_wait--;
        }

        // Check to update?
        memcpy(&kbd->last_data, &kbd->data, sizeof(usb_kbd_data_t));
        kbd->intf->dev->interrupt(kbd->intf->dev->c, kbd->intf->dev, &kbd->transfer);
        arch_pause();
    }
}

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
    kbd->intf = intf;
    kbd->transfer.endp = kbd->endp;
    kbd->transfer.req = 0;
    kbd->transfer.length = 8;
    kbd->transfer.data = (void*)kbd->data.data;
    kbd->transfer.status = USB_TRANSFER_IN_PROGRESS;
    kbd->auto_repeat_wait = KBD_DEFAULT_WAIT;

    intf->driver->s = (void*)kbd;

    kbd->proc = process_createKernel("usbkbd_poller", PROCESS_KERNEL, PRIORITY_HIGH, usbkbd_thread, (void*)kbd);
    scheduler_insertThread(kbd->proc->main_thread);
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
