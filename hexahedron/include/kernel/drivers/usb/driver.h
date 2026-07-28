/**
 * @file hexahedron/include/kernel/drivers/usb/driver.h
 * @brief USB driver framework
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_DRIVER_H
#define DRIVERS_USB_DRIVER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>
#include <kernel/drivers/usb/dev.h>
#include <kernel/drivers/usb/status.h>

/**** DEFINITIONS ****/


/**** TYPES ****/

/**
 * @brief Find parameters for a device
 * 
 * @param vid A specific vendor ID to look for. Can be 0 to not look for one
 * @param pid A specific product ID to look for. Can be 0 to not look for one
 * @param classcode The class code to look for IN THE INTERFACE. Can be 0 to not look for one
 * @param subclasscode The subclass code to look for IN THE INTERFACE. Can be 0 to not look for one.
 * @param protocol A protocol to look for IN THE INTERFACE. Can be 0 to not look for one. 
 */
typedef struct USBDriverFindParameters {
    uint16_t vid;           // Vendor ID to look for. Can be 0.
    uint16_t pid;           // Product ID to look for. Can be 0.
    uint8_t classcode;      // Class code to look for in the interface. Can be 0.
    uint8_t subclasscode;   // Subclass code to look for in the interface. Can be 0.
    uint8_t protocol;       // Protocol to look for in the interface. Can be 0.
} USBDriverFindParameters_t;

/**
 * @brief Device check/initialize method
 * @param intf The interface to deinitialize
 * 
 * @note This will be called multiple times. Verify the device found is the correct device.
 * @returns @c USB_SUCCESS on a successfully initialized device
 */
typedef USB_STATUS (*driver_dev_init_t)(USBInterface_t *intf);

/**
 * @brief Device deinitialize method. This should just unload and free any driver-specific structures related to this device.
 * @param intf The interface to deinitialize
 * 
 * @note This device may be claimed by another driver after deinitialization.
 * @returns @c USB_SUCCESS on a successfully deinitialized device
 */
typedef USB_STATUS (*driver_dev_deinit_t)(USBInterface_t *intf);

struct USBDriver;

/**
 * @brief USB driver opsstruct USBDriver
 */
typedef struct USBDriverOps {
    // Initialize a device
    USB_STATUS (*init)(USBInterface_t *intf);
    
    // Deinitialize a device
    USB_STATUS (*deinit)(USBInterface_t *intf);

    // Match device, returns true if driver is compatible
    bool (*match)(struct USBDriver *driver, USBInterface_t *intf);
} USBDriverOps_t;


/**
 * @brief USB driver structure
 * 
 * If you are writing a USB driver for a specific type of device, e.g. mass-storage devices or human interface devices,
 * you MUST register your driver with the framework. Call @c usb_registerDriver and have your @c dev_init method return a USB
 * status code (as defined in the USB header file, @c USB_SUCCESS or whatnot)
 * 
 * This driver structure gets duplicated so you can store device-specific information in the S field
 * 
 * Bind settings must be configured to handle collisions. 
 * @warning There is a small bit of priority besides weak binds. Providing find parameters will cause a higher tendency for your driver to get the device
 */
typedef struct USBDriver {
    char *name;                         // Optional name
    int weight;                         // Used to calculate the most optimal drievr
    USBDriverOps_t *ops;                // Driver operations
    USBDriverFindParameters_t *find;    // Optional find parameters.

    void *s;                            // Driver-specific structure
} USBDriver_t;

/**** VARIABLES ****/

extern list_t *usb_driver_list; // !!!: The only purpose of this is so that we don't clutter this header file and confuse users with a function to initialize this.

/**** FUNCTIONS ****/

/**
 * @brief Create a new device driver structure
 * @returns A new @c USBDriver_t structure
 */
USBDriver_t *usb_createDriver();

/**
 * @brief Register your driver and use it to find devices
 * @param driver The driver to register
 * @returns @c USB_SUCCESS on success
 */
USB_STATUS usb_registerDriver(USBDriver_t *driver);

/**
 * @brief Try to initialize a device using currently registered drivers
 * @param dev The device to try and initialize
 * 
 * @note This is specific to the USB stack
 * @returns USB_SUCCESS on a found driver and USB_FAILURE on a failure.
 */
USB_STATUS usb_initializeDeviceDriver(USBDevice_t *dev);

#endif