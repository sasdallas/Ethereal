/**
 * @file hexahedron/include/kernel/drivers/usb2/protocol.h
 * @brief USB protocol-specific definitions and macros
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_USB_PROTOCOL_H
#define DRIVERS_USB_PROTOCOL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/util.h>

/**** DEFINITIONS ****/

/* Endpoint types */
#define USB_ENDP_TYPE_CONTROL       0x00
#define USB_ENDP_TYPE_ISOCH         0x01
#define USB_ENDP_TYPE_BULK          0x02
#define USB_ENDP_TYPE_INT           0x03

/* Directions */
#define USB_ENDP_DIR_IN             0x80
#define USB_ENDP_DIR_OUT            0x00

/* Common descriptor types */
#define USB_DESC_DEVICE             0x01
#define USB_DESC_CONF               0x02
#define USB_DESC_STRING             0x03
#define USB_DESC_INTF               0x04
#define USB_DESC_ENDP               0x05
#define USB_DESC_BOS                0x0F
#define USB_DESC_HUB                0x29
#define USB_DESC_HUB_SUPERSPEED     0x2A

/* Requests */
#define USB_REQ_GET_STATUS                  0x00
#define USB_REQ_CLEAR_FEATURE               0x01
#define USB_REQ_SET_FEATURE                 0x03
#define USB_REQ_SET_ADDR                    0x05
#define USB_REQ_GET_DESC                    0x06
#define USB_REQ_SET_DESC                    0x07
#define USB_REQ_GET_CONF                    0x08
#define USB_REQ_SET_CONF                    0x09
#define USB_REQ_GET_INTF                    0x0A
#define USB_REQ_SET_INTF                    0x0B
#define USB_REQ_SYNC_FRAME                  0x0C

/* Directions */
#define USB_RT_D2H                      0x80
#define USB_RT_H2D                      0x00

/* Destinations */
#define USB_RT_DEV                      0x00
#define USB_RT_INTF                     0x01
#define USB_RT_ENDP                     0x02
#define USB_RT_OTHER                    0x03

/* Types */
#define USB_RT_STANDARD                 0x00
#define USB_RT_CLASS                    0x20
#define USB_RT_VENDOR                   0x40

/* Classes */
#define USB_CLASS_IN_INTERFACE          0x00
#define USB_CLASS_HUB                   0x09

/* Common subclasses */
#define USB_SUBCLASS_HUB                0x00

/* Hub protocol */
#define USB_PROTOCOL_HUB_FULLSPD        0x00
#define USB_PROTOCOL_HUB_HIGHSPD_STT    0x01
#define USB_PROTOCOL_HUB_HIGHSPD_MTT    0x02
#define USB_PROTOCOL_HUB_SUPERSPD       0x03

/* Hub-specific */
#define USB_HUB_STATUS_CONNECTION           (1 << 0)
#define USB_HUB_STATUS_ENABLED              (1 << 1)
#define USB_HUB_STATUS_SUSPENDED            (1 << 2)
#define USB_HUB_STATUS_OVER_CURRENT         (1 << 3)
#define USB_HUB_STATUS_RESET                (1 << 4)

/* 2.0 */
#define USB_HUB_STATUS_POWER                (1 << 8)
#define USB_HUB_STATUS_LOW_SPEED            (1 << 9)
#define USB_HUB_STATUS_HIGH_SPEED           (1 << 10)
#define USB_HUB_STATUS_TEST                 (1 << 11)

/* 3.0 */
#define USB_HUB_SS_STATUS_POWER             (1 << 9)
#define USB_HUB_SS_LINK_STATE(status)       (((status) & 0x1E0) >> 5)
#define USB_HUB_SS_SPEED(status)            (((status) & 0x1C00) >> 10)
#define USB_HUB_SS_SPEED_SHIFT              10
#define USB_HUB_SS_SPEED_FULL               1
#define USB_HUB_SS_SPEED_LOW                2
#define USB_HUB_SS_SPEED_HIGH               3
#define USB_HUB_SS_SPEED_SUPER              4

#define USB_HUB_SS_LINK_STATE_U0            0x0
#define USB_HUB_SS_LINK_STATE_U1            0x1
#define USB_HUB_SS_LINK_STATE_U2            0x2
#define USB_HUB_SS_LINK_STATE_U3            0x3
#define USB_HUB_SS_LINK_STATE_RXDETECT      0x5
#define USB_HUB_SS_LINK_STATE_INACTIVE      0x6
#define USB_HUB_SS_LINK_STATE_POLLING       0x7
#define USB_HUB_SS_LINK_STATE_RECOVERY      0x8

/* Change bits */
#define USB_HUB_CHANGE_CONNECTION           (1 << 0)
#define USB_HUB_CHANGE_ENABLE               (1 << 1)
#define USB_HUB_CHANGE_SUSPEND              (1 << 2)
#define USB_HUB_CHANGE_OVERCURRENT          (1 << 3)
#define USB_HUB_CHANGE_RESET                (1 << 4)
#define USB_HUB_SS_CHANGE_BH_PORT_RESET     (1 << 5)
#define USB_HUB_SS_CHANGE_PORT_LINK_STATE   (1 << 6)
#define USB_HUB_SS_CHANGE_PORT_CONFIG_ERROR (1 << 7)

/* Selector */
#define USB_HUB_SEL_PORT_CONNECTION         0
#define USB_HUB_SEL_PORT_ENABLE             1
#define USB_HUB_SEL_PORT_SUSPEND            2
#define USB_HUB_SEL_PORT_OVER_CURRENT       3
#define USB_HUB_SEL_PORT_RESET              4
#define USB_HUB_SEL_PORT_LINK_STATE         5
#define USB_HUB_SEL_PORT_POWER              8
#define USB_HUB_SEL_PORT_LOW_SPEED          9
#define USB_HUB_SEL_C_PORT_CONNECTION       16
#define USB_HUB_SEL_C_PORT_ENABLE           17
#define USB_HUB_SEL_C_PORT_SUSPEND          18
#define USB_HUB_SEL_C_PORT_OVER_CURRENT     19
#define USB_HUB_SEL_C_PORT_RESET            20
#define USB_HUB_SEL_C_PORT_LINK_STATE       25
#define USB_HUB_SEL_C_PORT_CONFIG_ERROR     26
#define USB_HUB_SEL_BH_PORT_RESET           28
#define USB_HUB_SEL_C_BH_PORT_RESET         29

/* Hub-specific requests */
#define USB_HUB_REQ_SET_HUB_DEPTH           0xC
#define USB_HUB_REQ_GET_PORT_ERR_COUNT      0xD

#define USB_MAX_STRING_LENGTH           126

/**** TYPES ****/

typedef struct usb_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
} __attribute__((packed)) usb_descriptor_t;

STATIC_ASSERT(sizeof(usb_descriptor_t) == 2);

typedef struct usb_device_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) usb_device_desc_t;

STATIC_ASSERT(sizeof(usb_device_desc_t) == 18);

typedef struct usb_interface_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_desc_t;

STATIC_ASSERT(sizeof(usb_interface_desc_t) == 9);

typedef struct usb_config_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed)) usb_config_desc_t;

STATIC_ASSERT(sizeof(usb_config_desc_t) == 9);

typedef struct usb_endpoint_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) usb_endpoint_desc_t;

STATIC_ASSERT(sizeof(usb_endpoint_desc_t) == 7);

typedef struct usb_string_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bString[126];
} __attribute__((packed)) usb_string_desc_t;

STATIC_ASSERT(sizeof(usb_string_desc_t) == 254);

typedef struct usb_string_lang_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wLangId[];
} __attribute__((packed)) usb_string_lang_desc_t;

STATIC_ASSERT(sizeof(usb_string_lang_desc_t) == 2);

typedef struct usb_hub_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t bPowerOnGood;
    uint8_t bHubContrCurrent;
} __attribute__((packed)) usb_hub_desc_t;

STATIC_ASSERT(sizeof(usb_hub_desc_t) == 7);

typedef struct usb_hub_desc_ss {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t bPwrOn2PwrGood;
    uint8_t bHubContrCurrent;
    uint8_t bHubHdrDecLat;
    uint16_t wHubDelay;
} __attribute__((packed)) usb_hub_desc_ss_t;

STATIC_ASSERT(sizeof(usb_hub_desc_ss_t) == 10);

typedef struct usb_hub_port_status {
    uint16_t wPortStatus;
    uint16_t wPortChanged;
} __attribute__((packed)) usb_hub_port_status_t;

STATIC_ASSERT(sizeof(usb_hub_port_status_t) == 4);

typedef struct usb_device_request {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_device_request_t;

STATIC_ASSERT(sizeof(usb_device_request_t) == 8);

typedef struct usb_hid_optional_desc {
    uint8_t bDescriptorType;
    uint16_t wItemLength;
} __attribute__((packed)) usb_hid_optional_desc_t;

STATIC_ASSERT(sizeof(usb_hid_optional_desc_t) == 3);

typedef struct usb_hid_desc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHid;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;

    // following this are the optional descriptors
    usb_hid_optional_desc_t desc[];
} __attribute__((packed)) usb_hid_desc_t;

STATIC_ASSERT(sizeof(usb_hid_desc_t) == 6);

typedef enum usb_speed {
    USB_SPEED_UNKNOWN,
    USB_SPEED_LOW,
    USB_SPEED_FULL,
    USB_SPEED_HIGH,
    USB_SPEED_SUPER,
    USB_SPEED_SUPER_PLUS
} usb_speed_t;

/**** MACROS ****/

#define USB_ENDP_TYPE(bmAttributes) ((bmAttributes) & 0x3)
#define USB_ENDP_DIRECTION(bEndpointAddress) ((bEndpointAddress) & 0x80)
#define USB_ENDP_NUMBER(bEndpointAddress) (((bEndpointAddress) & 0x0F))

// This is for helpers since the USB_ENDP_TYPE macros are really heavy
#define USB_ENDP_IS_CONTROL(endp) (USB_ENDP_TYPE((endp)->desc.bmAttributes) == USB_ENDP_TYPE_CONTROL)
#define USB_ENDP_IS_INT(endp) (USB_ENDP_TYPE((endp)->desc.bmAttributes) == USB_ENDP_TYPE_INT)
#define USB_ENDP_IS_BULK(endp) (USB_ENDP_TYPE((endp)->desc.bmAttributes) == USB_ENDP_TYPE_BULK)
#define USB_ENDP_IS_ISOCH(endp) (USB_ENDP_TYPE((endp)->desc.bmAttributes) == USB_ENDP_TYPE_ISOCH)

#define USB_DESC_NEXT(desc) (usb_descriptor_t*)((uint8_t*)(desc) + (desc)->bLength)

#endif
