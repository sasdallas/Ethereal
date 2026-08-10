/**
 * @file drivers/usb/hub/hub.h
 * @brief USB hub definitions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _HUB_H
#define _HUB_H

/**** INCLUDES ****/
#include <kernel/drivers/usb2/usb.h>
#include <stdint.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef struct hub_port_status {
    uint16_t wPortStatus;
    uint16_t wPortChanged;
} __attribute__((packed)) hub_port_status_t;

typedef struct hub_internal {
    usb_interface_t *intf;
    usb_pipe_t *int_pipe;
    usb_hub_t *hub;
    unsigned long *bitmap;
    size_t num_ports;
    usb_transfer_t *intr_transfer;
    spinlock_t lock;
} hub_internal_t;

#endif
