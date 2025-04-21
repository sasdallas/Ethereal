/**
 * @file drivers/usbhub/hub.h
 * @brief USB Hub driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef HUB_H
#define HUB_H

/**** INCLUDES ****/
#include <kernel/drivers/usb/usb.h>
#include <structs/list.h>

/**** TYPES ****/

typedef struct USBHub {
    USBInterface_t *intf;       // Interface
    uint32_t nports;            // Number of connected ports
    list_t *hub_ports;          // List of hub ports
    USBHubDescriptor_t desc;    // Descriptor
} USBHub_t;

/**** DEFINITIONS ****/

#define HUB_CLASS_CODE          0x09

// USB hub requests
#define HUB_REQ_GET_STATUS          0x00
#define HUB_REQ_CLEAR_FEATURE       0x01
#define HUB_REQ_SET_FEATURE         0x03
#define HUB_REQ_CLEAR_TT_BUFFER     0x08
#define HUB_REQ_RESET_TT            0x09
#define HUB_REQ_GET_TT_STATE        0x0A
#define HUB_REQ_CSTOP_TT            0x0B

// USB port feature fields
#define HUB_FEATURE_PORT_CONNECTION     0
#define HUB_FEATURE_PORT_ENABLE         1
#define HUB_FEATURE_PORT_SUSPEND        2
#define HUB_FEATURE_PORT_OVER_CURRENT   3
#define HUB_FEATURE_PORT_RESET          4
#define HUB_FEATURE_PORT_POWER          8
#define HUB_FEATURE_PORT_LOW_SPEED      9

// USB port status fields (in wPortStatus)
#define HUB_PORT_STATUS_CONNECTION         0x01
#define HUB_PORT_STATUS_ENABLE             0x02
#define HUB_PORT_STATUS_SUSPEND            0x04
#define HUB_PORT_STATUS_OVER_CURRENT       0x08
#define HUB_PORT_STATUS_RESET              0x10
#define HUB_PORT_STATUS_POWER              0x100
#define HUB_PORT_STATUS_LOW_SPEED          0x200
#define HUB_PORT_STATUS_HIGH_SPEED         0x400
#define HUB_PORT_STATUS_TEST               0x800
#define HUB_PORT_STATUS_INDICATOR          0x1000


#endif