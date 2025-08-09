/**
 * @file hexahedron/drivers/usb/driver.c
 * @brief Driver (for specific USB devices) handler
 * 
 * @warning Some of this, specifically priorities/binds, is really weird and hacky. If you have a problem, please report it - thank you
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <structs/list.h>
#include <string.h>

/* Device driver list */
list_t *usb_driver_list = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:DRIVER", __VA_ARGS__)



/**
 * @brief Create a new device driver structure
 * @returns A new @c USBDriver_t structure
 */
USBDriver_t *usb_createDriver() {
    USBDriver_t *driver = kmalloc(sizeof(USBDriver_t));
    memset(driver, 0, sizeof(USBDriver_t));
    return driver;
}

/**
 * @brief Internal method to check a USB device against a specific driver
 * @param driver The driver
 * @param dev The device
 * 
 * @returns @c USB_SUCCESS on success
 */
static USB_STATUS usb_driverInitializeDevice(USBDriver_t *driver, USBDevice_t *dev, USBInterface_t *intf) {
    if (!driver || !dev || !intf) return USB_FAILURE;

    if (driver->find) {
        // Check parameters
        if (driver->find->classcode && intf->desc.bInterfaceClass != driver->find->classcode) return USB_FAILURE;
        if (driver->find->subclasscode && intf->desc.bInterfaceSubClass != driver->find->subclasscode) return USB_FAILURE;
        if (driver->find->protocol && intf->desc.bInterfaceProtocol != driver->find->protocol) return USB_FAILURE;
    }


    // Create a copy of the driver
    USBDriver_t *driver_clone = kmalloc(sizeof(USBDriver_t));
    memcpy((void*)driver_clone, (void*)driver, sizeof(USBDriver_t));
    
    // If there's already a driver given to the device, make sure this driver and the one that's currently loaded isn't a weak bind
    if (intf->driver) {
        // No find parameters? Skip this driver.
        if (!driver->find) goto _error;

        // We know that this driver specifically wants this device, and that there's already a driver loaded onto it

        if (driver->weak_bind) {
            // The current driver has a weak bind, meaning we can assume the currently loaded driver is better.
            goto _error;
        } else if (intf->driver->weak_bind) {
            // The currently loaded driver has a weak bind, meaning we can do a weird unload/load sequence
            // !!!: Clean this up
            USBDriver_t *prev_driver = intf->driver; // Save a copy of the previous driver
            if (intf->driver->dev_deinit(intf) != USB_SUCCESS) {
                LOG(WARN, "Failed to deinitialize driver '%s' from device (loading new driver '%s')\n", prev_driver->name, driver->name);
            }

            intf->driver = driver_clone;

            if (driver->dev_init(intf) != USB_SUCCESS) {
                // The new driver failed to initialize, try to fallback to the old one.
                LOG(WARN, "Failed to initialize driver '%s', fallback to previous driver '%s'\n", driver->name, prev_driver->name);
                prev_driver->dev_init(intf);
                intf->driver = prev_driver;
                goto _error;
            }

            return USB_SUCCESS;
        } else {
            // Both drivers do not have weak binds or both do have weak binds. What the hell do we do?
            // This cannot be resolved easily in any way I can think of without a full USB stack restructure, and god knows I am not doing that.
            LOG(ERR, "Collision detected while initializing USB driver.");
            LOG(ERR, "Device loaded driver '%s' has a weak bind, but driver '%s' matches find parameters and also does/doesnot a weak bind.\n", intf->driver->name, driver->name);
            LOG(ERR, "This situation cannot be resolved with the current USB stack structure. Please contact the developer.\n");
            goto _error;
        }
    }

    // Try to initialize
    if (driver->dev_init) {
        // Prematurely assign intf->driver
        USBDriver_t *prev_driver = intf->driver;
        intf->driver = driver_clone;

        USB_STATUS ret = driver->dev_init(intf);
        if (ret == USB_SUCCESS) {
            return ret;
        }

        intf->driver = prev_driver; // Reassign old driver and fall through to _error
    }
 
_error:
    kfree(driver_clone);
    return USB_FAILURE;
}

/**
 * @brief Register your driver and use it to find devices
 * @param driver The driver to register
 * @returns @c USB_SUCCESS on success
 */
USB_STATUS usb_registerDriver(USBDriver_t *driver) {
    if (!driver) return USB_FAILURE;

    // Add it to the list
    list_append(usb_driver_list, (void*)driver);

    // Now get all devices and try them all.
    // !!!: Hacky

extern list_t *usb_controller_list;
    foreach(contnode, usb_controller_list) {
        USBController_t *cont = (USBController_t*)(contnode->value);
        if (!cont || !cont->devices->head) continue;

        foreach(devnode, cont->devices) {
            USBDevice_t *dev = (USBDevice_t*)(devnode->value);
            if (!dev) continue;
            
            if (driver->find->vid && dev->device_desc.idVendor != driver->find->vid) continue;
            if (driver->find->pid && dev->device_desc.idProduct != driver->find->pid) continue; // I mean, this is kinda weird if you have a NULL VID but want a PID, but whatever
            

            // Now we need to iterate through the interfaces to find the correct one
            foreach(intf_node, dev->config->interface_list) {
                USBInterface_t *intf = (USBInterface_t*)(intf_node->value);
                if (!intf) continue;
                if (usb_driverInitializeDevice(driver, dev, intf) == USB_SUCCESS) {
                    return USB_SUCCESS; // All done
                }       
            }
        }
    }

    return USB_SUCCESS;
}


/**
 * @brief Try to initialize a device using currently registered drivers
 * @param dev The device to try and initialize
 * 
 * @note This is specific to the USB stack
 * @returns USB_SUCCESS on a found driver and USB_FAILURE on a failure.
 */
USB_STATUS usb_initializeDeviceDriver(USBDevice_t *dev) {
    int initialized_interfaces = 0;

    foreach(drv_node, usb_driver_list) {
        USBDriver_t *driver = (USBDriver_t*)(drv_node->value);
        if (!driver) continue;

        // Check VID/PID
        if (driver->find) {
            if (driver->find->vid && dev->device_desc.idVendor != driver->find->vid) continue;
            if (driver->find->pid && dev->device_desc.idProduct != driver->find->pid) continue;
        }

        // Now we need to iterate through the interfaces to find the correct one
        foreach(intf_node, dev->config->interface_list) {
            USBInterface_t *intf = (USBInterface_t*)(intf_node->value);
            if (!intf) continue;
            if (usb_driverInitializeDevice(driver, dev, intf) == USB_SUCCESS) {
                initialized_interfaces++;
            }       
        }
        
    }

    LOG(INFO, "Initialized %d interfaces\n", initialized_interfaces);

    return USB_SUCCESS;
}