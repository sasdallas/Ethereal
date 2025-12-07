/**
 * @file hexahedron/include/kernel/drivers/usb/usb.h
 * @brief Universal Serial Bus driver
 * 
 * @note This USB driver is mostly sourced from the previous Ethereal Operating System. It will likely need to be expanded and added upon. 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_USB_H
#define DRIVERS_USB_USB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>

// USB
#include <kernel/drivers/usb/dev.h>
#include <kernel/drivers/usb/driver.h>
#include <kernel/drivers/usb/status.h>
#include <kernel/drivers/usb/api.h>

#include <kernel/fs/kernelfs.h>

/**** TYPES ****/

// Prototype
struct USBController;

/**
 * @brief Poll method for USB controller
 * 
 * @warning Currently unused as of Jan 16, 2025
 * 
 * @param controller The controller
 */
typedef void (*usb_poll_t)(struct USBController *controller);

/**
 * @brief Initialize device on hub port method
 * @param hub_device The USB device of the hub
 * @param port_number The port number to initialize
 * @param speed Speed of USB device
 */
typedef int (*usb_hub_init_t)(USBDevice_t *hub_device, uint32_t port_number, uint32_t speed);

/**
 * @brief USB controller structure
 * 
 * Normal USB drivers do not need to register this. This is for host controller
 * drivers, such as xHCI/EHCI
 */
typedef struct USBController {
    uint32_t id;                // ID of this USB controller
    void *hc;                   // Pointer to the host controller structure
    usb_hub_init_t hub_init;    // Initialize device on hub port
    list_t *devices;            // List of USB devices with a maximum of 127
    uint32_t last_address;      // Last address given to a device. Starts at 0x1
} USBController_t;

/**** VARIABLES ****/

extern kernelfs_dir_t *usb_kernelfs;

/**** FUNCTIONS ****/

/**
 * @brief Create a USB controller
 * 
 * @param hc The host controller
 * @returns A @c USBController_t structure
 */
USBController_t *usb_createController(void *hc);

/**
 * @brief Register a new USB controller
 * 
 * @param controller The controller to register
 */
void usb_registerController(USBController_t *controller);

/**
 * @brief Initialize a USB device and assign to the USB controller's list of devices
 * @param dev The device to initialize
 * 
 * @returns Negative value on failure and 0 on success
 */
USB_STATUS usb_initializeDevice(USBDevice_t *dev);

/**
 * @brief Deinitialize a USB device
 * @param dev The device to deinitialize
 * 
 * @note This WILL NOT free the memory of the device. Call @c usb_destroyDevice after this.
 */
USB_STATUS usb_deinitializeDevice(USBDevice_t *dev);

/**
 * @brief Create a new USB device structure for initialization
 * @param controller The controller
 * @param port The device port
 * @param speed The device speed
 * 
 * @param shutdown The HC device shutdown method
 * @param control The HC control request method
 * @param interrupt The HC interrupt request method
 */
USBDevice_t *usb_createDevice(USBController_t *controller, uint32_t port, int speed, hc_shutdown_t shutdown, hc_control_t control, hc_interrupt_t interrupt);

/**
 * @brief Destroy a USB device
 * @param controller The controller
 * @param dev The device to destroy
 * 
 * @warning Does not shut the device down, just frees it from memory. Call @c usb_deinitializeDevice first
 */
void usb_destroyDevice(USBController_t *controller, USBDevice_t *dev);

#endif