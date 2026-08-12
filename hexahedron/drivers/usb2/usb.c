/**
 * @file hexahedron/drivers/usb/usb.c
 * @brief USB subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>
#include <kernel/panic.h>
#include <kernel/mm/slab.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <string.h>
#include <errno.h>

/* Caches */
static slab_cache_t *bus_cache = NULL;
static slab_cache_t *controller_cache = NULL;
static slab_cache_t *device_cache = NULL;
static slab_cache_t *pipe_cache = NULL;
static slab_cache_t *endpoint_cache = NULL;
static slab_cache_t *transfer_cache = NULL;
static slab_cache_t *interface_cache = NULL;
static slab_cache_t *hub_cache = NULL;
static slab_cache_t *port_cache = NULL;
static slab_cache_t *configuration_cache = NULL;

/* Controllers */
mutex_t usb_controllers_lock = MUTEX_INITIALIZER;
DLIST_HEAD(usb_controllers, usb_controller_t);

/* List of devices */
mutex_t usb_device_lock = MUTEX_INITIALIZER;
DLIST_HEAD(usb_devices, usb_device_t);

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB", __VA_ARGS__)

/**
 * @brief Allocate and zero a specific object
 * @param type The object type to allocate
 * @note This isn't fancy, it's just so that the proper slab pool can be selected
 */
void *usb_allocateObject(usb_object_type_t type) {
    slab_cache_t *caches[] = {
        [USB_OBJECT_TYPE_CONTROLLER] = controller_cache,
        [USB_OBJECT_TYPE_BUS] = bus_cache,
        [USB_OBJECT_TYPE_DEVICE] = device_cache,
        [USB_OBJECT_TYPE_INTERFACE] = interface_cache,
        [USB_OBJECT_TYPE_ENDPOINT] = endpoint_cache,
        [USB_OBJECT_TYPE_PIPE] = pipe_cache,
        [USB_OBJECT_TYPE_HUB] = hub_cache,
        [USB_OBJECT_TYPE_TRANSFER] = transfer_cache,
        [USB_OBJECT_TYPE_PORT] = port_cache,
        [USB_OBJECT_TYPE_CONFIGURATION] = configuration_cache
    };

    size_t sizes[] = {
        [USB_OBJECT_TYPE_CONTROLLER] = sizeof(usb_controller_t),
        [USB_OBJECT_TYPE_BUS] = sizeof(usb_bus_t),
        [USB_OBJECT_TYPE_DEVICE] = sizeof(usb_device_t),
        [USB_OBJECT_TYPE_INTERFACE] = sizeof(usb_interface_t),
        [USB_OBJECT_TYPE_ENDPOINT] = sizeof(usb_endpoint_t),
        [USB_OBJECT_TYPE_PIPE] = sizeof(usb_pipe_t),
        [USB_OBJECT_TYPE_HUB] = sizeof(usb_hub_t),
        [USB_OBJECT_TYPE_TRANSFER] = sizeof(usb_transfer_t),
        [USB_OBJECT_TYPE_PORT] = sizeof(usb_port_t),
        [USB_OBJECT_TYPE_CONFIGURATION] = sizeof(usb_configuration_t)
    };

    void *object = slab_allocate(caches[type]);
    memset(object, 0, sizes[type]);
    return object;
}

/**
 * @brief Free a specific object
 * @param type The object type to free
 * @param obj The object
 */
void usb_freeObject(usb_object_type_t type, void *obj) {
    slab_cache_t *caches[] = {
        [USB_OBJECT_TYPE_CONTROLLER] = controller_cache,
        [USB_OBJECT_TYPE_BUS] = bus_cache,
        [USB_OBJECT_TYPE_DEVICE] = device_cache,
        [USB_OBJECT_TYPE_INTERFACE] = interface_cache,
        [USB_OBJECT_TYPE_ENDPOINT] = endpoint_cache,
        [USB_OBJECT_TYPE_PIPE] = pipe_cache,
        [USB_OBJECT_TYPE_HUB] = hub_cache,
        [USB_OBJECT_TYPE_TRANSFER] = transfer_cache,
        [USB_OBJECT_TYPE_PORT] = port_cache,
        [USB_OBJECT_TYPE_CONFIGURATION] = configuration_cache
    };

    slab_free(caches[type], obj);
    // LOG(DEBUG, "Freed object %p (type %d)\n", obj, type);
}

/**
 * @brief Allocate a new USB controller object
 * @returns A new USB controller object
 */
usb_controller_t *usb_allocateController() {
    usb_controller_t *controller = usb_allocateObject(USB_OBJECT_TYPE_CONTROLLER);
    DLIST_INIT(&controller->buses);
    controller->wq = workqueue_create("usb_hcwq", WORKQUEUE_DEFAULT);
    return controller;
}

/**
 * @brief USB register a new controller
 * @param controller The controller to register
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_registerController(usb_controller_t *controller) {
    mutex_acquire(&usb_controllers_lock);
    DLIST_INSERT_TAIL(&usb_controllers, controller, node);
    mutex_release(&usb_controllers_lock);
    return USB_SUCCESS;
}

/**
 * @brief Create a new USB bus
 * @param controller Parental USB controller
 * @param revision Revision of the USB controller
 * @param ops Operations for the bus
 * @param priv Private pointer of the bus
 * @param out Bus output
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_createBus(usb_controller_t *controller, uint8_t revision, usb_bus_ops_t *ops, void *priv, usb_bus_t **out) {
    usb_bus_t *bus = usb_allocateObject(USB_OBJECT_TYPE_BUS);
    DLIST_INIT(&bus->devices);
    MUTEX_INIT(&bus->lock);
    bus->hc = controller;
    bus->revision = revision;
    bus->ops = ops;
    bus->priv = priv;
    bus->root_hub = NULL;

    // Determine root hub speed
    usb_speed_t speed;
    switch (revision) {
        case USB_REVISION_1_0:
        case USB_REVISION_1_1:
            speed = USB_SPEED_FULL;
            break;
        
        case USB_REVISION_2_0:
            speed = USB_SPEED_HIGH;
            break;

        case USB_REVISION_3_0:
            speed = USB_SPEED_SUPER;
            break;

        case USB_REVISION_3_1:
            speed = USB_SPEED_SUPER_PLUS;
            break;

        default:
            assert(0 && "invalid usb revision");
    }

    // Create the root hub
    usb_status_t status = usb_createDevice(bus, NULL, speed, &bus->root_hub);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to create device root hub: %s\n", usb_strerror(status));
        
        slab_free(bus_cache, bus);
        return status;
    }

    // Append to controller's bus list
    DLIST_INSERT_TAIL(&controller->buses, bus, node);
    if (out) *out = bus;
    return USB_SUCCESS;
}

/**
 * @brief Creates and registers a new device for the USB subsystem
 * @param bus The bus the device was attached to
 * @param port The hub port the device is attached to
 * @param speed The speed of the device
 * @param out Output pointer for device
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_createDevice(usb_bus_t *bus, usb_port_t *port, usb_speed_t speed, usb_device_t **out) {
    LOG(DEBUG, "Creating device on port %p with speed %d on bus %p\n", port, speed, bus);
    usb_device_t *dev = usb_allocateObject(USB_OBJECT_TYPE_DEVICE);
    dev->bus = bus;
    dev->hub = (port != NULL) ? port->hub : NULL;
    dev->port = port;
    dev->attached_hub = NULL;
    dev->state = USB_DEVICE_DEFAULT;
    dev->speed = speed;
 
    if (port) {
        dev->depth = port->hub->self->depth + 1;
    } else {
        dev->depth = 0;
    }

    if (port) {
        port->device = dev;
    }

    // Initialize the new device 
    usb_status_t status = dev->bus->ops->new_device(dev->bus, dev);
    if (USB_ERROR(status)) {
        LOG(ERR, "Bus new_device failed: %s\n", usb_strerror(status));
        return status;
    }

    // Initialize the device's control endpoint
    usb_endpoint_desc_t desc = {
        .bDescriptorType = USB_DESC_ENDP,
        .bEndpointAddress = 0x00,
        .bLength = sizeof(usb_endpoint_desc_t),
        .bmAttributes = 0x00,
        .wMaxPacketSize = 0
    };

    // set a temporary max packet size
    if (speed == USB_SPEED_LOW || speed == USB_SPEED_FULL) {
        desc.wMaxPacketSize = 8; // full speed is temporary until updated by the device desc
    } else if (speed == USB_SPEED_HIGH) {
        desc.wMaxPacketSize = 64;
    } else if (speed == USB_SPEED_SUPER || speed == USB_SPEED_SUPER_PLUS) {
        desc.wMaxPacketSize = 512; // TODO: verify this is right for USB_SPEED_SUPER_PLUS
    } else {
        assert(0);
    }

    dev->control_ep.desc = desc;
    dev->control_ep.interface = NULL;
    dev->control_ep.mps = desc.wMaxPacketSize;
    dev->control_ep.toggle = 0;
    dev->control_ep.pipe = NULL;

    status = usb_openControlPipe(dev, &dev->control);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error opening control pipe\n");
        slab_free(device_cache, dev);
        return status;
    }

    // Initialize the device
    status = usb_initializeDevice(dev);
    if (USB_ERROR(status)) {
        LOG(ERR, "Error initializing device\n");
        slab_free(device_cache, dev);
        return status;
    }

    DLIST_INSERT_TAIL(&bus->devices, dev, c_node);
    
    mutex_acquire(&usb_device_lock);
    DLIST_INSERT_TAIL(&usb_devices, dev, node);
    mutex_release(&usb_device_lock);

    if (out) *out = dev;
    return USB_SUCCESS;
}

/**
 * @brief Remove a USB device (detach)
 * @param device The device to remove
 * @warning THIS ONLY SUPPORTS DEVICE UNPLUGGING AT THE MOMENT!
 */
void usb_removeDevice(usb_device_t *device) {
    if (device->state == USB_DEVICE_DISCONNECTING) {
        LOG(WARN, "Attempted to close device %p while already closing\n", device);
        return;
    }

    device->state = USB_DEVICE_DISCONNECTING;

    // remove from the bus devices list
    mutex_acquire(&device->bus->lock);
    DLIST_REMOVE(&device->bus->devices, usb_device_t, device, c_node);
    mutex_release(&device->bus->lock);

    // remove from the global list
    mutex_acquire(&usb_device_lock);
    DLIST_REMOVE(&usb_devices, usb_device_t, device, node);
    mutex_release(&usb_device_lock);

    // Close the control pipe first, then close the rest.
    usb_closePipe(device->control);

    // TODO: Make this safe under all conditions
    //       It's safe enough to unplug devices and mostly be okay, since multi-threaded processing should usually
    //       stop everything upon seeing that the pipe is closed butttt who knows.
    //       I dunno, bad design, objects need refcounts, yada yada.
    for (unsigned i = 0; i < device->num_configs; i++) {
        usb_configuration_t *config = device->configs[i];
        for (unsigned j = 0; j < config->num_interfaces; j++) {
            usb_interface_t *intf = config->interfaces[j];
            if (intf->driver) {
                intf->driver->ops.detach(device, intf);
            }

            for (unsigned k = 0; k < intf->num_endpoints; k++) {
                usb_endpoint_t *endp = intf->endpoints[k];
                if (endp->pipe != NULL) {
                    usb_closePipe(endp->pipe);
                }

                usb_freeObject(USB_OBJECT_TYPE_ENDPOINT, endp);
            }

            intf->num_endpoints = 0;
            kfree(intf->endpoints);
            usb_freeObject(USB_OBJECT_TYPE_INTERFACE, intf);
        }

        kfree(config->interfaces);
        usb_freeObject(USB_OBJECT_TYPE_CONFIGURATION, config);
    }

    // All pipes are closed, notify the HC of the device removal and allow it
    // to free resources.
    device->bus->ops->remove_device(device->bus, device);

    // Bye bye mr. device
    usb_freeObject(USB_OBJECT_TYPE_DEVICE, device);
}

/**
 * @brief Open a control pipe
 * @param dev The device to open the control pipe for
 * @param pipe_out Output pointer for USB pipe
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_openControlPipe(usb_device_t *dev, usb_pipe_t **pipe_out) {
    assert (dev->control_ep.pipe == NULL && "double opening not implemented");
    usb_pipe_t *pipe = usb_allocateObject(USB_OBJECT_TYPE_PIPE);
    MUTEX_INIT(&pipe->lock);
    WAIT_QUEUE_INIT(&pipe->waiters);
    DLIST_INIT(&pipe->transfers);
    refcount_init(&pipe->ref, 1); // device holds one reference to this pipe
    pipe->device = dev;
    pipe->interface = NULL;
    pipe->endp = &dev->control_ep;
    pipe->flags = USB_PIPE_DEFAULT;
    pipe->state.running = false;
    pipe->state.aborting = false;
    pipe->state.closing = false;
    pipe->state.flushing = false;
    pipe->state.flushpend = false;
    dev->control = pipe;
    dev->control_ep.pipe = pipe;
    
    usb_status_t status = dev->bus->ops->open_pipe(dev->bus, pipe);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to open control pipe: %s\n", usb_strerror(status));
        usb_freeObject(USB_OBJECT_TYPE_PIPE, pipe);
        dev->control_ep.pipe = NULL;
        return status;
    }

    if (pipe_out) {
        *pipe_out = pipe;
    }

    return USB_SUCCESS;
}

/**
 * @brief Open a pipe on a USB device
 * @param endpoint The endpoint to open he pipe to
 * @param flags The flags to open the pipe with
 * @param pipe_out Output pointer for USB pipe
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_openPipe(usb_endpoint_t *endpoint, uint8_t flags, usb_pipe_t **pipe_out) {
    assert(endpoint->pipe == NULL && "opening pipes on the same endpoint not supported yet");

    usb_pipe_t *pipe = usb_allocateObject(USB_OBJECT_TYPE_PIPE);
    MUTEX_INIT(&pipe->lock);
    WAIT_QUEUE_INIT(&pipe->waiters);
    DLIST_INIT(&pipe->transfers);
    refcount_init(&pipe->ref, 1); // device holds one reference to this pipe
    pipe->device = endpoint->interface->device;
    pipe->interface = endpoint->interface;
    pipe->endp = endpoint;
    pipe->flags = flags;
    pipe->state.running = false;
    pipe->state.aborting = false;
    pipe->state.closing = false;
    pipe->state.flushing = false;
    pipe->state.flushpend = false;
    endpoint->pipe = pipe;

    usb_status_t status = pipe->device->bus->ops->open_pipe(pipe->device->bus, pipe);
    if (USB_ERROR(status)) {
        LOG(ERR, "Failed to open pipe: %s\n", usb_strerror(status));
        endpoint->pipe = NULL;
        usb_freeObject(USB_OBJECT_TYPE_PIPE, pipe);
        return status;
    }

    if (pipe_out) {
        *pipe_out = pipe;
    }

    return USB_SUCCESS;
}

/**
 * @brief Close a pipe
 * @param pipe The pipe to close
 */
void usb_closePipe(usb_pipe_t *pipe) {
    USB_HOLD_PIPE(pipe);
    mutex_acquire(&pipe->lock);

    // Mark this pipe as closing AND aborting to make sure new transfers will die off
    pipe->state.closing = true;
    pipe->state.aborting = true;

    // Flush any pending transfers, aborting them
    while (true) {
        usb_transfer_t *transfer = DLIST_FIRST(&pipe->transfers);
        if (transfer == NULL) {
            break;
        }

        transfer->status = USB_ABORTED;
        transfer->actual_length = 0;
        usb_transferCompleteLocked(transfer);
    }

    // close the pipe
    usb_bus_t *bus = pipe->device->bus;
    bus->ops->close_pipe(bus, pipe);

    pipe->endp->pipe = NULL;

    mutex_release(&pipe->lock);
    USB_RELEASE_PIPE(pipe);

    // release the initial reference that the device held
    USB_RELEASE_PIPE(pipe);
}

/**
 * @brief Create a new USB transfer object
 * @param device The device to allocate the transfer from
 * @returns A new transfer object
 */
usb_transfer_t *usb_allocateTransfer(usb_device_t *device) {
    // TODO allow devices to expose alloc_transfer method
    usb_transfer_t *transfer = usb_allocateObject(USB_OBJECT_TYPE_TRANSFER);
    transfer->status = USB_NOT_STARTED;
    return transfer;
}

/**
 * @brief Free transfer
 * @param transfer The transfer to free
 */
void usb_freeTransfer(usb_transfer_t *transfer) {
    slab_free(transfer_cache, transfer);
}

/**
 * @brief Wait for transfer to complete
 */
static usb_status_t usb_waitTransfer(usb_transfer_t *transfer) {
    // TODO: timeout support
    while (1) {
        int r = WAIT_QUEUE_CONDITION_TIMEOUT(&transfer->pipe->waiters, transfer->status != USB_IN_PROGRESS, transfer->timeout);

        if (r == 0) {
            break;
        } else if (r == -EINTR) {
            LOG(ERR, "Cannot interrupt syncronous USB transfer.\n");
            continue;
        } else if (r == -ETIMEDOUT) {
            // This transfer must be aborted
            LOG(ERR, "Transfer timed out, abort transfer.\n");
            usb_abortTransfer(transfer);
            return USB_ABORTED;
        } else {
            LOG(ERR, "Unknown error when performing waitqueue_wait (%d).\n", r);
            LOG(ERR, "Aborting transfer as failsafe\n");
            usb_abortTransfer(transfer);
            return USB_ABORTED;
        }
    }

    // this transfer may become visible while the pipe is still processing it
    // quickly acquire the pipe lock
    mutex_acquire(&transfer->pipe->lock);
    arch_pause_single();
    mutex_release(&transfer->pipe->lock);

    return transfer->status;
}

/**
 * @brief Flush a pipe while locked
 * @param pipe The pipe to flush 
 */
static void usb_flushPipeLocked(usb_pipe_t *pipe) {
    if (pipe->state.flushing == true) {
        // this pipe is already being flushed
        pipe->state.flushpend = true;
        return;
    }

    pipe->state.flushing = true;
    do {
        // acknowledge the pending flush request
        pipe->state.flushpend = false;

        if (pipe->state.running || pipe->state.aborting || pipe->state.closing) {
            // pipe is a state not ready to accept another transfer
            break;
        }

        usb_transfer_t *transfer = DLIST_FIRST(&pipe->transfers);
        if (transfer == NULL) {
            break;
        }

        pipe->state.running = true;
        transfer->status = USB_IN_PROGRESS;

        // start the transfer
        usb_status_t status = pipe->ops->start(pipe, transfer);
        if (USB_ERROR(status)) {
            transfer->status = status;
            transfer->actual_length = 0;
            
            // transferComplete removes from list + sets running to false
            usb_transferCompleteLocked(transfer);

            LOG(ERR, "Failed to flush transfer %p: %s\n", transfer, usb_strerror(status));
        }

        // if completion happened syncronously (such as roothub), then transferComplete
        // automatically sets running to false and requests another transfer.
    } while (pipe->state.flushpend && !pipe->state.running);

    pipe->state.flushing = false;
}

/**
 * @brief Request a pipe flush (locked)
 * @param pipe The pipe to request on
 */
static void usb_requestFlushLocked(usb_pipe_t *pipe) {
    pipe->state.flushpend = true;
    
    if (pipe->state.flushing == false) {
        usb_flushPipeLocked(pipe);
    }
}


/**
 * @brief Perform a transfer on a USB device
 * @param transfer The transfer to execute
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_transfer(usb_transfer_t *transfer) {
    if (transfer->status != USB_NOT_STARTED) {
        LOG(ERR, "Transfer is not in USB_NOT_STARTED state\n");
        return USB_INVALID;
    }

    usb_pipe_t *pipe = transfer->pipe;

    USB_HOLD_PIPE(pipe);
    mutex_acquire(&pipe->lock);

    // If the pipe is aborting/closing it cannot accept new transfers.
    // If its closing it can be freed in _leave_error
    usb_status_t status;
    if (pipe->state.aborting || pipe->state.closing) {
        status = USB_ABORTED;
        goto _leave_error;
    }

    // Initialize the transfer with the pipe
    status = pipe->ops->init_transfer(pipe, transfer);
    if (USB_ERROR2(status)) {
        LOG(ERR, "Failed to init transfer: %s\n", usb_strerror(status));
        goto _leave_error;
    }

    // Prepare to submit the transfer
    DLIST_INSERT_TAIL(&pipe->transfers, transfer, node);
    status = pipe->ops->submit(pipe, transfer);
    if (USB_ERROR2(status)) {
        LOG(ERR, "Failed to submit transfer: %s\n", usb_strerror(status));

        // Nothing is going to complete this transfer
        DLIST_REMOVE(&pipe->transfers, usb_transfer_t, transfer, node);

        // Bye Mr. Transfer
        pipe->ops->free_transfer(pipe, transfer);
        goto _leave_error;
    }

    // Transfer advances to in progress on submit, and ownership of the status field goes to the HC
    assert(transfer->status == USB_IN_PROGRESS);

    // Will enter a flush loop for every pending
    usb_requestFlushLocked(pipe);

    // Now release the lock to wait if needed
    mutex_release(&pipe->lock);
    USB_RELEASE_PIPE(pipe);

    if (transfer->flags & USB_TRANSFER_SYNC) {
        status = usb_waitTransfer(transfer);
        if (USB_ERROR(status)) {
            return status;
        }
    }

    return transfer->status;

_leave_error:
    transfer->status = status;
    mutex_release(&pipe->lock);
    USB_RELEASE_PIPE(pipe);
    return status;
}

/**
 * @brief Complete a transfer (locked)
 * @param transfer The transfer to complete
 * 
 * Must be called with the pipe's lock held
 */
void usb_transferCompleteLocked(usb_transfer_t *transfer) {
    usb_pipe_t *pipe = transfer->pipe;
    if (transfer->status == USB_IN_PROGRESS) {
        BUG("transferComplete detected the transfer is still USB_IN_PROGRESS");
    }

    // If this is called from a context where DLIST_FIRST != transfer
    // then this transfer does not exist in the pipe transfer list
    bool flush_pipe = false;
    if (DLIST_FIRST(&pipe->transfers) == transfer) {
        assert(DLIST_FIRST(&pipe->transfers) == transfer);
        assert(pipe->state.running == true);
        DLIST_REMOVE(&pipe->transfers, usb_transfer_t, transfer, node);
        
        // allow the pipe to continue flushing if part of a syncronous loop
        pipe->state.running = false;
        flush_pipe = true;
    }

    // if short transfers are not allowed, fail this transfer
    if (transfer->status == USB_SUCCESS && transfer->actual_length < transfer->length && (transfer->flags & USB_TRANSFER_ALLOW_SHORT) == 0) {
        LOG(WARN, "Detected short transfer (actlen=%d len=%d)\n", transfer->actual_length, transfer->length);
        transfer->status = USB_SHORT_TRANSFER;
    }

    // since callback is allowed to do whatever to this transfer, it cannot be trusted
    bool is_sync = transfer->flags & USB_TRANSFER_SYNC;

    // request another flush
    if (flush_pipe) {
        usb_requestFlushLocked(pipe);
    }

    if (transfer->has_work) {
        // Unholy cast lmao
        transfer->has_work = false;
        workqueue_add(pipe->device->bus->hc->wq, &transfer->callback);
    }

    if (is_sync) {
        waitqueue_wakeup(&pipe->waiters, 1);
    }
}


/**
 * @brief Complete a transfer
 * @param transfer The transfer to create
 * @param status The transfer status to complete with (since this is called from unlocked context)
 */
void usb_transferComplete(usb_transfer_t *transfer, usb_status_t status) {
    usb_pipe_t *pipe = transfer->pipe;

    USB_HOLD_PIPE(pipe);
    mutex_acquire(&pipe->lock);

    transfer->status = status;
    usb_transferCompleteLocked(transfer);
    
    mutex_release(&pipe->lock);
    USB_RELEASE_PIPE(pipe);
}

/**
 * @brief Abort a transfer (locked)
 * @param transfer The transfer to abort
 */
void usb_abortTransferLocked(usb_transfer_t *transfer) {
    usb_pipe_t *pipe = transfer->pipe;

    if (transfer->status == USB_NOT_STARTED) {
        transfer->status = USB_ABORTED;
        transfer->actual_length = 0;
        return usb_transferCompleteLocked(transfer);
    }

    if (transfer->status != USB_IN_PROGRESS) {
        // This transfer has already finished
        LOG(DEBUG, "Tried to abort already completed transfer (all good)\n");
        return;
    }

    bool active = pipe->state.running && DLIST_FIRST(&pipe->transfers);
    if (active) {
        DLIST_REMOVE(&pipe->transfers, usb_transfer_t, transfer, node);

        transfer->status = USB_ABORTED;
        transfer->actual_length = 0;
        
        usb_transferCompleteLocked(transfer);
    } else {
        pipe->ops->abort(pipe, transfer);
    }
}

/**
 * @brief Abort pipe while locked
 * @param pipe The pipe to abort
 */
void usb_abortPipeLocked(usb_pipe_t *pipe) {
    usb_transfer_t *transfer = DLIST_FIRST(&pipe->transfers);
    pipe->state.aborting = true;

    while (transfer) {
        usb_abortTransferLocked(transfer);
        transfer = DLIST_NEXT(transfer, node);
    }
}

/**
 * @brief Abort a transfer
 * @param transfer The transfer to abort
 */
void usb_abortTransfer(usb_transfer_t *transfer) {
    usb_pipe_t *pipe = transfer->pipe;

    mutex_acquire(&pipe->lock);
    usb_abortTransferLocked(transfer);
    mutex_release(&pipe->lock);
}

/**
 * @brief Abort a pipe
 * @param pipe The pipe to abort
 */
void usb_abortPipe(usb_pipe_t *pipe) {
    mutex_acquire(&pipe->lock);
    usb_abortPipeLocked(pipe);
    mutex_release(&pipe->lock);
}

/**
 * @brief USB initialize method
 */
static int usb_init() {
    // Create all USB caches
    bus_cache = slab_createCache("usb bus cache", SLAB_CACHE_DEFAULT, sizeof(usb_bus_t), 0, NULL, NULL);
    controller_cache = slab_createCache("usb controller cache", SLAB_CACHE_DEFAULT, sizeof(usb_controller_t), 0, NULL, NULL);
    device_cache = slab_createCache("usb device cache", SLAB_CACHE_DEFAULT, sizeof(usb_device_t), 0, NULL, NULL);
    pipe_cache = slab_createCache("usb pipe cache", SLAB_CACHE_DEFAULT, sizeof(usb_pipe_t), 0, NULL, NULL);
    endpoint_cache = slab_createCache("usb endp cache", SLAB_CACHE_DEFAULT, sizeof(usb_endpoint_t), 0, NULL, NULL);
    transfer_cache = slab_createCache("usb transfer cache", SLAB_CACHE_DEFAULT, sizeof(usb_transfer_t), 0, NULL, NULL);
    interface_cache = slab_createCache("usb interface cache", SLAB_CACHE_DEFAULT, sizeof(usb_interface_t), 0, NULL, NULL);
    hub_cache = slab_createCache("usb hub cache", SLAB_CACHE_DEFAULT, sizeof(usb_hub_t), 0, NULL, NULL);
    port_cache = slab_createCache("usb hub cache", SLAB_CACHE_DEFAULT, sizeof(usb_port_t), 0, NULL, NULL);
    configuration_cache = slab_createCache("usb config cache", SLAB_CACHE_DEFAULT, sizeof(usb_configuration_t), 0, NULL, NULL);
    
    return 0;
}

KERN_EARLY_INIT_ROUTINE(usb, INIT_FLAG_DEFAULT, usb_init);
