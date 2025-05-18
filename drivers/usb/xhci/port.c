/**
 * @file drivers/usb/xhci/port.c
 * @brief xHCI port handler
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
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/drivers/pci.h>
#include <kernel/drivers/clock.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>
#include <string.h>

#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#else
#error "HAL"
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", "[XHCI:PORT] "); \
                            dprintf(NOHEADER, __VA_ARGS__);



/**
 * @brief Hard reset an xHCI port
 * @param xhci The xHCI to reset the port on
 * @param port The port to reset
 * @returns 0 on success
 */
int xhci_portReset(xhci_t *xhci, int port) {
    // Get the PORTSC register
    uint32_t portsc = XHCI_PORTSC_READ(port);

    // First, we need to supply power to the port if needed
    if (!(portsc & XHCI_PORTSC_PP)) {
        portsc |= XHCI_PORTSC_PP;
        XHCI_PORTSC_WRITE(portsc, port);

        // Wait for power to stabilize
        clock_sleep(20);
        
        // Re-read
        portsc = XHCI_PORTSC_READ(port);
        if (!(portsc & XHCI_PORTSC_PP)) {
            // Error, PP was never set
            LOG(ERR, "Failed to reset port %d - power on sequence failure\n", port);
            return 1;
        }
    }

    // Clear the status bits
    portsc |= XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_PRC;
    XHCI_PORTSC_WRITE(portsc, port);

    // USB3 expects a warm reset, while USB2 expects a standard port reset
    if (xhci_portUSB3(xhci, port)) {
        // Warm reset
        portsc |= XHCI_PORTSC_WPR;
    } else {
        // Set the port reset bit
        portsc |= XHCI_PORTSC_PR;
    }

    XHCI_PORTSC_WRITE(portsc, port);

    // Wait for the port status change event
    // if (XHCI_TIMEOUT((pr->portsc & XHCI_PORTSC_PRC) || (pr->portsc & XHCI_PORTSC_WRC), 10000)) {
    //     LOG(ERR, "Failed to reset port %d - timeout while waiting for PRC to set\n", port);
    //     return 1;
    // }

    while (1) {
        portsc = XHCI_PORTSC_READ(port);

        if ((portsc & XHCI_PORTSC_PRC) || (portsc & XHCI_PORTSC_WRC)) {
            break;
        }

        LOG(DEBUG, "Still waiting for PRC to unset: %x\n", portsc);
    }

    // Sleep a bit
    clock_sleep(20);

    // Clear the status bits agai
    portsc = portsc | XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_WRC | XHCI_PORTSC_PRC;
    XHCI_PORTSC_WRITE(portsc, port);

    clock_sleep(10);

    portsc = XHCI_PORTSC_READ(port);

    // Make sure the port is enabled
    if (!(portsc & XHCI_PORTSC_PED)) {
        LOG(WARN, "Port reset completed but port did not enable: %08x\n", portsc);
        return 1;
    }

    return 0;
}

/**
 * @brief Get port speed to string (debug)
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
 * @brief Create a new port device context and set it in the DCBAAp
 * @param xhci The xHCI device to use
 * @param slot The slot to use
 * @returns 0 on success
 */
static int xhci_portCreateDeviceContext(xhci_t *xhci, uint8_t slot_id) {
    // Get context (void * as can either be 32/64)
    uintptr_t ctx = pool_allocateChunk(xhci->ctx_pool);
    if (!ctx) {
        LOG(ERR, "Ran out of memory in xHCI device context pool\n");
        return 1;
    }

    memset((void*)ctx, 0, XHCI_DEVICE_CONTEXT_SIZE(xhci));

    // Install it into the DCBAA
    xhci->dcbaa[slot_id] = mem_getPhysicalAddress(NULL, (uintptr_t)ctx);
    xhci->dcbaa_virt[slot_id] = ctx;
    return 0;
}

/**
 * @brief Wait for transfer to complete
 * @param dev The device to send the control endpoint transfer on
 * @param ring The TRB ring we are waiting on
 */
static xhci_transfer_completion_trb_t *xhci_waitForTransferToComplete(xhci_dev_t *dev, xhci_transfer_ring_t *ring) {
    // TODO: Thread-safeing this properly across multiple USB transfers happening simultaneously is going to be REALLY bad.
    // !!!: There is almost 0 cooperation if two transfers are running at the same time

    while (1) {
        // Wait for completion
        while (!dev->xhci->transfer_queue->length) {
            // We should be executing in the poller thread..
            process_yield(1);
        }


        // Pop the TRB from the queue
        node_t *node = list_popleft(dev->xhci->transfer_queue);
        if (!node) {
            LOG(ERR, "Transfer handled but no completion (?)\n");
            return NULL;
        }

        xhci_transfer_completion_trb_t *ttrb = (xhci_transfer_completion_trb_t*)node->value;
        kfree(node);

        LOG(INFO, "Transfer succeeded - TRB buffer %p\n", ttrb->buffer);
        return ttrb;
    }
}

/**
 * @brief Perform a control transfer on an xHCI port
 */
int xhci_control(USBController_t *controller, USBDevice_t *device, USBTransfer_t *transfer) {
    if (!controller || !device || !transfer || !device->dev) return USB_TRANSFER_FAILED;
    xhci_dev_t *dev = (xhci_dev_t*)(device->dev);

    // Start building the TRB chain
    // Control transactions: SETUP -> DATA -> STATUS
    // NOTE: A few sources say that you need to wait for a successful transfer before enqueueing STATUS. I don't care.

    // Create the setup TRB
    xhci_setup_trb_t setup_trb = {
        .bmRequestType = transfer->req->bmRequestType,
        .bRequest = transfer->req->bRequest,
        .wIndex = transfer->req->wIndex,
        .wLength = transfer->req->wLength,
        .wValue = transfer->req->wValue,
        .transfer_len = 8,
        .interrupter = 0,
        .idt = 1,
        .ioc = 0,
        .type = XHCI_TRB_TYPE_SETUP_STAGE
    };

    // Assign TRT
    if (transfer->req->bmRequestType & USB_RT_D2H && transfer->length) {
        setup_trb.trt = 3; // Input
    } else if (transfer->req->bmRequestType & USB_RT_H2D && transfer->length) {
        setup_trb.trt = 2; // Output
    } else {
        setup_trb.trt = 0; // No data stage
    }

    xhci_enqueueTransferTRB(dev->control_ring, (xhci_trb_t*)&setup_trb);

    // Build the data TRB
    if (transfer->length) {
        xhci_data_trb_t data_trb = {
            .buffer = mem_getPhysicalAddress(NULL, (uintptr_t)transfer->data),
            .transfer_len = transfer->length,
            .td_size = 0,
            .interrupter = 0,
            .dir = 1,
            .ch = 1,
            .ioc = 0,
            .idt = 0,
            .type = XHCI_TRB_TYPE_DATA_STAGE
        };
        
        xhci_enqueueTransferTRB(dev->control_ring, (xhci_trb_t*)&data_trb);
    }   

    // Build the status trb
    xhci_status_trb_t status_trb = {
        .type = XHCI_TRB_TYPE_STATUS_STAGE,
        .interrupter = 0,
        .ch = 0,
        .ioc = 1,
        .dir = (setup_trb.trt ? 0 : 1)
    };

    xhci_enqueueTransferTRB(dev->control_ring, (xhci_trb_t*)&status_trb);
    
    // Flush the transfer queue first if needed
    if (dev->xhci->transfer_queue->length) {
        node_t *tq = list_popleft(dev->xhci->transfer_queue);
        while (tq) {
            kfree(tq);
            tq = list_popleft(dev->xhci->transfer_queue);
        }
    }

    // Transfer
    XHCI_RING_DOORBELL(dev->xhci->capregs, XHCI_DOORBELL_TARGET_CONTROL_EP_RING, dev->slot_id)
    xhci_transfer_completion_trb_t *transfer_trb = xhci_waitForTransferToComplete(dev, dev->control_ring);
    if (transfer_trb) {
        transfer->status = USB_TRANSFER_SUCCESS;
        return USB_TRANSFER_SUCCESS;
    }

    transfer->status = USB_TRANSFER_FAILED;
    return USB_TRANSFER_FAILED;
}

/**
 * @brief Perform an interrupt transfer on an xHCI port
 */
int xhci_interrupt(USBController_t *controller, USBDevice_t *usbdev, USBTransfer_t *transfer) {
    // Get device
    xhci_dev_t *dev = (xhci_dev_t*)usbdev->dev;

    // Interrupt transfers require that we use a Normal TRB
    // First, though, get the endpoint we want to send to
    xhci_endpoint_t *endp = dev->endp[XHCI_ENDPOINT_NUMBER_FROM_DESC(transfer->endp->desc)-1];
    LOG(DEBUG, "INTERRUPT transfer to endpoint %d (buffer %p length %d)\n", endp->num, transfer->data, transfer->length);

    // Construct the TRB
    xhci_normal_trb_t trb = {
        .type = XHCI_TRB_TYPE_NORMAL,
        .data_buffer = mem_getPhysicalAddress(NULL, (uintptr_t)transfer->data),
        .len = transfer->length,
        .ioc = 1,
        .td_size = 0,
        .target = 0,
        .isp = 1,
    };

    // Queue it
    xhci_enqueueTransferTRB(endp->ring, (xhci_trb_t*)&trb);

    // Start the transfer
    XHCI_RING_DOORBELL(dev->xhci->capregs, dev->slot_id, endp->num);
    xhci_transfer_completion_trb_t *transfer_trb = xhci_waitForTransferToComplete(dev, endp->ring);
    if (transfer_trb) {
        transfer->status = USB_TRANSFER_SUCCESS;
        return USB_TRANSFER_SUCCESS;
    }

    LOG(ERR, "Transfer failed\n");
    transfer->status = USB_TRANSFER_FAILED;
    return USB_TRANSFER_FAILED;
}

/**
 * @brief Address the device
 */
int xhci_address(USBController_t *controller, USBDevice_t *device) {
    // Send the address device command
    xhci_address_device_trb_t trb = { 0 };
    trb.type = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
    trb.bsr = 0;
    trb.input_ctx = ((xhci_dev_t*)device->dev)->input_ctx_phys;
    trb.slot_id = ((xhci_dev_t*)device->dev)->slot_id;

    // Send
    xhci_command_completion_trb_t *cmdtrb = xhci_sendCommand(((xhci_dev_t*)device->dev)->xhci, (xhci_trb_t*)&trb);
    if (!cmdtrb) {
        LOG(ERR, "Failed to send ADDRESS_DEVICE command to xHCI\n");
        return USB_FAILURE;
    }

    if (XHCI_CMD_TRB_FAILURE(cmdtrb)) {
        // will handle better later
        LOG(ERR, "Command TRB failed\n");
        return USB_FAILURE;
    }

    // HACK: Now that we're done we should set add_flags to 0x1 to only enable control
    xhci_dev_t *dev = (xhci_dev_t*)device->dev;
    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    ic->control->add_flags = 0x1;

    // All done
    return USB_SUCCESS;
}

/**
 * @brief Evaluate context method
 */
int xhci_evaluateContext(USBController_t *controller, USBDevice_t *device) {
    // Get device
    xhci_dev_t *dev = (xhci_dev_t*)device->dev;

    // Update input context first
    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    ic->device->control_endpoint_context.max_packet_size = device->mps;

    // Send the evaluate context command
    xhci_evaluate_context_trb_t trb = { 0 };
    trb.input_context = dev->input_ctx_phys;
    trb.type = XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD;
    trb.slot_id = dev->slot_id;
    
    xhci_command_completion_trb_t *cmdtrb = xhci_sendCommand(dev->xhci, (xhci_trb_t*)&trb);
    if (!cmdtrb) {
        LOG(ERR, "Command failed: Timed out and no TRB was given\n");
        return USB_FAILURE;
    }

    if (XHCI_CMD_TRB_FAILURE(cmdtrb)) {
        // TODO
        LOG(ERR, "Command failed: Completion code error\n");
        return USB_FAILURE;
    }

    return USB_SUCCESS;
}

/**
 * @brief Configure an endpoint for a device
 * @param controller The controller
 * @param device The device
 * @param endp The endpoint
 */
int xhci_configureEndpoint(USBController_t *controller, USBDevice_t *usbdev, USBEndpoint_t *endp) {
    xhci_dev_t *dev = (xhci_dev_t*)usbdev->dev;

    // Allocate a new xHCI endpoint structure
    xhci_endpoint_t *endpoint = kzalloc(sizeof(xhci_endpoint_t));
    endpoint->dev = dev;
    endpoint->num = XHCI_ENDPOINT_NUMBER_FROM_DESC(endp->desc);

    // Read the endpoint type
    if (endp->desc.bmAttributes & USB_ENDP_TRANSFER_BULK) {
        // BULK
        endpoint->type = (endp->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) ? XHCI_ENDPOINT_TYPE_BULK_IN : XHCI_ENDPOINT_TYPE_BULK_OUT;
    } else if (endp->desc.bmAttributes & USB_ENDP_TRANSFER_INT) {
        // INTERRUPT
        endpoint->type = (endp->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) ? XHCI_ENDPOINT_TYPE_INTERRUPT_IN : XHCI_ENDPOINT_TYPE_INTERRUPT_OUT;
    } else if (endp->desc.bmAttributes & USB_ENDP_TRANSFER_ISOCH) {
        // ISOCHRONOUS
        endpoint->type = (endp->desc.bEndpointAddress & USB_ENDP_DIRECTION_IN) ? XHCI_ENDPOINT_TYPE_ISOCHRONOUS_IN : XHCI_ENDPOINT_TYPE_ISOCHRONOUS_OUT;
    } else {
        // CONTROL
        endpoint->type = XHCI_ENDPOINT_TYPE_CONTROL;
    }

    // Create the transfer ring
    endpoint->ring = xhci_createTransferRing();
    endpoint->desc = &endp->desc;

    // Enable the endpoint in the input context
    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    ic->control->add_flags = (1 << (endpoint->num)) | (1 << 0);
    ic->control->drop_flags = 0;

    if (endpoint->num > ic->device->slot_context.context_entries) {
        ic->device->slot_context.context_entries = endpoint->num;
    }

    // Build the endpoint context
    xhci_endpoint_context32_t *ctx = &ic->device->ep[endpoint->num - 2];
    ctx->endpoint_state = XHCI_ENDPOINT_STATE_DISABLED;
    ctx->endpoint_type = endpoint->type;
    ctx->max_packet_size = endpoint->desc->wMaxPacketSize;
    ctx->max_esit_payload_lo = endpoint->desc->wMaxPacketSize;
    ctx->error_count = 3;
    ctx->max_burst_size = 0;
    ctx->average_trb_length = endpoint->desc->wMaxPacketSize;
    ctx->transfer_ring_dequeue_ptr = mem_getPhysicalAddress(NULL, (uintptr_t)&XHCI_RING_DEQUEUE(endpoint->ring));
    ctx->dcs = endpoint->ring->cycle;
    
    // Setup the interval (get port speed)
    xhci_t *xhci = dev->xhci;
    uint32_t portsc = XHCI_PORTSC_READ(dev->port);
    uint32_t speed = (portsc & XHCI_PORTSC_SPD >> XHCI_PORTSC_SPD_SHIFT);

    if (speed == XHCI_USB_SPEED_HIGH_SPEED || speed == XHCI_USB_SPEED_SUPER_SPEED) {
        ctx->interval = endpoint->desc->bInterval - 1;
    } else {
        ctx->interval = endpoint->desc->bInterval;

        // For Interrupt and Isochronous endpoints, the valid range is 3 - 18.
        if (endpoint->desc->bmAttributes & USB_ENDP_TRANSFER_INT || endpoint->desc->bmAttributes & USB_ENDP_TRANSFER_ISOCH) {
            if (ctx->interval < 3) ctx->interval = 3;
            else if (ctx->interval > 18) ctx->interval = 18;
        }
    }


    // Configure the endpoint
    xhci_configure_endpoint_trb_t trb = {
        .type = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD,
        .slot_id = dev->slot_id,
        .input_context = dev->input_ctx_phys
    };

    xhci_command_completion_trb_t *cmdtrb = xhci_sendCommand(xhci, (xhci_trb_t*)&trb);
    if (!cmdtrb) {
        LOG(ERR, "Command failed: Timed out and no TRB was given\n");
        return USB_FAILURE;
    }

    if (XHCI_CMD_TRB_FAILURE(cmdtrb)) {
        // TODO
        LOG(ERR, "Command failed: Completion code error\n");
        return USB_FAILURE;
    }

    LOG(INFO, "Successfully configured endpoint #%d\n", endpoint->num);
    dev->endp[endpoint->num-1] = endpoint;
    return USB_SUCCESS;
}

/**
 * @brief Try to initialize an xHCI port
 * @param xhci The xHCI device to use
 * @param port The port number to attempt to initialize
 * @returns 0 on success
 */
int xhci_portInitialize(xhci_t *xhci, int port) {
    xhci_port_registers_t *regs = XHCI_PORT_REGISTERS(xhci->opregs, port);

    uint32_t portsc = XHCI_PORTSC_READ(port);

    LOG(INFO, "Port %d reset successfully - initializing\n", port);
    LOG(INFO, "Speed of this port: %s\n", xhci_portSpeedToString(portsc & XHCI_PORTSC_SPD >> XHCI_PORTSC_SPD_SHIFT));
    LOG(INFO, "Removable device: %s\n", (portsc & XHCI_PORTSC_DR ? "YES" : "NO"));
    LOG(DEBUG, "Raw PORTSC: %08x\n", portsc);


    // Enable the device slot
    xhci_trb_t enable_devslot_trb = XHCI_CONSTRUCT_CMD_TRB(XHCI_TRB_TYPE_ENABLE_SLOT_CMD);
    xhci_command_completion_trb_t *slot_trb = xhci_sendCommand(xhci, &enable_devslot_trb);
    if (!slot_trb) {
        LOG(ERR, "Could not enable device slot\n");
        return 1;
    }

    LOG(DEBUG, "Slot %d enabled for port %d\n", slot_trb->slot_id, port);

    // Build device context
    if (xhci_portCreateDeviceContext(xhci, slot_trb->slot_id)) {
        LOG(ERR, "Could not create device context\n");
        return 1;
    }

    // Build a new device
    xhci_dev_t *dev = kzalloc(sizeof(xhci_dev_t));
    dev->slot_id = slot_trb->slot_id;
    // dev->input_ctx = XHCI_CREATE_INPUT_CONTEXT(xhci);
    // memset(dev->input_ctx, 0, XHCI_INPUT_CONTEXT_SIZE(xhci));
    dev->input_ctx = (xhci_input_context32_t*)mem_allocateDMA(PAGE_SIZE);
    memset(dev->input_ctx, 0, PAGE_SIZE);
    dev->input_ctx_phys = mem_getPhysicalAddress(NULL, (uintptr_t)dev->input_ctx);
    dev->control_ring = xhci_createTransferRing();
    dev->xhci = xhci;
    dev->port = port;

    // Configure the control endpoint input context
    xhci_input_context_t *input_context = XHCI_INPUT_CONTEXT(dev);
    input_context->control->add_flags |= 0x3;
    input_context->control->drop_flags = 0;


    // Setup the slot context
    input_context->device->slot_context.context_entries = 1;
    input_context->device->slot_context.root_hub_port_num = port+1;
    input_context->device->slot_context.speed = portsc & XHCI_PORTSC_SPD >> XHCI_PORTSC_SPD_SHIFT;
    input_context->device->slot_context.route_string = 0;
    input_context->device->slot_context.interrupter_target = 0;

    input_context->device->control_endpoint_context.endpoint_state = XHCI_ENDPOINT_STATE_DISABLED;
    input_context->device->control_endpoint_context.endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
    input_context->device->control_endpoint_context.interval = 0;
    input_context->device->control_endpoint_context.error_count = XHCI_ENDPOINT_DEFAULT_ERROR_COUNT;
    input_context->device->control_endpoint_context.transfer_ring_dequeue_ptr = mem_getPhysicalAddress(NULL, (uintptr_t)&XHCI_RING_DEQUEUE(dev->control_ring));
    input_context->device->control_endpoint_context.dcs = dev->control_ring->cycle;
    input_context->device->control_endpoint_context.max_esit_payload_lo = 0;
    input_context->device->control_endpoint_context.max_esit_payload_hi = 0;
    input_context->device->control_endpoint_context.average_trb_length = 8;

    // Determine speed
    uint32_t speed = USB_HIGH_SPEED;
    uint32_t port_speed = regs->portsc & XHCI_PORTSC_SPD >> XHCI_PORTSC_SPD_SHIFT;
    if (port_speed == XHCI_USB_SPEED_SUPER_SPEED || port_speed == XHCI_USB_SPEED_SUPER_SPEED_PLUS) {
        speed = USB_HIGH_SPEED;
        input_context->device->control_endpoint_context.max_packet_size = 512;
    } else if (port_speed == XHCI_USB_SPEED_HIGH_SPEED) {
        speed = USB_HIGH_SPEED;
        input_context->device->control_endpoint_context.max_packet_size = 64;
    } else if (port_speed == XHCI_USB_SPEED_FULL_SPEED) {
        speed = USB_FULL_SPEED;
        input_context->device->control_endpoint_context.max_packet_size = 64;
    } else if (port_speed == XHCI_USB_SPEED_LOW_SPEED) {
        speed = USB_LOW_SPEED;
        input_context->device->control_endpoint_context.max_packet_size = 8;
    } else {
        LOG(ERR, "Unknown speed: 0x%x. Assuming full speed device\n");
        speed = USB_FULL_SPEED;
        input_context->device->control_endpoint_context.max_packet_size = 64;
    }

    // Create a USB device
    USBDevice_t *usbdev = usb_createDevice(xhci->controller, port, speed, NULL, xhci_control, xhci_interrupt);   
    usbdev->dev = (void*)dev;
    usbdev->setaddr = xhci_address;
    usbdev->evaluate = xhci_evaluateContext;
    usbdev->confendp = xhci_configureEndpoint;
    usbdev->mps = input_context->device->control_endpoint_context.max_packet_size;

    // Send the address device command (with BSR)
    xhci_address_device_trb_t trb = { 0 };
    trb.type = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
    trb.bsr = 1;
    trb.input_ctx = dev->input_ctx_phys;
    trb.slot_id = dev->slot_id;

    // Send
    xhci_command_completion_trb_t *cmdtrb = xhci_sendCommand(dev->xhci, (xhci_trb_t*)&trb);
    if (!cmdtrb) {
        LOG(ERR, "Failed to send ADDRESS_DEVICE command to xHCI\n");
        return 1;
    }

    LOG(INFO, "Device addressed successfully\n");
    LOG(INFO, "Initializing USB device: initial mps=%d, address=0x%x\n", usbdev->mps, usbdev->address);

    // Initialize
    if (usb_initializeDevice(usbdev) != USB_SUCCESS) {
        LOG(ERR, "Failed to initialize xHCI device\n");
        usb_destroyDevice(xhci->controller, usbdev);

        // TODO: Free associated xHCI memory
        return 1;
    }

    return 0;
}