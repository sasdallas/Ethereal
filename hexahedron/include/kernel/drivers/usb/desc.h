/**
 * @file hexahedron/include/kernel/drivers/usb/desc.h
 * @brief USB descriptors
 * 
 * @see https://wiki.osdev.org/Universal_Serial_Bus#Standard_USB_Descriptors
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_USB_DESC_H
#define DRIVERS_USB_DESC_H

/**** INCLUDES ****/

#include <stdint.h>

/**** DEFINITIONS ****/

// Descriptor types
#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONF           0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTF           0x04
#define USB_DESC_ENDP           0x05

// HID types
#define USB_DESC_HID            0x21
#define USB_DESC_REPORT         0x22
#define USB_DESC_PHYSICAL       0x23

// Hub types
#define USB_DESC_HUB            0x29

// TODO: Find a better way to define 0 bits

// Endpoint bitmasks
#define USB_ENDP_NUMBER         0x0F
#define USB_ENDP_USAGE          0x30

// Endpoint usage types
#define USB_ENDP_DATA               0x00        // Data endpoint
#define USB_ENDP_FEEDBACK           0x10        // Feedback endpoint
#define USB_ENDP_FEEDBACK_IMPL      0x30        // Feedback implicit endpoint

// Endpoint transfer types - recommended to use USB_ENDP_IS_...
#define USB_ENDP_TRANSFER_CONTROL   0x00        // !!!: DO NOT USE AS THIS IS AN INVALID BITMASK
#define USB_ENDP_TRANSFER_ISOCH     0x01        // Isochronous endpoint
#define USB_ENDP_TRANSFER_BULK      0x02        // Bulk endpoint
#define USB_ENDP_TRANSFER_INT       0x03        // Interrupt endpoint

/* Endpoint directions - to be used with USB_ENDP_GET_DIRECTION() only! */
#define USB_ENDP_DIRECTION_OUT      0x00        // OUT endpoint
#define USB_ENDP_DIRECTION_IN       0x80        // IN endpoint

// TODO: Endpoint syncronization types

// USB configuration attributes
#define USB_CONF_REMOTE_WAKEUP  0x20
#define USB_CONF_SELF_POWERED   0x40

// String language IDs (bitmasks)
// IDs are presented as a short where bits 15-10 are the sublanguage ID and bits 9-0 are the language ID
// See https://github.com/brookebasile/USB-langids/blob/master/USB_LANGIDs.pdf for full list
#define USB_LANGID_ENGLISH      0xFF09  // English


// IDs for macros specifically

/**** TYPES ****/

/**
 * @brief USB device descriptor
 * 
 * Describes general information about a USB device.
 * 
 * @note    If a device is high-speed (and has different device information for each speed),
 *          it must have a @c USBDeviceQualifierDescriptor descriptor
 */
typedef struct USBDeviceDescriptor {
    uint8_t     bLength;            // Size of this descriptor in bytes
    uint8_t     bDescriptorType;    // USB_DESC_DEVICE
    uint16_t    bcdUSB;             // USB specification release number in BCD

    uint8_t     bDeviceClass;       // Class code
    uint8_t     bDeviceSubClass;    // Subclass code
    uint8_t     bDeviceProtocol;    // Protocol code
    uint8_t     bMaxPacketSize0;    // Maximum packet size for the endpoint 0 (8/16/32/64 only valid)
    
    uint16_t    idVendor;           // Vendor ID
    uint16_t    idProduct;          // Product ID
    uint16_t    bcdDevice;          // Device release number in BCD
    
    uint8_t     iManufacturer;      // Index of STRING descriptor describing manufacturer
    uint8_t     iProduct;           // Index of STRING descriptor describing product
    uint8_t     iSerialNumber;      // Index of STRING descriptor describing device's SN

    uint8_t     bNumConfigurations; // Number of possible configurations
} USBDeviceDescriptor_t;


/**
 * @brief Interface descriptor
 */
typedef struct USBInterfaceDescriptor {
    uint8_t     bLength;            // Size of this descriptor in bytes
    uint8_t     bDescriptorType;    // USB_DESC_INTERFACE

    uint8_t     bInterfaceNumber;   // Interface number (zero-based)
    uint8_t     bAlternateSetting;  // Selects the alternate settings described by this descriptor
    uint8_t     bNumEndpoints;      // Number of endpoints used by this interface
    uint8_t     bInterfaceClass;    // Class code
    uint8_t     bInterfaceSubClass; // Subclass code
    uint8_t     bInterfaceProtocol; // Protocol code

    uint8_t     iInterface;         // Index of STRING descriptor describing this interface
} USBInterfaceDescriptor_t;

/**
 * @brief Configuration descriptor
 * 
 * At the end of this resides wTotalLength - bLength interface/endpoint descriptors that you can read
 */
typedef struct USBConfigurationDescriptor {
    uint8_t     bLength;                // Size of this descriptor in bytes
    uint8_t     bDescriptorType;        // USB_DESC_CONF

    uint16_t    wTotalLength;           // Total combined length in bytes of all descriptors returned with the request for this descriptor
    uint8_t     bNumInterfaces;         // Number of interfaces supported by this configuration
    uint8_t     bConfigurationValue;    // Used as an argument in USB_REQ_SET_CONFIGURATION
    uint8_t     iConfiguration;         // Index of STRING descriptor describing this configuration
    uint8_t     bmAttributes;           // Bitmap of attributes (see USB_CONF_...)
    uint8_t     bMaxPower;              // Maximum power consumption in units of 2mA
} USBConfigurationDescriptor_t;

/**
 * @brief Endpoint descriptor
 */
typedef struct USBEndpointDescriptor {
    uint8_t     bLength;            // Size of this descriptor in bytes
    uint8_t     bDescriptorType;    // USB_DESC_ENDPOINT

    uint8_t     bEndpointAddress;   // Address of the endpoint on the USB device (bit 7 is direction, bits 0-3 are the endpoint number)
    uint8_t     bmAttributes;       // Endpoint attributes
    uint16_t    wMaxPacketSize;     // Maximum packet size
    uint8_t     bInterval;          // Interval for polling a device during a transfer
} USBEndpointDescriptor_t;

/**
 * @brief String descriptor (for language)
 */
typedef struct USBStringLanguagesDescriptor {
    uint8_t     bLength;            // Size of this descriptor in bytes
    uint8_t     bDescriptorType;    // USB_DESC_STRING

    uint16_t    wLangID[];          // Language IDs
} USBStringLanguagesDescriptor_t;

/**
 * @brief String descriptor
 */
typedef struct USBStringDescriptor {
    uint8_t     bLength;            // Size of this descriptor in bytes
    uint8_t     bDescriptorType;    // USB_DESC_STRING

    uint8_t     bString[];          // Unicode string
} USBStringDescriptor_t;

/**
 * @brief Hub descriptor
 */
typedef struct USBHubDescriptor {
    uint8_t     bLength;                // Size of this descriptor in bytes
    uint8_t     bDescriptorType;        // USB_DESC_HUB
    uint8_t     bNbrPorts;              // Number of ports
    uint16_t    wHubCharacteristics;    // Hub characteristics
    uint8_t     bPowerOnGood;           // Time (*2ms) from port power on to power good
    uint8_t     bHubContrCurrent;       // Maximum current used by hub controller (mA)
    
    // The remaining two fields of the hub descriptor are DeviceRemovable and PortPwrControlMask.
    // These take the size of bNbrPorts bits and are not included
} USBHubDescriptor_t;

/**** MACROS ****/

#define USB_ENDP_GET_NUMBER(endp) (((endp)->desc.bEndpointAddress & 0x0F) * 2 + !!((endp)->desc.bEndpointAddress & 0x80))

#define USB_ENDP_IS_INTERRUPT(endp) (((endp)->desc.bmAttributes & USB_ENDP_TRANSFER_INT) == USB_ENDP_TRANSFER_INT)
#define USB_ENDP_IS_BULK(endp) (!(USB_ENDP_IS_INTERRUPT(endp)) && ((endp)->desc.bmAttributes & USB_ENDP_TRANSFER_BULK))
#define USB_ENDP_IS_ISOCH(endp) (!(USB_ENDP_IS_INTERRUPT(endp)) && ((endp)->desc.bmAttributes & USB_ENDP_TRANSFER_ISOCH))
#define USB_ENDP_IS_CONTROL(endp) (!(USB_ENDP_IS_BULK(endp)) && !(USB_ENDP_IS_ISOCH(endp)))

#define USB_ENDP_GET_DIRECTION(endp) ((endp)->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN)

#endif