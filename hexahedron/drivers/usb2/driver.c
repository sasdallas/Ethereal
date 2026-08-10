/**
 * @file hexahedron/drivers/usb2/driver.c
 * @brief USB driver manager
 * 
 * @todo The attach code really needs a cleanup
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>

/* Drivers */
mutex_t usb_drivers_lock = MUTEX_INITIALIZER;
SLIST_HEAD(usb_drivers, usb_driver_t);

extern DLIST_HEAD(usb_devices, usb_device_t);
extern mutex_t usb_device_lock;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:DRIVER", __VA_ARGS__)

/**
 * @brief Attempt to attach a driver to an interface
 * @param driver The driver to attach
 * @param intf The interface to attach it to
 */
static usb_status_t usb_attachToInterface(usb_driver_t *driver, usb_interface_t *intf) {
    // TODO: locking
    int match = driver->ops.match(intf->device, intf);
    if (match == USB_MATCH_NONE) return USB_SUCCESS;

    if (intf->driver != NULL) {
        int prev_match = intf->driver->ops.match(intf->device, intf);

        if (prev_match >= match) {
            return USB_SUCCESS;
        }

        usb_status_t status = intf->driver->ops.detach(intf->device, intf);
        if (USB_ERROR(status)) {
            // ??? what do we do here?
            LOG(WARN, "Failed to detach driver \"%s\" from interface: %s\n", intf->driver->name, usb_strerror(status));

            // we ball, that's we do. continue on.
        }
        
        status = driver->ops.attach(intf->device, intf);

        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to attach better driver \"%s\" on interface: %s\n", driver->name, usb_strerror(status));

            // ??? now what
            usb_driver_t *old = intf->driver;
            intf->driver = NULL;
            return usb_attachToInterface(old, intf);
        }

        intf->driver = driver;
    } else {
        usb_status_t status = driver->ops.attach(intf->device, intf);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to attach driver \"%s\" on interface: %s\n", driver->name, usb_strerror(status));
            return status;
        }

        intf->driver = driver;
    }

    LOG(INFO, "Attached driver \"%s\" to interface %d\n", driver->name, intf->desc.bInterfaceNumber);

    return USB_SUCCESS;
}

/**
 * @brief Try to attach driver to device
 * @param driver The driver to attach
 * @param device The device to attach it to
 */
static usb_status_t usb_attachToDevice(usb_driver_t *driver, usb_device_t *device) {
    int score = driver->ops.match(device, NULL);
    if (score <= USB_MATCH_GENERIC) {
        return USB_INVALID;
    }

    assert(0 && "unimpl");
} 

/**
 * @brief Register a new USB driver
 * @param driver The driver to register
 */
void usb_registerDriver(usb_driver_t *driver) {
    LOG(INFO, "Adding new USB driver \"%s\"\n", driver->name);
    mutex_acquire(&usb_drivers_lock);
    SLIST_INSERT_HEAD(&usb_drivers, driver, node);
    mutex_release(&usb_drivers_lock);

    mutex_acquire(&usb_device_lock);
    usb_device_t *device = DLIST_FIRST(&usb_devices);
    while (device != NULL) {
        if (usb_attachToDevice(driver, device) == USB_SUCCESS) {
            // The device was claimed.
            goto _next_device;
        }

        if (device->num_configs == 0 || !device->selected) {
            goto _next_device;
        }

        for (unsigned i = 0; i < device->selected->num_interfaces; i++) {
            usb_interface_t *intf = device->selected->interfaces[i];
            
            usb_status_t status = usb_attachToInterface(driver, intf);
            if (USB_ERROR(status)) {
                // An error occurred while attaching the driver, what do we do?
                LOG(ERR, "Failed to attach driver \"%s\" to interface %p\n", driver->name, intf);
            }
        }

    _next_device:
        device = DLIST_NEXT(device, node);
    }
    mutex_release(&usb_device_lock);
}

/**
 * @brief Find and attach driver for new device
 * @param dev The device to attach a driver for
 */
void usb_attachDriver(usb_device_t *device) {
    usb_configuration_t *config = device->selected;
    if (config == NULL) {
        // no config = no drivers
        return;
    }

    mutex_acquire(&usb_drivers_lock);
    
    // Try to match the entire device first
    SLIST_FOREACH(usb_driver_t, driver, &usb_drivers, node) {
        if (usb_attachToDevice(driver, device) == USB_SUCCESS) {
            mutex_release(&usb_drivers_lock);
            return;
        }
    }

    // Now iterate through interfaces
    for (unsigned i = 0; i < config->num_interfaces; i++) {
        usb_interface_t *intf = config->interfaces[i];
        usb_driver_t *best = NULL;
        int best_score = USB_MATCH_NONE;
        SLIST_FOREACH(usb_driver_t, driver, &usb_drivers, node) {
            int score = driver->ops.match(device, intf);
            if (score > best_score) {
                best_score = score;
                best = driver;
            }
        }

        if (best) {
            usb_attachToInterface(best, intf);
        } else {
            LOG(INFO, "No driver detected for interface %d\n", intf->desc.bInterfaceNumber);
        }
    }

    mutex_release(&usb_drivers_lock);
}
