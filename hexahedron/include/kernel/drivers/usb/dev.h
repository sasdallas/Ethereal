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
#include <kernel/drivers/usb/status.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

// Speeds
#define USB_FULL_SPEED      0x00    // Full speed
#define USB_LOW_SPEED       0x01    // Low speed
#define USB_HIGH_SPEED      0x02    // High speed
#define USB_SUPER_SPEED     0x03    // Super speed

// TODO not use this
typedef enum {
    USB_TRANSFER_IN_PROGRESS,
    USB_TRANSFER_SUCCESS,
    USB_TRANSFER_FAILED
} USB_TRANSFER_STATUS;

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
    list_t *additional_desc_list;   // Additional, unrecognized descriptors 

    struct USBDriver *driver;       // Driver currently registered to this interface
    void *d;                        // Driver-specific
} USBInterface_t;

/**
 * @brief USB configuration
 */
typedef struct USBConfiguration {
    int index;                          // Index of the endpoint
    USBConfigurationDescriptor_t desc;  // Descriptor
    list_t *interface_list;             // List of interfaces
} USBConfiguration_t;

struct USBTransfer;
struct USBTransferCompletion;

/**
 * @brief Interrupt transfer callback
 * @param endp The endpoint that received the transfer
 * @param completion The completed transfer that was done
 */
typedef void (*usb_int_callback_t)(USBEndpoint_t *endp, struct USBTransferCompletion *completion);

/**
 * @brief USB transfer
 * 
 * This is a basic transfer usually passed to the host controller's request (along with the device).
 */
typedef struct USBTransfer {
    uint32_t endpoint;              // Endpoint number
    USBDeviceRequest_t req;         // Device request  
    void *data;                     // Buffer data
    uint32_t length;                // Length of the data
    USB_TRANSFER_STATUS status;     // Transfer status (USB_TRANSFER_...)
    USBEndpoint_t *endp;            // Endpoint structure
    usb_int_callback_t callback;    // (Optional) Transfer callback for interrupt transfers
    void *parameter;                // Specific transfer parameter, this is for your reference
} USBTransfer_t;

/**
 * @brief USB transfer completion object
 */
typedef struct USBTransferCompletion {
    USBTransfer_t *transfer;        // Original transfer
    size_t length;                  // The amount of data that was sent
} USBTransferCompletion_t;

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

typedef struct USBDeviceOps {
    USB_TRANSFER_STATUS (*control)(struct USBController *, struct USBDevice *, USBTransfer_t *);
    USB_TRANSFER_STATUS (*interrupt)(struct USBController *, struct USBDevice *, USBTransfer_t *);
    USB_TRANSFER_STATUS (*bulk)(struct USBController *, struct USBDevice *, USBTransfer_t *);
    USB_STATUS (*endpoint)(struct USBController *, struct USBDevice *, struct USBEndpoint *);
    USB_STATUS (*evaluate)(struct USBController *, struct USBDevice *); // TODO: replace with address op
    USB_STATUS (*shutdown)(struct USBController *, struct USBDevice *);
} USBDeviceOps_t;

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
    USBDeviceOps_t *ops;

    // Other
    void *dev;                              // Controller-defined device structure
} USBDevice_t;

/**** FUNCTIONS ****/

/**
 * @brief USB device control request method
 * @param device The device to request
 * @param req The request
 * @param buf The buffer to use
 * @returns Transfer status
 */
USB_TRANSFER_STATUS usb_controlRequest(USBDevice_t *device, USBDeviceRequest_t *req, void *buffer);

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
USB_TRANSFER_STATUS usb_requestDevice(USBDevice_t *device, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t length, void *data);

#endif
