/**
 * @file drivers/usb/xhci_new/xhci_roothub.c
 * @brief xHCI root hub manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "xhci.h"
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI:ROOTHUB", __VA_ARGS__)

static usb_status_t xhci_roothub_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_roothub_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_roothub_submit(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_roothub_start(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_roothub_abort(usb_pipe_t *pipe, usb_transfer_t *transfer);
usb_pipe_ops_t xhci_roothub_intr_ops = {
    .init_transfer = xhci_roothub_init_transfer,
    .free_transfer = xhci_roothub_free_transfer,
    .submit = xhci_roothub_submit,
    .start = xhci_roothub_start,
    .abort = xhci_roothub_abort
};

usb_status_t xhci_root_hub_control(usb_bus_t *ubus, usb_transfer_t *transfer) {
    xhci_bus_t *bus = ubus->priv;
    xhci_t *xhci = bus->xhci;
    usb_device_request_t *req = &transfer->req;
    if (req->bRequest == USB_REQ_GET_DESC) {
        uint8_t desc_type = (req->wValue >> 8) & 0xFF;
        uint8_t desc_index = (req->wValue & 0xFF);

        if (desc_type == USB_DESC_HUB && desc_index == 0) {
            // !!! THIS IS NOT A VALID HUB DESCRIPTOR. IT DOES NOT HAVE THE VARIABLE FIELDS AT THE END.
            usb_hub_desc_t d = {
                .bDescriptorType = USB_DESC_HUB,
                .bLength = sizeof(usb_hub_desc_t),
                .bPowerOnGood = 1,
                .wHubCharacteristics = 0,
                .bNbrPorts = bus->port_count,
                .bHubContrCurrent = 0,
            };

            size_t to_copy = min(sizeof(d), transfer->length);
            memcpy(transfer->buffer, &d, to_copy);

            transfer->status = USB_SUCCESS;
            transfer->actual_length = to_copy;
            return USB_SUCCESS;
        } else if (desc_type == USB_DESC_HUB_SUPERSPEED && desc_index == 0) {
            // !!! THIS IS NOT A VALID HUB DESCRIPTOR. IT DOES NOT HAVE THE VARIABLE FIELDS AT THE END.
            usb_hub_desc_ss_t d = {
                .bDescriptorType = USB_DESC_HUB_SUPERSPEED,
                .bLength = sizeof(usb_hub_desc_ss_t),
                .bNbrPorts = bus->port_count,
                .wHubCharacteristics = 0,
                .bHubContrCurrent = 0,
                .bHubHdrDecLat = 0,
                .bPwrOn2PwrGood = 1,
                .wHubDelay = 1
            };

            size_t to_copy = min(sizeof(d), transfer->length);
            memcpy(transfer->buffer, &d, to_copy);

            transfer->status = USB_SUCCESS;
            transfer->actual_length = to_copy;
            return USB_SUCCESS;
        } else {
            LOG(ERR, "xHCI cannot handle unknown USB_REQ_GET_DESC for descriptor %02x on index %02x\n", desc_type, desc_index);
            return USB_INVALID;
        }
    } else if (req->bRequest == USB_REQ_GET_STATUS) {
        uint8_t port = req->wIndex & 0xFF;
        assert(port > 0 && "get hub status not impld");
        if (port > bus->port_count+1) {
            LOG(ERR, "Invalid port %d specified in USB_REQ_CLEAR_FEATURE\n", req->wIndex);
            return USB_INVALID;
        }

        int physical_index = (port + bus->port_base) - 1;
        volatile xhci_port_regs_t *regs = XHCI_PORTREGS(xhci, physical_index-1);

        uint32_t portsc = regs->portsc;
        uint32_t status = 0;
        uint32_t change = 0;

        if (portsc & XHCI_PORTSC_CCS) status |= USB_HUB_STATUS_CONNECTION;
        if (portsc & XHCI_PORTSC_PED) status |= USB_HUB_STATUS_ENABLED;
        if (portsc & XHCI_PORTSC_OCA) status |= USB_HUB_STATUS_OVER_CURRENT;
        if (portsc & XHCI_PORTSC_PR) status |= USB_HUB_STATUS_RESET;

        // TODO: USB_HUB_CHANGE_SUSPEND
        if (portsc & XHCI_PORTSC_CSC) change |= USB_HUB_CHANGE_CONNECTION;
        if (portsc & XHCI_PORTSC_PEC) change |= USB_HUB_CHANGE_ENABLE;
        if (portsc & XHCI_PORTSC_OCC) change |= USB_HUB_CHANGE_OVERCURRENT;
        if (portsc & XHCI_PORTSC_PRC) change |= USB_HUB_CHANGE_RESET;

        uint32_t speed = (portsc & 0x3C00) >> 10;
        if (ubus->revision == USB_REVISION_2_0) {
            // Speed is only valid when CCS is set
            if (portsc & XHCI_PORTSC_CCS) {
                if (speed == 1) {
                    // Full speed device (neither set)
                } else if (speed == 2) {
                    status |= USB_HUB_STATUS_LOW_SPEED;
                } else if (speed == 3) {
                    status |= USB_HUB_STATUS_HIGH_SPEED;
                } else {
                    assert(0 && "invalid speed for USB 2.0 bus");
                }
            }

            if (portsc & XHCI_PORTSC_PP) {
                status |= USB_HUB_STATUS_POWER;
            }
        } else {
            // This is a USB 3.0/3.1 hub
            // Speed is only valid when connected
            if (portsc & XHCI_PORTSC_CCS) {
                if (speed == 1) {
                    status |= (USB_HUB_SS_SPEED_FULL << USB_HUB_SS_SPEED_SHIFT);                
                } else if (speed == 2) {
                    status |= (USB_HUB_SS_SPEED_LOW << USB_HUB_SS_SPEED_SHIFT);
                } else if (speed == 3) {
                    status |= (USB_HUB_SS_SPEED_HIGH << USB_HUB_SS_SPEED_SHIFT);
                } else if (speed == 4 || speed == 5) {
                    status |= (USB_HUB_SS_SPEED_SUPER << USB_HUB_SS_SPEED_SHIFT);
                } else {
                    assert(0 && "invalid speed for USB 3.x bus");
                }
            }

            // The hub specification thankfully allows us to just copy the xHCI PLS to the status bits
            status |= (portsc & 0x1E0);

            if (portsc & XHCI_PORTSC_PP) {
                status |= USB_HUB_SS_STATUS_POWER;
            }

            if (portsc & XHCI_PORTSC_WRC) change |= USB_HUB_SS_CHANGE_BH_PORT_RESET;
            if (portsc & XHCI_PORTSC_PLC) change |= USB_HUB_SS_CHANGE_PORT_LINK_STATE;
            if (portsc & XHCI_PORTSC_CEC) change |= USB_HUB_SS_CHANGE_PORT_CONFIG_ERROR;
        }

        usb_hub_port_status_t port_status = {
            .wPortStatus = status,
            .wPortChanged = change
        };

        size_t actlen = min(sizeof(port_status), transfer->length);
        memcpy(transfer->buffer, &port_status, actlen);
        transfer->actual_length = actlen;
        transfer->status = USB_SUCCESS;
        return USB_SUCCESS;
    } else if (req->bRequest == USB_REQ_SET_FEATURE) {
        uint8_t port = req->wIndex & 0xFF;
        assert(port > 0 && "get hub status not impld");
        if (port > bus->port_count+1) {
            LOG(ERR, "Invalid port %d specified in USB_REQ_CLEAR_FEATURE\n", req->wIndex);
            return USB_INVALID;
        }

        int physical_index = (port + bus->port_base) - 1;
        volatile xhci_port_regs_t *regs = XHCI_PORTREGS(xhci, physical_index-1);
    
        uint32_t set = (regs->portsc & XHCI_PORTSC_PRESERVE_BITS);
        switch (req->wValue) {
            case USB_HUB_SEL_PORT_POWER:
                set |= XHCI_PORTSC_PP;
                break;
            
            case USB_HUB_SEL_PORT_RESET:
                set |= XHCI_PORTSC_PR;
                break;

            case USB_HUB_SEL_BH_PORT_RESET:
                set |= XHCI_PORTSC_WPR;
                break;

            default:
                LOG(ERR, "Unknown SET_FEATURE: 0x%08x\n", set);
                return USB_INVALID;
        }

        regs->portsc = set;
        return USB_SUCCESS;
     } else if (req->bRequest == USB_REQ_CLEAR_FEATURE) {
        uint8_t port = req->wIndex & 0xFF;
        assert(port > 0 && "get hub status not impld");
        if (port > bus->port_count+1) {
            LOG(ERR, "Invalid port %d specified in USB_REQ_CLEAR_FEATURE\n", req->wIndex);
            return USB_INVALID;
        }

        int physical_index = (port + bus->port_base) - 1;
        volatile xhci_port_regs_t *regs = XHCI_PORTREGS(xhci, physical_index-1);
    
        uint32_t clear = (regs->portsc & XHCI_PORTSC_PRESERVE_BITS);

        switch (req->wValue) {
            case USB_HUB_SEL_C_PORT_CONNECTION:
                clear |= XHCI_PORTSC_CSC;
                break;            

            case USB_HUB_SEL_C_PORT_ENABLE:
                clear |= XHCI_PORTSC_PEC;
                break;

            case USB_HUB_SEL_C_PORT_OVER_CURRENT:
                clear |= XHCI_PORTSC_OCC;
                break;

            case USB_HUB_SEL_C_PORT_RESET:
                clear |= XHCI_PORTSC_PRC;
                break;

            case USB_HUB_SEL_PORT_POWER:
                clear &= ~(XHCI_PORTSC_PP);
                break;

            case USB_HUB_SEL_C_BH_PORT_RESET:
                clear |= XHCI_PORTSC_WRC;
                break;

            case USB_HUB_SEL_C_PORT_LINK_STATE:
                clear |= XHCI_PORTSC_PLC;
                break;

            case USB_HUB_SEL_C_PORT_CONFIG_ERROR:
                clear |= XHCI_PORTSC_CEC;
                break;

            default:
                LOG(ERR, "Unknown CLEAR_FEATURE: %08x\n", req->wValue);
                return USB_INVALID;
        }

        regs->portsc = clear;

        return USB_SUCCESS;
    }

    LOG(ERR, "xhci_root_hub_control is not implemented!\n");
    return USB_INVALID;
}


static usb_status_t xhci_roothub_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    return USB_SUCCESS;
}

static void xhci_roothub_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    // no-op
}

static usb_status_t xhci_roothub_submit(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    transfer->status = USB_IN_PROGRESS;
    return USB_SUCCESS;
}

static usb_status_t xhci_roothub_start(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    usb_bus_t *ubus = pipe->device->bus;
    xhci_bus_t *bus = ubus->priv;

    // Need the bus lock
    spinlock_acquire(&bus->lock);

    // Before the transfer is marked dead, if the port map is not zeroed it should be copied
    if (bitmap_find_first_set(bus->port_map, bus->port_count) != -1) {
        size_t to_copy = min(((bus->port_count+7)/8), transfer->length);
        memcpy(transfer->buffer, bus->port_map, to_copy);
        bitmap_fill(bus->port_map, 0, bus->port_count);
        spinlock_release(&bus->lock);

        transfer->actual_length = to_copy;
        transfer->status = USB_SUCCESS;
        usb_transferCompleteLocked(transfer);
        return USB_SUCCESS;
    }

    // No, mark it as pending
    transfer->status = USB_IN_PROGRESS;

    assert(bus->rhub_pending == NULL);
    bus->rhub_pending = transfer;
    spinlock_release(&bus->lock);

    return USB_SUCCESS;
}

static void xhci_roothub_abort(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    assert(0);
}
