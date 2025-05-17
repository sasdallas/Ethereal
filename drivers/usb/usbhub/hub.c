/**
 * @file drivers/usbhub/hub.c
 * @brief USB Hub driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "hub.h"

#include <kernel/loader/driver.h>
#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:USBHUB", __VA_ARGS__)

/* Helper macro */
#define HUB_SET_FEATURE(hub, port, feature) usb_controlTransfer(hub->intf->dev, USB_RT_H2D | USB_RT_CLASS | USB_RT_OTHER, HUB_REQ_SET_FEATURE, feature, port+1, 0, NULL)

/**
 * @brief Probe hub for ports
 * @param hub The hub to probe
 */
USB_STATUS usbhub_probe(USBHub_t *hub) {
    if (hub->nports == 0) return 0;

    // The port may not be powered. This will be the case if wHubCharacteristics has bit 1 set
    if ((hub->desc.wHubCharacteristics & 0x3) == 0x2) {
        LOG(DEBUG, "Individual port power detected - powering up hub ports...\n");
        for (uint32_t port = 0; port < hub->nports; port++) {
            if (HUB_SET_FEATURE(hub, port, HUB_FEATURE_PORT_POWER) != USB_SUCCESS) {
                LOG(ERR, "Failed to power up port %d\n", port+1);
                return USB_FAILURE;
            }

            // Wait for completion
            clock_sleep(hub->desc.bPowerOnGood * 2); // bPowerOnGood is in units of 2ms
        }
    }

    for (uint32_t port = 0; port < hub->nports; port++) {
        // Now we need to reset the port, similar to UHCI
        // Set PORT_RESET (we can't directly manipulate PORT_ENABLE)
        if (HUB_SET_FEATURE(hub, port, HUB_FEATURE_PORT_RESET) != USB_SUCCESS) {
            LOG(ERR, "Failed to enable port %d\n", port+1);
            return USB_FAILURE;
        }


        uint32_t port_status = 0;

        for (int t = 0; t < 10; t++) {
            // 100ms in 10ms increments
            clock_sleep(10);

            // Read in current state
            if (usb_controlTransfer(hub->intf->dev, USB_RT_D2H | USB_RT_CLASS | USB_RT_OTHER, HUB_REQ_GET_STATUS, 0, port+1, sizeof(uint32_t), &port_status) != USB_SUCCESS) {
                LOG(ERR, "Could not read port %d status\n", port+1);
                return USB_FAILURE;
            }

            if (!(port_status & HUB_PORT_STATUS_CONNECTION)) {
                // Nothing is connected to the port
                break;
            }

            if (port_status & HUB_PORT_STATUS_ENABLE) {
                // Port enable
                LOG(DEBUG, "Found device connected to hub port %d\n", port+1);
                break;
            }
        }

        // Did the enable bit set?
        if (port_status & HUB_PORT_STATUS_ENABLE) {
            // Yes, create a device and initialize it
            uint32_t port_speed = (port_status & HUB_PORT_STATUS_LOW_SPEED) ? USB_LOW_SPEED : ((port_status & HUB_PORT_STATUS_HIGH_SPEED) ? USB_HIGH_SPEED : USB_FULL_SPEED);
            USBDevice_t *dev = usb_createDevice(hub->intf->dev->c, port, port_speed, hub->intf->dev->shutdown, hub->intf->dev->control, hub->intf->dev->interrupt);
            dev->mps = 8; // TODO: Bochs says to make this equal the mps corresponding to the speed of the device
            if (!dev) {
                LOG(ERR, "Failed to create device for port %d\n", port+1);
                return USB_FAILURE;
            }

            if (usb_initializeDevice(dev) == USB_FAILURE) {
                // Failed to initialize
                usb_deinitializeDevice(dev);
                usb_destroyDevice(hub->intf->dev->c, dev); 
                continue;
            }

            // Insert into list
            list_append(hub->hub_ports, (void*)dev);
        }

    }  

    return USB_SUCCESS;
}

/**
 * @brief Hub initialize device
 */
USB_STATUS usbhub_initializeDevice(USBInterface_t *intf) {
    // Read the hub descriptor in
    USBHub_t *hub = kmalloc(sizeof(USBHub_t));
    hub->hub_ports = list_create("usb hub ports");
    if (usb_getDescriptor(intf->dev, USB_RT_CLASS, USB_DESC_HUB, 0, sizeof(USBHubDescriptor_t), &hub->desc) != USB_SUCCESS) {
        // Failed
        list_destroy(hub->hub_ports, false);
        kfree(hub);
        LOG(ERR, "Error while trying to get USB hub descriptor\n");
        return USB_FAILURE;
    }

    hub->nports = hub->desc.bNbrPorts;
    hub->intf = intf;

    // Now we've read the hub descriptor, probe it for ports
    if (usbhub_probe(hub) != USB_SUCCESS) {
        list_destroy(hub->hub_ports, false);
        kfree(hub);
        LOG(ERR, "Error while trying to initialize hub ports\n");
        return USB_FAILURE;
    }

    // Set the interface driver structure
    intf->driver->s = (void*)hub;
    return USB_SUCCESS;
}

/**
 * @brief Hub deinitialize device
 */
USB_STATUS usbhub_deinitializeDevice(USBInterface_t *intf) {
    // !!!: We have to also deinitialize all devices connected to the hub, and likely also free their memory.
    // !!!: This is to allow for the next USB hub driver (or whatever it is) to reinitialize everything properly.
    return USB_FAILURE;
}

/**
 * @brief Hub initialize
 */
int usbhub_initialize(int argc, char **argv) {
    // Create driver structure
    USBDriver_t *driver = usb_createDriver();
    if (!driver) {
        LOG(ERR, "Failed to allocate driver\n");
        return -1;
    }

    // Now setup fields
    driver->name = strdup("Hexahedron USB Hub Driver");
    driver->find = kmalloc(sizeof(USBDriverFindParameters_t));
    memset(driver->find, 0, sizeof(USBDriverFindParameters_t));
    driver->find->classcode = HUB_CLASS_CODE; // 0x09 = USB_HUB_CLASSCODE

    driver->dev_init = usbhub_initializeDevice;
    driver->dev_deinit = usbhub_deinitializeDevice;

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
 * @brief Hub deinitialize
 */
int usbhub_deinitialize() {
    return 0;
}

/* Metadata */
struct driver_metadata driver_metadata = {
    .name = "USB Hub Driver",
    .author = "Samuel Stuart",
    .init = usbhub_initialize,
    .deinit = usbhub_deinitialize
};