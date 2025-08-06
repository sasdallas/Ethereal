/**
 * @file drivers/usb/xhci/device.c
 * @brief xHCI device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "xhci.h"
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <kernel/misc/mutex.h>
#include <string.h>
#include <kernel/mem/pmm.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:DEV ] " __VA_ARGS__);

/**
 * @brief Create a new transfer ring
 */
xhci_transfer_ring_t *xhci_createTransferRing() {
    xhci_transfer_ring_t *tr = kzalloc(sizeof(xhci_transfer_ring_t));
    tr->trb_list = (xhci_trb_t*)mem_allocateDMA(XHCI_TRANSFER_RING_TRB_COUNT * sizeof(xhci_trb_t));
    memset(tr->trb_list, 0, XHCI_TRANSFER_RING_TRB_COUNT * sizeof(xhci_trb_t));
    tr->trb_list_phys = mem_getPhysicalAddress(NULL, (uintptr_t)tr->trb_list);
    tr->cycle = 1;
    tr->dequeue = 0;
    tr->enqueue = 0;

    // Prepare link TRB
    xhci_link_trb_t *link_trb = (xhci_link_trb_t*)&tr->trb_list[XHCI_TRANSFER_RING_TRB_COUNT-1];
    link_trb->ring_segment = tr->trb_list_phys;
    link_trb->type = XHCI_TRB_TYPE_LINK;
    link_trb->tc = 1;
    link_trb->c = tr->cycle;
    

    return tr;
}

/**
 * @brief Enqueue a transfer TRB
 * @param tr The ring to enqueue the TRB into
 * @param trb The TRB to enqueue
 */
int xhci_enqueueTransferTRB(xhci_transfer_ring_t *tr, xhci_trb_t *trb) {
    trb->c = tr->cycle;
    memcpy(&tr->trb_list[tr->enqueue], trb, sizeof(xhci_trb_t));
    tr->enqueue++;
    
    if (tr->enqueue >= XHCI_TRANSFER_RING_TRB_COUNT-1) {
        xhci_link_trb_t *link_trb = (xhci_link_trb_t*)&tr->trb_list[XHCI_TRANSFER_RING_TRB_COUNT-1];
        link_trb->type = XHCI_TRB_TYPE_LINK;
        link_trb->tc = 1;
        link_trb->c = tr->cycle;
    
        tr->enqueue = 0;
        tr->cycle = !tr->cycle;
    }

    return 0;
}

/**
 * @brief Wait for transfer to complete
 * @param endpoint The endpoint to wait on
 * @returns 0 on success
 */
int xhci_waitForTransfer(xhci_endpoint_t *endp) {
    while (!__atomic_load_n(&endp->flag, __ATOMIC_SEQ_CST)) {
        if (current_cpu->current_thread) {
            process_yield(1);
        } else {
            arch_pause_single();
        }
    }


    return !endp->ctr || (endp->ctr->completion_code != 1);
}

/**
 * @brief Perform a control transfer on an xHCI port
 */
int xhci_control(USBController_t *controller, USBDevice_t *device, USBTransfer_t *transfer) {
    if (!controller || !device || !transfer || !device->dev) return USB_TRANSFER_FAILED;
    xhci_device_t *dev = (xhci_device_t*)(device->dev);

    mutex_acquire(dev->mutex);
    
    // Start building the TRB chain
    // Control transactions: SETUP -> DATA -> STATUS
    // NOTE: A few sources say that you need to wait for a successful transfer before enqueueing STATUS. I don't care.

    xhci_setup_stage_trb_t setup_trb = {
        .bmRequestType = transfer->req->bmRequestType,
        .bRequest = transfer->req->bRequest,
        .wIndex = transfer->req->wIndex,
        .wLength = transfer->req->wLength,
        .wValue = transfer->req->wValue,
        .transfer_length = 8,
        .interrupter = 0,
        .idt = 1,
        .ioc = 0,
        .type = XHCI_TRB_TYPE_SETUP_STAGE,
        .rsvd0 = 0,
        .rsvd1 = 0,
        .rsvd2 = 0,
        .rsvd3 = 0,
    };

    // Assign TRT
    if (transfer->req->bmRequestType & USB_RT_D2H && transfer->length) {
        setup_trb.trt = 3; // Input
    } else if (transfer->req->bmRequestType & USB_RT_H2D && transfer->length) {
        setup_trb.trt = 2; // Output
    } else {
        setup_trb.trt = 0; // No data stage
    }

    xhci_enqueueTransferTRB(dev->endpoints[0].tr, (xhci_trb_t*)&setup_trb);

    // Build the data TRB
    uintptr_t temp = mem_allocateDMA(4096);
    if (transfer->length) {
        xhci_data_stage_trb_t data_trb = {
            .buffer = mem_getPhysicalAddress(NULL, (uintptr_t)temp),
            .transfer_length = transfer->length,
            .td_size = 0,
            .interrupter = 0,
            .dir = !!(transfer->req->bmRequestType & USB_RT_D2H),
            .ch = 0,
            .ioc = 0,
            .idt = 0,
            .type = XHCI_TRB_TYPE_DATA_STAGE,
        };
        
        xhci_enqueueTransferTRB(dev->endpoints[0].tr, (xhci_trb_t*)&data_trb);
    }

    // Build the status trb
    xhci_status_stage_trb_t status_trb = {
        .type = XHCI_TRB_TYPE_STATUS_STAGE,
        .interrupter = 0,
        .ch = 0,
        .ioc = 1,
        .dir = !((transfer->req->wLength > 0) && (transfer->req->bmRequestType & USB_RT_D2H)),
    };

    xhci_enqueueTransferTRB(dev->endpoints[0].tr, (xhci_trb_t*)&status_trb);
    
    // Ring ring
    XHCI_DOORBELL(dev->parent, dev->slot_id) = 1;

    __atomic_store_n(&dev->endpoints[0].flag, 0, __ATOMIC_SEQ_CST);

    // Wait for transfer to complete
    dev->endpoints[0].ctr = NULL;
    if (xhci_waitForTransfer(&dev->endpoints[0])) {
        LOG(ERR, "Detected a transfer failure during CONTROL transfer\n");
        mutex_release(dev->mutex);
        transfer->status = USB_TRANSFER_FAILED;
        return USB_TRANSFER_FAILED;
    }

    if (transfer->req->bmRequestType & USB_RT_D2H) memcpy(transfer->data, (void*)temp, transfer->length);


    // Done!
    mutex_release(dev->mutex);
    transfer->status = USB_TRANSFER_SUCCESS;
    return USB_TRANSFER_SUCCESS;
}

/**
 * @brief Evaluate xHCI context
 */
int xhci_evaluateContext(USBController_t *controller, USBDevice_t *device) {
    xhci_device_t *dev = (xhci_device_t*)(device->dev);
    if (device->mps == dev->endpoints[0].mps) return USB_SUCCESS; // No need

    // Let's reconfigure the context
    xhci_input_context_t *input_context = XHCI_INPUT_CONTEXT(dev);
    xhci_endpoint_context_t *ep_ctx = XHCI_ENDPOINT_CONTEXT(dev, 1);
    memset((void*)input_context, 0, XHCI_CONTEXT_SIZE(dev->parent));
    input_context->add_flags = 0x1;
    ep_ctx->max_packet_size = device->mps;
    dev->endpoints[0].mps = device->mps;

    // Send evaluate context
    xhci_evaluate_context_trb_t eval = {
        .type = XHCI_CMD_EVALUATE_CONTEXT,
        .bsr = 0,
        .input_context = dev->input_ctx_phys,
        .slot_id = dev->slot_id,
        .rsvd0 = 0,
        .rsvd1 = 0,
        .rsvd2 = 0,
    };

    if (!xhci_sendCommand(dev->parent, (xhci_trb_t*)&eval)) {
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Initialize a device
 * @param xhci The xHCI controller
 * @param port The port number of the device
 * @returns 0 on success
 */
int xhci_initializeDevice(xhci_t *xhci, uint8_t port) {
    LOG(INFO, "Initializing device on port %d\n", port);

    // Enable a slot for the device 
    xhci_enable_slot_trb_t slot;
    memset(&slot, 0, sizeof(xhci_enable_slot_trb_t));
    slot.type = XHCI_CMD_ENABLE_SLOT;

    LOG(DEBUG, "Sending slot TRB\n");
    xhci_command_completion_trb_t *completion = xhci_sendCommand(xhci, (xhci_trb_t*)&slot);
    if (!completion) {
        LOG(ERR, "Error enabling a slot\n");
        return 1;
    }
    LOG(DEBUG, "Slot: %d\n", completion->slot_id);

    // Now we can actually begin initialization
    xhci_device_t *dev = kzalloc(sizeof(xhci_device_t));
    dev->parent = xhci;
    dev->slot_id = completion->slot_id;
    dev->port_id = port;
    dev->mutex = mutex_create("xhci device mutex");

    // Register ourselves
    xhci->slots[dev->slot_id-1] = dev;

    // Initialize input context
    // TODO: DO NOT WASTE TWO WHOLE PAGES
    dev->input_ctx = (xhci_input_context_t*)mem_allocateDMA(4096); // TODO: We can determine target size from xHC
    dev->input_ctx_phys = mem_getPhysicalAddress(NULL, (uintptr_t)dev->input_ctx);
    memset((void*)dev->input_ctx, 0, 4096);

    // Initialize output context
    uintptr_t output_ctx = mem_allocateDMA(4096);
    memset((void*)output_ctx, 0, 4096);
    dev->output_ctx_phys = mem_getPhysicalAddress(NULL, output_ctx);

    // Build endpoint 0 transfer ring
    dev->endpoints[0].tr = xhci_createTransferRing();
    
    // Figure out the speed class
    uint32_t spd = (xhci->op->ports[port].portsc & 0x3c00) >> 10;
    switch (spd) {
        case XHCI_USB_SPEED_LOW_SPEED:
        case XHCI_USB_SPEED_FULL_SPEED:
            dev->endpoints[0].mps = 8;
            break;

        case XHCI_USB_SPEED_HIGH_SPEED:
            dev->endpoints[0].mps = 64;
            break;

        case XHCI_USB_SPEED_SUPER_SPEED:
        case XHCI_USB_SPEED_SUPER_SPEED_PLUS: // TODO: check this
            dev->endpoints[0].mps = 512;
            break;

        default:
            LOG(WARN, "Unrecognized speed: %d\n", spd);
            dev->endpoints[0].mps = 8;
    }

    dev->endpoints[0].m = mutex_create("endp mutex");

    // Make contexts
    xhci_input_context_t *input_ctx = XHCI_INPUT_CONTEXT(dev);
    xhci_slot_context_t *slot_ctx = XHCI_SLOT_CONTEXT(dev);
    xhci_endpoint_context_t *ep_ctx = XHCI_ENDPOINT_CONTEXT(dev, 1);

    // Setu input flags
    input_ctx->add_flags |= 0x3;
    input_ctx->drop_flags = 0x0;

    // Prepare slot context
    slot_ctx->context_entries = 1;
    slot_ctx->root_hub_port_num = (port+1) & 0x0F; // TODO
    slot_ctx->speed = spd;
    slot_ctx->route_string = (port+1)>>4; // TODO
    slot_ctx->interrupter_target = 0;

    // Prepare endpoint context
    ep_ctx->endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
    ep_ctx->max_packet_size = dev->endpoints[0].mps;
    ep_ctx->max_burst_size = 0;
    ep_ctx->max_esit_payload_hi = 0;
    ep_ctx->max_esit_payload_lo = 0;
    ep_ctx->average_trb_length = 0;
    ep_ctx->error_count = 3;
    ep_ctx->state = 0;
    ep_ctx->transfer_ring_dequeue_ptr = dev->endpoints[0].tr->trb_list_phys | 1;

    LOG(DEBUG, "TR dequeue pointer is at %p\n", dev->endpoints[0].tr->trb_list_phys);

    // Set DCBAA
    ((uint64_t*)xhci->dcbaa)[dev->slot_id] = dev->output_ctx_phys;

    // Addressing time
    xhci_address_device_trb_t address_device = {
        .type = XHCI_CMD_ADDRESS_DEVICE,
        .bsr = 1,
        .slot_id = dev->slot_id,
        .input_ctx = dev->input_ctx_phys,
        .c = 0,
        .rsvd0 = 0,
        .rsvd1 = 0,
        .rsvd2 = 0,
    };

    if (xhci_sendCommand(xhci, (xhci_trb_t*)&address_device) == NULL) {
        LOG(ERR, "Failed to initialize device (ADDRESS_DEVICE with BSR = 1 failure)\n");
        return 1;
    }

    address_device.bsr = 0;
    if (xhci_sendCommand(xhci, (xhci_trb_t*)&address_device) == NULL) {
        LOG(ERR, "Failed to initialize device (ADDRESS_DEVICE with BSR = 0 failure)\n");
        return 1;
    }

    // Create the device
    uint32_t speed = USB_LOW_SPEED;

    switch (spd) {
        case XHCI_USB_SPEED_FULL_SPEED:
            speed = USB_FULL_SPEED;
            break;

        case XHCI_USB_SPEED_HIGH_SPEED:
        case XHCI_USB_SPEED_SUPER_SPEED:
        case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
            speed = USB_HIGH_SPEED;
            break;
    }

    LOG(INFO, "!!!!!!!!!!!!!! BEGIN INIT OF USB DEVICE !!!!!!!!!!!!!!\n");
    USBDevice_t *usbdev = usb_createDevice(xhci->controller, port, speed, NULL, xhci_control, NULL);   
    usbdev->dev = (void*)dev;
    usbdev->evaluate = xhci_evaluateContext;

    usb_initializeDevice(usbdev);

    xhci->ports[port].slot_id = dev->slot_id;

    return 0;
}