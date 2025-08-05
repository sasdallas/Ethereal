/**
 * @file hexahedron/include/kernel/drivers/usb/dev.h
 * @brief USB device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_DEV_H
#define DRIVERS_USB_DEV_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/drivers/usb/desc.h>
#include <kernel/drivers/usb/req.h>
#include <structs/list.h>
#include <kernel/fs/kernelfs.h>

/**** DEFINITIONS ****/

// Speeds
#define USB_FULL_SPEED      0x00    // Full speed
#define USB_LOW_SPEED       0x01    // Low speed
#define USB_HIGH_SPEED      0x02    // High speed (fastest)

// Transfer statuses
#define USB_TRANSFER_IN_PROGRESS    0
#define USB_TRANSFER_FAILED         1
#define USB_TRANSFER_SUCCESS        2

// Maximum address
#define USB_MAX_ADDRESS     127     // Each controller can have at most 127 devices

/**** TYPES ****/

// Prototypes
struct USBDevice;
struct USBInterface;

// Can't include these headers because they include us
struct USBController;
struct USBDriver;


/**
 * @brief USB endpoint
 */
typedef struct USBEndpoint {
    struct USBInterface *intf;      // Parent interface
    USBEndpointDescriptor_t desc;   // Descriptor
    uint32_t toggle;                // For bulk data transfers
} USBEndpoint_t;

/**
 * @brief USB interface
 */
typedef struct USBInterface {
    struct USBDevice *dev;          // Parent device

    USBInterfaceDescriptor_t desc;  // Descriptor
    list_t *endpoint_list;          // List of endpoints (USBEndpoint_t)

    struct USBDriver *driver;       // Driver currently registered to this interface
} USBInterface_t;

/**
 * @brief USB configuration
 */
typedef struct USBConfiguration {
    int index;                          // Index of the endpoint
    USBConfigurationDescriptor_t desc;  // Descriptor
    list_t *interface_list;             // List of interfaces
} USBConfiguration_t;

/**
 * @brief USB transfer
 * 
 * This is a basic transfer usually passed to the host controller's request (along with the device).
 */
typedef struct USBTransfer {
    uint32_t endpoint;              // Endpoint number
    USBDeviceRequest_t *req;        // Device request  
    void *data;                     // Data
    uint32_t length;                // Length of the data
    int status;                     // Transfer status (USB_TRANSFER_...)
    USBEndpoint_t *endp;            // Endpoint structure
} USBTransfer_t;


/**
 * @brief Host controller transfer method for a CONTROL transfer
 * @param controller The controller
 * @param dev The device
 * @param transfer The transfer
 * @returns USB_TRANSFER status code
 */
typedef int (*hc_control_t)(struct USBController *controller, struct USBDevice *dev, USBTransfer_t *transfer);

/**
 * @brief Host controller transfer method for an INTERRUPT transfer
 * @param controller The controller
 * @param dev The device
 * @param transfer The transfer
 * @returns USB_TRANSFER status code
 */
typedef int (*hc_interrupt_t)(struct USBController *controller, struct USBDevice *dev, USBTransfer_t *transfer);

/**
 * @brief Special host controller method to address the device 
 * @param controller The controller
 * @param dev The device
 * @returns USB status code
 */
typedef int (*hc_address_t)(struct USBController *controller, struct USBDevice *dev);

/**
 * @brief Configure a specific endpoint
 * @param controller The controller
 * @param dev The device
 * @param endp The endpoint
 * @returns USB status code
 */
typedef int (*hc_endpoint_t)(struct USBController *controller, struct USBDevice *dev, struct USBEndpoint *endp);

/**
 * @brief Evaluate the device context
 * @param controller The controller
 * @param dev The device
 * @returns USB status code
 */
typedef int (*hc_evaluate_t)(struct USBController *controller, struct USBDevice *dev);

/**
 * @brief Shutdown and free the memory associated with a device
 * @param controller The controller
 * @param dev The device
 * @returns USB status code
 */
typedef int (*hc_shutdown_t)(struct USBController *controller, struct USBDevice *dev);

/**
 * @brief Main USB device structure
 * 
 * @note This uses a TON of memory, as it holds config/endpoint/interface lists. When you want to deinitialize, CALL @c usb_destroyDevice TO CLEAN ALL MEMORY!
 */
typedef struct USBDevice {
    struct USBController *c;                // Controller

    // Device info
    uint32_t    port;                       // Port of the device
    int         speed;                      // Speed of the device
    uint32_t    address;                    // Address assigned to the device
    uint32_t    mps;                        // Max packet size as determined by device descriptor

    // Configuration/endpoint/interface
    USBConfiguration_t *config;             // Current configuration selected

    list_t *config_list;                    // List of endpoints

    // Other descriptors
    USBDeviceDescriptor_t device_desc;      // Device descriptor
    USBStringLanguagesDescriptor_t *langs;  // Languages (this is a pointer as we need to realloc after reading bLength)
    
    // HACK: Probably have to remove this
    uint16_t chosen_language;               // Chosen language for Hexahedron to use by default

    // Host controller methods
    hc_shutdown_t       shutdown;           // Shutdown the device
    hc_address_t        setaddr;            // Address the device (OPTIONAL)
    hc_endpoint_t       confendp;           // Configure endpoint (OPTIONAL)
    hc_evaluate_t       evaluate;           // Evaluate context (OPTIONAL)
    hc_control_t        control;            // Control transfer request
    hc_interrupt_t      interrupt;          // Interrupt transfer request

    // Other
    uint32_t dev_id;                        // Hexahedron-chosen device ID (don't worry, this doesn't affect you)
    kernelfs_dir_t *dir;                    // KernelFS directory
    void *dev;                              // Controller-defined device structure
} USBDevice_t;

/**** FUNCTIONS ****/

/**
 * @brief USB device request method
 * 
 * @param device The device
 * @param type The request type (should have a direction, type, and recipient - bmRequestType)
 * @param request The request to send (USB_REQ_... - bRequest)
 * @param value Optional parameter to the request (wValue)
 * @param index Optional index for the request (this field differs depending on the recipient - wIndex)
 * @param length The length of the data (wLength)
 * @param data The data
 * 
 * @returns The transfer status (not USB_STATUS)
 */
int usb_requestDevice(USBDevice_t *device, uintptr_t type, uintptr_t request, uintptr_t value, uintptr_t index, uintptr_t length, void *data);



#endif