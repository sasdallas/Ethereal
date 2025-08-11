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
        arch_pause();
        if (current_cpu->current_thread) {
            process_yield(1);
        } else {
            arch_pause_single();
        }
    }


    return (endp->ctr.completion_code != 1);
}

/**
 * @brief Perform a control transfer on an xHCI port
 */
int xhci_control(USBController_t *controller, USBDevice_t *device, USBTransfer_t *transfer) {
    if (!controller || !device || !transfer || !device->dev) return USB_TRANSFER_FAILED;
    xhci_device_t *dev = (xhci_device_t*)(device->dev);
    mutex_acquire(dev->endpoints[0].m);
    
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
    if (transfer->length) {
        xhci_data_stage_trb_t data_trb = {
            .buffer = mem_getPhysicalAddress(NULL, (uintptr_t)transfer->data),
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
    if (xhci_waitForTransfer(&dev->endpoints[0])) {
        LOG(ERR, "Detected a transfer failure during CONTROL transfer\n");
        mutex_release(dev->endpoints[0].m);
        transfer->status = USB_TRANSFER_FAILED;
        return USB_TRANSFER_FAILED;
    }


    // Done!
    mutex_release(dev->endpoints[0].m);
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
    LOG(INFO, "!!!!!!!!!!!! EVALUATING CONTEXT\n");
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
 * @brief Configure a USB device endpoint
 * @param controller USB controller
 * @param device USB device
 * @param endpoint USB endpoint
 */
int xhci_configure(USBController_t *controller, USBDevice_t *device, USBEndpoint_t *endpoint) {
    xhci_device_t *dev = (xhci_device_t*)device->dev;

    mutex_acquire(dev->mutex);

    uint8_t ep_type = 0;
    if (endpoint->desc.bmAttributes & USB_ENDP_TRANSFER_INT) {
        if (endpoint->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_INT_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_INT_OUT;
        }
    } else if (endpoint->desc.bmAttributes & USB_ENDP_TRANSFER_BULK) {
        if (endpoint->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_BULK_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_BULK_OUT;
        }
    } else if (endpoint->desc.bmAttributes & USB_ENDP_TRANSFER_ISOCH) {
        if (endpoint->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) {
            ep_type = XHCI_ENDPOINT_TYPE_ISOCH_IN;
        } else {
            ep_type = XHCI_ENDPOINT_TYPE_ISOCH_OUT;
        }
    } else {
        ep_type = XHCI_ENDPOINT_TYPE_CONTROL;
    }

    // Determine endpoint number + setup endpoint
    uint8_t endp_num = (((endpoint->desc.bEndpointAddress & USB_ENDP_NUMBER) * 2) + !!(endpoint->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN));
    xhci_endpoint_t *ep = &(dev->endpoints[endp_num]);

    ep->m = mutex_create("xhci endpoint mutex");
    ep->tr = xhci_createTransferRing();

    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    xhci_slot_context_t *sc = XHCI_SLOT_CONTEXT(dev);
    xhci_endpoint_context_t *ec = XHCI_ENDPOINT_CONTEXT(dev, endp_num);

    // Enable the endpoint in the input context
    memset(ic, 0, XHCI_CONTEXT_SIZE(dev->parent));
    ic->add_flags = (1 << endp_num) | (1 << 0);
    ic->drop_flags = 0;

    // Setup the slot context
    // TODO: USBHubInformation_t
    // if (endp_num > sc->context_entries) {
        // sc->context_entries = endp_num;
    // }

    // Get the last valid endpoint
    uint32_t last_valid_ep = USB_ENDP_GET_NUMBER(endpoint);
    for (size_t i = last_valid_ep; i < 32; i++) {
        if (dev->endpoints[i].tr) last_valid_ep = i + 1;
    }

    sc->context_entries = last_valid_ep;

    // Setup the endpoint context
    ec->max_packet_size = (USB_ENDP_IS_CONTROL(endpoint) || USB_ENDP_IS_BULK(endpoint)) ? endpoint->desc.wMaxPacketSize : endpoint->desc.wMaxPacketSize & 0x7ff;
    ec->max_burst_size = (USB_ENDP_IS_CONTROL(endpoint) || USB_ENDP_IS_BULK(endpoint)) ? 0 : (endpoint->desc.wMaxPacketSize & 0x1800) >> 11;
    ec->state = XHCI_ENDPOINT_STATE_DISABLED;
    ec->endpoint_type = ep_type;
    ec->error_count = (USB_ENDP_IS_ISOCH(endpoint)) ? 0 : 3;
    ec->transfer_ring_dequeue_ptr = ep->tr->trb_list_phys | 1; 

    uint32_t max_esit = ((ec->max_packet_size * (ec->max_burst_size + 1)));
    ec->max_esit_payload_lo = max_esit & 0xFFFF;
    ec->max_esit_payload_hi = max_esit >> 16;
    ec->average_trb_length = (USB_ENDP_IS_CONTROL(endpoint)) ? 8 : max_esit;

    // Determine interval
    uint32_t speed = ((dev->parent->op->ports[dev->port_id].portsc & 0x3c00) >> 10);


    // !!!: EXTREMELY TEMPORARY (stolen from banan-os)
#define ilog2(val) (sizeof(val) * 8 - __builtin_clz(val) - 1)
#pragma GCC diagnostic ignored "-Wtype-limits"
#define XHCI_CLAMP_INTERVAL(intv, low, high) ((intv < low) ? low : (intv > high) ? high : intv)
    switch (speed) {
        case XHCI_USB_SPEED_HIGH_SPEED:
            if (USB_ENDP_IS_BULK(endpoint) || USB_ENDP_IS_CONTROL(endpoint)) {
                ec->interval = (endpoint->desc.bInterval) ? XHCI_CLAMP_INTERVAL(ilog2(endpoint->desc.bInterval), 0, 15) : 0;
                break;
            }
            // fall through
        case XHCI_USB_SPEED_SUPER_SPEED:
            if (USB_ENDP_IS_ISOCH(endpoint) || USB_ENDP_IS_INTERRUPT(endpoint)) {
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
            if (USB_ENDP_IS_ISOCH(endpoint) || USB_ENDP_IS_INTERRUPT(endpoint)) {
                ec->interval = (endpoint->desc.bInterval) ? XHCI_CLAMP_INTERVAL(ilog2(endpoint->desc.bInterval * 8), 3, 10) : 0;
                break;
            }

            ec->interval = 0;
            break;
    }

    LOG(DEBUG, "Configuring endpoint %d for device on slot %d with speed %d\n", endp_num, dev->slot_id, speed)
    LOG(DEBUG, "max_esit_payload=0x%x max_burst_size=0x%x max_packet_size=%d ep_type=%d error_count=%d trdq=%p avg_trb=%d interval=%d\n", max_esit, ec->max_burst_size, ec->max_packet_size, ec->endpoint_type, ec->error_count, ec->transfer_ring_dequeue_ptr, ec->average_trb_length, ec->interval);

    xhci_configure_endpoint_trb_t trb = {
        .type = XHCI_CMD_CONFIGURE_ENDPOINT,
        .input_context = dev->input_ctx_phys,
        .rsvd0 = 0,
        .rsvd1 = 0,
        .rsvd2 = 0,
        .slot_id = dev->slot_id,
        .dc = 0,
    };

    if (!xhci_sendCommand(dev->parent, (xhci_trb_t*)&trb)) {
        LOG(ERR, "Failed to configure endpoint %d\n", endp_num);
        return USB_FAILURE;
    }
    
    mutex_release(dev->mutex);

    LOG(DEBUG, "Configured endpoint %d\n", endp_num);
    return USB_SUCCESS;
}

/**
 * @brief Interrupt transfer method
 * @param controller USB controller
 * @param device USB device
 * @param transfer USB transfer
 */
int xhci_interrupt(USBController_t *controller, USBDevice_t *device, USBTransfer_t *transfer) {
    xhci_device_t *dev = (xhci_device_t*)device->dev;

    // Get the endpoint they want to transfer to
    uint8_t ep_num = XHCI_ENDPOINT_NUMBER_FROM_DESC(transfer->endp->desc);
    xhci_endpoint_t *ep = &(dev->endpoints[ep_num]);
    if (!ep->tr || !ep->m) {
        LOG(ERR, "Endpoint %d is not configured\n", ep_num);
        return USB_FAILURE;
    }

    mutex_acquire(ep->m);

    xhci_normal_trb_t trb;
    memset(&trb, 0, sizeof(xhci_normal_trb_t));

    trb.type = XHCI_TRB_TYPE_NORMAL;
    trb.buffer = mem_getPhysicalAddress(NULL, (uintptr_t)transfer->data);
    trb.len = transfer->length;
    trb.ioc = 1;
    trb.isp = 1;
    
    xhci_enqueueTransferTRB(ep->tr, (xhci_trb_t*)&trb);

    // Pending interrupt transfer points to this
    ep->pending_int = transfer;

    mutex_release(ep->m);

    // Ring doorbell
    XHCI_DOORBELL(dev->parent, dev->slot_id) = XHCI_ENDPOINT_NUMBER_FROM_DESC(transfer->endp->desc);

    transfer->status = USB_TRANSFER_IN_PROGRESS;
    return USB_TRANSFER_IN_PROGRESS;
}

/**
 * @brief Shutdown the xHCI USB device
 * @param controller USB controller
 * @param device USB device
 */
int xhci_shutdown(USBController_t *controller, USBDevice_t *device) {
    xhci_device_t *dev = (xhci_device_t*)device->dev;

    xhci_disable_slot_trb_t slot;
    memset(&slot, 0, sizeof(xhci_disable_slot_trb_t));
    slot.type = XHCI_CMD_DISABLE_SLOT;
    slot.slot_id = dev->slot_id;

    if (!xhci_sendCommand(dev->parent, (xhci_trb_t*)&slot)) {
        LOG(WARN, "Failed to disable slot %d\n", dev->slot_id);
    } else {
        LOG(INFO, "Slot disabled successfully\n");
    }

    // Free memory of the device
    mutex_destroy(dev->mutex);
    // mem_freeDMA((uintptr_t)dev->input_ctx, 4096);
    // mem_freeDMA((uintptr_t)dev->output_ctx, 4096);
    
    for (int i = 0; i < 32; i++) {
        if (dev->endpoints[i].tr) {
            LOG(DEBUG, "Freeing EP%d\n", i);
            // mem_freeDMA((uintptr_t)dev->endpoints[i].tr->trb_list, XHCI_TRANSFER_RING_TRB_COUNT * sizeof(xhci_trb_t));
            // kfree(dev->endpoints[i].tr);
        }
    }

    dev->parent->slots[dev->slot_id-1] = NULL;
    dev->parent->ports[dev->port_id].slot_id = 0;
    return USB_SUCCESS;
}

/**
 * @brief Port speed to string
 */
static char *xhci_portSpeedToString(uint8_t speed) {
    static char* speed_string[7] = {
        "Invalid",
        "Full Speed (12 MB/s - USB2.0)",
        "Low Speed (1.5 Mb/s - USB 2.0)",
        "High Speed (480 Mb/s - USB 2.0)",
        "Super Speed (5 Gb/s - USB3.0)",
        "Super Speed Plus (10 Gb/s - USB 3.1)",
        "Undefined"
    };

    return speed_string[speed];
}

/**
 * @brief Initialize a device
 * @param xhci The xHCI controller
 * @param port The port number of the device
 * @returns 0 on success
 */
int xhci_initializeDevice(xhci_t *xhci, uint8_t port) {
    LOG(INFO, "Initializing device on port %d\n", port);
    LOG(INFO, "This device has speed: %s\n", xhci_portSpeedToString((xhci->op->ports[port].portsc & 0x3c00) >> 10));

    // Enable a slot for the device 
    xhci_enable_slot_trb_t slot;
    memset(&slot, 0, sizeof(xhci_enable_slot_trb_t));
    slot.type = XHCI_CMD_ENABLE_SLOT;

    xhci_command_completion_trb_t *completion = xhci_sendCommand(xhci, (xhci_trb_t*)&slot);
    if (!completion) {
        LOG(ERR, "Error enabling a slot\n");
        return 1;
    }

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
    dev->output_ctx = output_ctx;

    // Build endpoint 0 transfer ring
    dev->endpoints[0].tr = xhci_createTransferRing();
    
    // Figure out the speed class
    uint32_t spd = (xhci->op->ports[port].portsc & 0x3c00) >> 10;
    switch (spd) {
        case XHCI_USB_SPEED_LOW_SPEED:
            dev->endpoints[0].mps = 8;
            break;

        case XHCI_USB_SPEED_FULL_SPEED:
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

    // Set DCBAA
    ((uint64_t*)xhci->dcbaa)[dev->slot_id] = mem_getPhysicalAddress(NULL, dev->output_ctx);

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
            speed = USB_HIGH_SPEED;
            break;

        case XHCI_USB_SPEED_SUPER_SPEED:
        case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
            speed = USB_SUPER_SPEED;
            break;
    }

    USBDevice_t *usbdev = usb_createDevice(xhci->controller, port, speed, NULL, xhci_control, xhci_interrupt);   
    usbdev->dev = (void*)dev;
    usbdev->evaluate = xhci_evaluateContext;
    usbdev->shutdown = xhci_shutdown;
    usbdev->confendp = xhci_configure;
    dev->dev = usbdev;

    if (usb_initializeDevice(usbdev) != USB_SUCCESS) {
        // TODO: FREE MEMORY
        LOG(WARN, "Device init failed (memory leaked)\n");
        return 1;
    } 

    xhci->ports[port].slot_id = dev->slot_id;
    return 0;
}