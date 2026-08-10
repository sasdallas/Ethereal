/**
 * @file drivers/usb/xhci_new/xhci_pipe.c
 * @brief Handles most of the pipes that xHCI supports
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 * 
 * Includes sources from banan-os (interval calculation):
 * https://github.com/Bananymous/banan-os/
 */

#include "xhci.h"
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Control endpoint */
static usb_status_t xhci_control_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_control_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_control_start(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_control_submit(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_control_abort(usb_pipe_t *pipe, usb_transfer_t *transfer);

usb_pipe_ops_t xhci_control_ep_ops = {
    .init_transfer = xhci_control_init_transfer,
    .free_transfer = xhci_control_free_transfer,
    .start = xhci_control_start,
    .submit = xhci_control_submit,
    .abort = xhci_control_abort
};

/* Interrupt endpoint */
static usb_status_t xhci_intr_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_intr_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_intr_start(usb_pipe_t *pipe, usb_transfer_t *transfer);
static usb_status_t xhci_intr_submit(usb_pipe_t *pipe, usb_transfer_t *transfer);
static void xhci_intr_abort(usb_pipe_t *pipe, usb_transfer_t *transfer);

usb_pipe_ops_t xhci_intr_ep_ops = {
    .init_transfer = xhci_intr_init_transfer,
    .free_transfer = xhci_intr_free_transfer,
    .start = xhci_intr_start,
    .submit = xhci_intr_submit,
    .abort = xhci_intr_abort
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI:PIPE", __VA_ARGS__)

// calculate endpoint interval
static void xhci_calculateInterval(xhci_endpoint_context_t *ec, usb_endpoint_t *endpoint, uint32_t speed) {
    // This code is taken from Banan-OS
    // Link: https://github.com/Bananymous/banan-os/blob/master/kernel/kernel/USB/XHCI/Device.cpp
    // Thank you to Bananymous for your countless contributions
#define ilog2(val) (sizeof(val) * 8 - __builtin_clz(val) - 1)
#define XHCI_CLAMP_INTERVAL(intv, low, high) ((intv < low) ? low : (intv > high) ? high : intv)
#pragma GCC diagnostic ignored "-Wtype-limits"

    switch (speed) {
        case XHCI_USB_SPEED_HIGH_SPEED:
            if (USB_ENDP_IS_BULK(endpoint) || USB_ENDP_IS_CONTROL(endpoint)) {
                ec->interval = (endpoint->desc.bInterval) ? XHCI_CLAMP_INTERVAL(ilog2(endpoint->desc.bInterval), 0, 15) : 0;
                break;
            }
            // fall through
        case XHCI_USB_SPEED_SUPER_SPEED:
            if (USB_ENDP_IS_ISOCH(endpoint) || USB_ENDP_IS_INT(endpoint)) {
                ec->interval = XHCI_CLAMP_INTERVAL(endpoint->desc.bInterval - 1, 0, 15);
                break;
            }

            ec->interval = 0;
            break;

        case XHCI_USB_SPEED_FULL_SPEED:
            if (USB_ENDP_IS_ISOCH(endpoint)) {
                ec->interval = XHCI_CLAMP_INTERVAL(endpoint->desc.bInterval + 2, 3, 18);
                break;
            }

            // fall through
        case XHCI_USB_SPEED_LOW_SPEED:
            if (USB_ENDP_IS_ISOCH(endpoint) || USB_ENDP_IS_INT(endpoint)) {
                ec->interval = (endpoint->desc.bInterval) ? XHCI_CLAMP_INTERVAL(ilog2(endpoint->desc.bInterval * 8), 3, 10) : 0;
                break;
            }

            ec->interval = 0;
            break;
    }
}

// mainly this is configure endpoint
usb_status_t xhci_configurePipe(xhci_t *xhci, usb_pipe_t *pipe) {
    usb_endpoint_t *endp = pipe->endp;
    xhci_device_t *dev = pipe->device->hc_priv;
    xhci_bus_t *bus = pipe->device->bus->priv;
    usb_port_t *p = pipe->device->port;

    // Create the xHCI pipe structure
    xhci_pipe_t *xpipe = kzalloc(sizeof(xhci_pipe_t));
    QUEUE_RB_INIT(&xpipe->transfers, 32);
    xpipe->xhci = xhci;
    xpipe->ring = xhci_createRing();
    pipe->hc_priv = xpipe;

    // Mark in parent context
    uint8_t endp_num = xhci_getDCI(pipe->endp);
    dev->pipes[endp_num-1] = xpipe;

    // Calculate the xHCI version of the endpoint type
    uint8_t ep_type = 0;
    if (USB_ENDP_TYPE(endp->desc.bmAttributes) == USB_ENDP_TYPE_INT) {
        if (USB_ENDP_DIRECTION(endp->desc.bEndpointAddress) == USB_ENDP_DIR_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_INT_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_INT_OUT;
        }
    } else if (USB_ENDP_TYPE(endp->desc.bmAttributes) == USB_ENDP_TYPE_BULK) {
        if (USB_ENDP_DIRECTION(endp->desc.bEndpointAddress) == USB_ENDP_DIR_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_BULK_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_BULK_OUT;
        }
    } else if (USB_ENDP_TYPE(endp->desc.bmAttributes) == USB_ENDP_TYPE_ISOCH) {
        if (USB_ENDP_DIRECTION(endp->desc.bEndpointAddress) == USB_ENDP_DIR_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_ISOCH_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_ISOCH_OUT;
        }
    } else {
        ep_type = XHCI_ENDPOINT_TYPE_CONTROL;
    }

    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    xhci_slot_context_t *sc = XHCI_SLOT_CONTEXT(dev);
    xhci_endpoint_context_t *ec = XHCI_ENDPOINT_CONTEXT(dev, endp_num);

    // Enable the endpoint in the input context
    memset(ic, 0, XHCI_CONTEXT_SIZE(xhci));
    ic->add_flags = (1 << endp_num) | (1 << 0);
    ic->drop_flags = 0;

    // Configure slot context
    if (dev->highest_ep < endp_num) {
        dev->highest_ep = endp_num;
    }

    // Prepare slot context
    sc->context_entries = dev->highest_ep;
    sc->max_exit_latency = 0;
    sc->route_string = dev->route_string;
    sc->interrupter_target = 0;
    sc->root_hub_port_num = dev->root_port;

    LOG(DEBUG, "Configuring endpoint %d (rhpn=%d rs=0x%08x)\n", endp_num, sc->root_hub_port_num, sc->route_string);

    // Calculate device speed
    uint32_t spd = 0;
    switch (pipe->device->speed) {
        case USB_SPEED_LOW: spd = XHCI_USB_SPEED_LOW_SPEED; break;
        case USB_SPEED_FULL: spd = XHCI_USB_SPEED_FULL_SPEED; break;
        case USB_SPEED_HIGH: spd = XHCI_USB_SPEED_HIGH_SPEED; break;
        case USB_SPEED_SUPER: spd = XHCI_USB_SPEED_SUPER_SPEED; break;
        case USB_SPEED_SUPER_PLUS: spd = XHCI_USB_SPEED_SUPER_SPEED_PLUS; break;
        default: assert(0);
    }
    
    sc->speed = spd;
    
    // Prepare endpoint context
    ec->endpoint_type = ep_type;
    ec->error_count = 3;
    ec->state = 0;
    ec->transfer_ring_dequeue_ptr = xpipe->ring->trb_phys | 1;

    // TODO: is this necessary? porting endpoint context stuff from the old driver
    if (USB_ENDP_IS_CONTROL(endp) || USB_ENDP_IS_BULK(endp)) {
        ec->max_packet_size = endp->desc.wMaxPacketSize;
    } else {
        ec->max_packet_size = endp->desc.wMaxPacketSize & 0x7FF;    
    }

    // configure max burst size
    uint8_t type = USB_ENDP_TYPE(endp->desc.bmAttributes);
    if (type == USB_ENDP_TYPE_CONTROL || type == USB_ENDP_TYPE_BULK) {
        ec->max_burst_size = 0;
    } else {
        ec->max_burst_size = (endp->mps & 0x1800) >> 11;
    }

    // calculate the ESIT payload
    uint32_t max_esit = ((ec->max_packet_size * (ec->max_burst_size + 1)));
    ec->max_esit_payload_lo = max_esit & 0xFFFF;
    ec->max_esit_payload_hi = max_esit >> 16;

    // control endpoints average a TRB length of 8 always
    if (type == USB_ENDP_TYPE_CONTROL) {
        ec->average_trb_length = 8;
    } else {
        ec->average_trb_length = max_esit;
    }

    // Calculate the interval
    xhci_calculateInterval(ec, endp, spd);
    if (pipe->endp == &pipe->device->control_ep) {
        // Done, now we have to issue address device.
        xhci_address_device_trb_t address_device = {
            .type = XHCI_CMD_ADDRESS_DEVICE,
            .bsr = 1,
            .slot_id = dev->slot_id,
            .input_ctx = arch_mmu_physical(NULL, (uintptr_t)dev->input_context),
            .c = 0,
            .rsvd0 = 0,
            .rsvd1 = 0,
            .rsvd2 = 0,
        };

        xhci_command_completion_trb_t trbout;
        xhci_sendCommand(xhci, &address_device, &trbout);
        if (!TRB_SUCCESS(&trbout)) {
            LOG(ERR, "ADDRESS_DEVICE with BSR=1 failed with completion code %d\n", trbout.cc);
            return USB_INTERNAL_ERROR;
        }

        address_device.bsr = 0;
        xhci_sendCommand(xhci, &address_device, &trbout);
        if (!TRB_SUCCESS(&trbout)) {
            LOG(ERR, "ADDRESS_DEVICE with BSR=0 failed with completion code %d\n", trbout.cc);
            return USB_INTERNAL_ERROR;
        }
    } else {
        xhci_configure_endpoint_trb_t trb = {
            .type = XHCI_CMD_CONFIGURE_ENDPOINT,
            .input_context = arch_mmu_physical(NULL, (uintptr_t)dev->input_context),
            .rsvd0 = 0,
            .rsvd1 = 0,
            .rsvd2 = 0,
            .slot_id = dev->slot_id,
            .dc = 0,
        };


        xhci_command_completion_trb_t trbout;
        xhci_sendCommand(xhci, &trb, &trbout);
        if (!TRB_SUCCESS(&trbout)) {
            LOG(ERR, "CONFIGURE_ENDPOINT failed with completion code %d\n", trbout.cc);
            return USB_INTERNAL_ERROR;
        }
    }

    // Device OK
    return USB_SUCCESS;
}

void xhci_freePipe(xhci_t *xhci, usb_pipe_t *pipe) {
    xhci_device_t *xdev = pipe->device->hc_priv;
    xhci_pipe_t *xpipe = pipe->hc_priv;

    LOG(DEBUG, "Closing endpoint pipe %d\n", xhci_getDCI(pipe->endp));

    // This pipe is already locked as its being freed meaning nothing more can come in
    while (queue_rb_empty(&xpipe->transfers) == false) {
        usb_transfer_t *transfer;

        if (queue_rb_pop(&xpipe->transfers, (void**)&transfer) != 0) {
            break;
        }

        transfer->status = USB_ABORTED;
        transfer->actual_length = 0;
        usb_transferCompleteLocked(transfer);
    }

    // Stop the endpoint
    // TODO: seems to be causing problems
    // xhci_stop_endpoint_trb_t stop_endpoint = {
    //     .type = XHCI_CMD_STOP_ENDPOINT,
    //     .endpoint = xhci_getDCI(pipe->endp),
    //     .slot_id = xdev->slot_id
    // };

    // xhci_command_completion_trb_t trbout;
    // xhci_sendCommand(xhci, &stop_endpoint, &trbout);

    // Free the TR
    xhci_freeRing(xpipe->ring);
    kfree(xpipe);
}

/* CONTROL */

static usb_status_t xhci_control_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    return transfer->length <= 0x1FFFF ? USB_SUCCESS : USB_INVALID;
}

static void xhci_control_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    // no-op
}

static usb_status_t xhci_control_start(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    xhci_pipe_t *xpipe = pipe->hc_priv;
    xhci_device_t *dev = pipe->device->hc_priv;
    xhci_t *xhci = dev->xhci;

    // ring must be locked so the full transfer sequence can be made
    XHCI_LOCK_RING(xpipe->ring);

    // HACK: this queue is protected by ring lock
    assert(queue_rb_space(&xpipe->transfers) && "maximum amount of transfers exceeded, kernel bug");
    queue_rb_push(&xpipe->transfers, transfer);

    // SETUP stage
    bool in = transfer->req.bmRequestType & USB_RT_D2H;
    xhci_setup_stage_trb_t setup = {
        .bmRequestType = transfer->req.bmRequestType,
        .bRequest = transfer->req.bRequest,
        .wValue = transfer->req.wValue,
        .wIndex = transfer->req.wIndex,
        .wLength = transfer->req.wLength,
        .transfer_length = 8,
        .idt = 1,
        .type = XHCI_TRB_TYPE_SETUP_STAGE,
        .trt = transfer->length ? (in ? 3 : 2) : 0,
    };

    xhci_enqueueRing(xpipe->ring, (xhci_trb_t*)&setup);

    // Create any DATA stage TRBs
    if (transfer->length) {
        xhci_data_stage_trb_t data = {
            .buffer = arch_mmu_physical(NULL, (uintptr_t)transfer->buffer),
            .transfer_length = transfer->length,
            .ch = 0,
            .type = XHCI_TRB_TYPE_DATA_STAGE,
            .dir = in,
        };

        xhci_enqueueRing(xpipe->ring, (xhci_trb_t*)&data);
    }

    // STATUS
    xhci_status_stage_trb_t status = {
        .ch = 0,
        .type = XHCI_TRB_TYPE_STATUS_STAGE,
        .dir = transfer->length ? !in : 1,
        .ioc = 1,
    };

    xhci_enqueueRing(xpipe->ring, (xhci_trb_t*)&status);

    // Notify the HC and unlock the ring for the next transfer
    XHCI_DOORBELL(xpipe->xhci, dev->slot_id) = 1;
    XHCI_UNLOCK_RING(xpipe->ring);

    return USB_SUCCESS;
}

static usb_status_t xhci_control_submit(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    transfer->status = USB_IN_PROGRESS;
    return USB_SUCCESS;
}

static void xhci_control_abort(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    assert(0);
}

/* INTERRUPT */

static usb_status_t xhci_intr_init_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    return USB_SUCCESS;
}

static void xhci_intr_free_transfer(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    // no-op
}

static usb_status_t xhci_intr_start(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    xhci_pipe_t *xpipe = pipe->hc_priv;
    xhci_device_t *dev = pipe->device->hc_priv;
    xhci_t *xhci = dev->xhci;

    XHCI_LOCK_RING(xpipe->ring);

    // HACK: this queue is protected by ring lock
    assert(queue_rb_space(&xpipe->transfers) && "maximum amount of transfers exceeded, kernel bug");
    queue_rb_push(&xpipe->transfers, transfer);

    // Send the normal TRB
    xhci_normal_trb_t trb = {
        .type = XHCI_TRB_TYPE_NORMAL,
        .buffer = arch_mmu_physical(NULL, (uintptr_t)transfer->buffer),
        .len = transfer->length,
        .ioc = 1,
        .c = 1,
        .ch = 0,
        .isp = 1
    };

    xhci_enqueueRing(xpipe->ring, (xhci_trb_t*)&trb);

    // Ring ring
    XHCI_DOORBELL(dev->xhci, dev->slot_id) = xhci_getDCI(pipe->endp);
    XHCI_UNLOCK_RING(xpipe->ring);

    return USB_SUCCESS;
}

static usb_status_t xhci_intr_submit(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    transfer->status = USB_IN_PROGRESS;
    return USB_SUCCESS;
}

static void xhci_intr_abort(usb_pipe_t *pipe, usb_transfer_t *transfer) {
    assert(0);
}
