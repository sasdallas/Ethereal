/**
 * @file hexahedron/include/kernel/drivers/usb2/driver.h
 * @brief USB driver subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_USB_DRIVER_H
#define DRIVERS_USB_DRIVER_H

/**** INCLUDES ****/
#include <kernel/drivers/usb2/status.h>
#include <structs/list.h>
#include <stdint.h>

/**** DEFINITIONS ****/

// TODO: These will need an update if the stack gets more featured
#define USB_MATCH_NONE                          0
#define USB_MATCH_GENERIC                       1
#define USB_MATCH_INTF_CLASS_GENERIC            2
#define USB_MATCH_INTF_CLASS                    3
#define USB_MATCH_INTF_CLASS_SUBCLASS           4
#define USB_MATCH_INTF_CLASS_SUBCLASS_PROTO     5
#define USB_MATCH_VENDOR_INTF_SUBCLASS          6
#define USB_MATCH_VENDOR_INTF_SUBCLASS_PROTO    7
#define USB_MATCH_VENDOR_PRODUCT                8
#define USB_MATCH_VENDOR_PRODUCT_REV            9
#define USB_MATCH_EXACT                         10

/**** TYPES ****/

struct usb_interface;
struct usb_device;

typedef struct usb_driver {
    char *name;
    SLIST_ENTRY(struct usb_driver) node;

    struct {
        int (*match)(struct usb_device *, struct usb_interface *);
        usb_status_t (*attach)(struct usb_device *, struct usb_interface *);
        usb_status_t (*detach)(struct usb_device *, struct usb_interface *);
    } ops;
} usb_driver_t;

#endif
