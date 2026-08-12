/**
 * @file hexahedron/include/kernel/drivers/usb2/usb.h
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

#ifndef DRIVERS_USB_USB_H
#define DRIVERS_USB_USB_H

/**** INCLUDES ****/
#include <kernel/drivers/usb2/protocol.h>
#include <kernel/drivers/usb2/driver.h>
#include <kernel/drivers/usb2/status.h>
#include <kernel/drivers/usb2/hid.h>
#include <kernel/task/workqueue.h>
#include <kernel/misc/waitqueue.h>
#include <kernel/misc/mutex.h>
#include <kernel/refcount.h>
#include <structs/bitmap.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

#define USB_REVISION_1_0        0
#define USB_REVISION_1_1        1
#define USB_REVISION_2_0        2
#define USB_REVISION_3_0        3
#define USB_REVISION_3_1        4

/* Transfer flags */
#define USB_TRANSFER_DEFAULT        0x0
#define USB_TRANSFER_SYNC           0x1 // Complete transfer syncronously
#define USB_TRANSFER_ALLOW_SHORT    0x2 // Short transfers are allowed, meaning that actlen can be <= len

/* Pipe flags */
#define USB_PIPE_DEFAULT            0x0

/* Timeouts */
#define USB_NO_TIMEOUT          -1      // Wait infinitely for this transfer to complete

#define USB_LANGID_ENGLISH      0x0401

#define USB_MAX_ADDRESS         128

/**** TYPES ****/

struct usb_controller;
struct usb_bus;
struct usb_device;
struct usb_interface;
struct usb_endpoint;
struct usb_pipe;
struct usb_hub;
struct usb_transfer;
struct usb_port;
struct usb_configuration;


/* Object types for usb_allocateObject */
typedef enum usb_object_type {
    USB_OBJECT_TYPE_CONTROLLER,
    USB_OBJECT_TYPE_BUS,
    USB_OBJECT_TYPE_DEVICE,
    USB_OBJECT_TYPE_INTERFACE,
    USB_OBJECT_TYPE_ENDPOINT,
    USB_OBJECT_TYPE_PIPE,
    USB_OBJECT_TYPE_HUB,
    USB_OBJECT_TYPE_TRANSFER,
    USB_OBJECT_TYPE_PORT,
    USB_OBJECT_TYPE_CONFIGURATION
} usb_object_type_t;


typedef void (*usb_callback_t)(struct usb_transfer *);

typedef struct usb_bus_ops {
    // should fill ops structure in the pipe
    usb_status_t (*open_pipe)(struct usb_bus *, struct usb_pipe *);

    // not all transfers may be done when this is called, however no new ones will
    // be submitted. you should free your resources, flushing existing transfers as
    // USB_ABORTED
    void (*close_pipe)(struct usb_bus *, struct usb_pipe *);
    
    // order: new_device -> get mps -> configure_control -> address_device
    // if you do not provide an address_device op, SET_ADDRESS will be used instead
    usb_status_t (*new_device)(struct usb_bus*, struct usb_device*);
    usb_status_t (*configure_control)(struct usb_bus *, struct usb_device *);
    usb_status_t (*address_device)(struct usb_bus *, struct usb_device *);
    void (*remove_device)(struct usb_bus*, struct usb_device*);

    // used only for a few requests
    usb_status_t (*root_hub_control)(struct usb_bus*,struct usb_transfer*);
} usb_bus_ops_t;

// all pipe operations are called with pipe's lock held
typedef struct usb_pipe_ops {
    // called once transfer has been setup during usb_transfer()
    usb_status_t (*init_transfer)(struct usb_pipe *, struct usb_transfer *);
    
    // called on freeTransfer if it was setup. note that transfer status may be USB_NOT_STARTED
    // so inspect transfer status before freeing resources
    void (*free_transfer)(struct usb_pipe *, struct usb_transfer *);

    // submit a transfer to inner queue structure
    // you may not complete this transfer until the start operation is called
    usb_status_t (*submit)(struct usb_pipe *, struct usb_transfer *);

    // start a transfer, by this point pipe driver is authorized to complete this transfer
    // if the transfer is rejected, it must be released back to USB core where it will be completed with errors
    // start should only return errors that come from starting the transfer,
    // even if the transfer fails it should return USB_SUCCESS and place the error in transfer->status
    usb_status_t (*start)(struct usb_pipe *, struct usb_transfer *);

    // abort a transfer, self-explanatory
    void (*abort)(struct usb_pipe *, struct usb_transfer *);
} usb_pipe_ops_t;

// lifecycle:
// begins at default after object creation
// transitions to ADDRESSED after SET_ADDRESS/address_device succeeds
// after configuration probing, transitions to READY
// on unplug, transitions to USB_DEVICE_DISCONNECTING while all resources are flushed and cleaned
typedef enum usb_device_state {
    USB_DEVICE_DEFAULT,
    USB_DEVICE_ADDRESSED,
    USB_DEVICE_READY,
    USB_DEVICE_DISCONNECTING
} usb_device_state_t;

typedef struct usb_controller {
    DLIST_HEAD(buses, struct usb_bus);
    DLIST_ENTRY(struct usb_controller) node;

    workqueue_t *wq;                // Used on transfer completions
    void *priv;                     // For usage by HC
} usb_controller_t;

typedef struct usb_bus {
    struct usb_controller *hc;
    struct usb_device *root_hub;
    usb_bus_ops_t *ops;
    mutex_t lock;

    uint8_t revision;               // USB revision, used to determine root hub speed

    DLIST_HEAD(devices, struct usb_device);
    DLIST_ENTRY(struct usb_bus) node;

    // For HCs that don't provide their own method of addressing a device
    BITMAP_DEFINE(address_map, USB_MAX_ADDRESS);

    void *priv;                     // For usage by HC
} usb_bus_t;

typedef enum usb_port_state {
    USB_PORT_IDLE,
    USB_PORT_RESETTING,
    USB_PORT_CONNECTED,
    USB_PORT_DISCONNECTING,
} usb_port_state_t;

typedef struct usb_port {
    struct usb_hub *hub;
    struct usb_device *device;
    usb_port_state_t state;
    unsigned int number;
    usb_speed_t speed;
} usb_port_t;

typedef struct usb_hub {
    struct usb_device *self;
    unsigned int num_ports;
    struct usb_port *ports;
} usb_hub_t;

typedef struct usb_interface {
    struct usb_device *device;
    struct usb_configuration *config;
    usb_interface_desc_t desc;
    unsigned int num_endpoints;
    struct usb_endpoint **endpoints;
    usb_driver_t *driver;
    void *driver_priv;
} usb_interface_t;

typedef struct usb_endpoint {
    // the control endpoint does not need a pointer to device, as the pipe stores it.
    // accesses directly to control_ep are not permitted
    struct usb_interface *interface;
    usb_endpoint_desc_t desc;
    unsigned int mps;
    unsigned int toggle;
    struct usb_pipe *pipe;
} usb_endpoint_t;

typedef struct usb_configuration {
    usb_config_desc_t desc;
    uint8_t index;
    unsigned int num_interfaces;
    usb_interface_t **interfaces;
} usb_configuration_t;

typedef struct usb_device {
    struct usb_bus *bus;                // Registered host controller for the device
    struct usb_hub *hub;                // PARENTAL hub the device is attached to (or NULL if root hub)
    struct usb_port *port;              // Port the device is attached to (or NULL if root hub)
    struct usb_hub *attached_hub;       // This is valid if the device itself is a USB hub.

    usb_device_desc_t desc;             // Device descriptor

    struct usb_pipe *control;           // The pipe for the control endpoint
    struct usb_endpoint control_ep;     // Control endpoint

    usb_device_state_t state;
    usb_speed_t speed;
    uint16_t langid;
    uint8_t address;
    unsigned int depth;                 // 0 for root hub

    // num_configs reflects size of array AND valid configurations
    // configurations that failed to read are skipped (array only contains valid)
    // you need to rely on the configuration index
    uint8_t num_configs;
    struct usb_configuration **configs;
    struct usb_configuration *selected;

    void *hc_priv;

    DLIST_ENTRY(struct usb_device) c_node;
    DLIST_ENTRY(struct usb_device) node;
} usb_device_t;

typedef struct usb_pipe {
    struct usb_device *device;
    struct usb_interface *interface;
    struct usb_endpoint *endp;
    
    uint8_t flags;
    
    mutex_t lock;
    DLIST_HEAD(transfers, struct usb_transfer);

    struct {
        bool running;
        bool aborting;
        bool closing;

        // true if the pipe is flushing its transfer queue
        bool flushing;
        bool flushpend;
    } state;
    
    wait_queue_t waiters;               // Signalled on usb_transferComplete (if syncronous)
    refcount_t ref;
    usb_pipe_ops_t *ops;
    void *hc_priv;                      // For use by host controller only
} usb_pipe_t;


typedef struct usb_transfer {
    usb_pipe_t *pipe;           // Target pipe for this transfer
    void *buffer;               // Buffer in use if needed
    size_t length;
    size_t actual_length;
    usb_device_request_t req;   // (For control transfers) Device request
    usb_status_t status;
    work_t callback;            // Work item, processed by HC workqueue
    bool has_work;
    int timeout;                // Milliseconds timeout (-1 = no timeout)
    unsigned int flags;         // Flags of the transfer
    void *priv;                 // Opaque pointer for usage by requester
    void *hc_priv;              // Opaque pointer for usage by host controller
    DLIST_ENTRY(struct usb_transfer) node;
} usb_transfer_t;

/**** MACROS ****/

#define USB_IS_SUPERSPEED(spd) ((spd) == USB_SPEED_SUPER || (spd) == USB_SPEED_SUPER_PLUS)

#define USB_HOLD_PIPE(pipe) refcount_inc(&(pipe)->ref)
#define USB_RELEASE_PIPE(pipe) if (refcount_dec(&(pipe)->ref) == 0) { usb_freeObject(USB_OBJECT_TYPE_PIPE, (pipe)); }

/**** VARIABLES ****/

extern usb_pipe_ops_t roothub_control_ops;

/**** FUNCTIONS ****/

/**
 * @brief Allocate and zero a specific object
 * @param type The object type to allocate
 * @note This isn't fancy, it's just so that the proper slab pool can be selected
 */
void *usb_allocateObject(usb_object_type_t type);

/**
 * @brief Free a specific object
 * @param type The object type to free
 * @param obj The object
 */
void usb_freeObject(usb_object_type_t type, void *obj);

/**
 * @brief Allocate a new USB controller object
 * @returns A new USB controller object
 */
usb_controller_t *usb_allocateController();

/**
 * @brief USB register a new controller
 * @param controller The controller to register
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_registerController(usb_controller_t *controller);

/**
 * @brief Create a new USB bus
 * @param controller Parental USB controller
 * @param revision Revision of the USB controller
 * @param ops Operations for the bus
 * @param priv Private pointer of the bus
 * @param out Bus output
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_createBus(usb_controller_t *controller, uint8_t revision, usb_bus_ops_t *ops, void *priv, usb_bus_t **out);

/**
 * @brief Initialize a device
 * @param dev The device to initialize
 */
usb_status_t usb_initializeDevice(usb_device_t *dev);

/**
 * @brief Creates and registers a new device for the USB subsystem
 * @param bus The bus the device was attached to
 * @param port The hub port the device is attached to
 * @param speed The speed of the device
 * @param out Output pointer for device
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_createDevice(usb_bus_t *bus, usb_port_t *port, usb_speed_t speed, usb_device_t **out);

/**
 * @brief Remove a USB device (detach)
 * @param device The device to remove
 * 
 * @warning THIS ONLY SUPPORTS DEVICE UNPLUGGING AT THE MOMENT!
 */
void usb_removeDevice(usb_device_t *device);

/**
 * @brief Create a new USB transfer object
 * @param device The device to allocate the transfer from
 * @returns A new transfer object
 */
usb_transfer_t *usb_allocateTransfer(usb_device_t *device);

/**
 * @brief Free transfer
 * @param transfer The transfer to free
 */
void usb_freeTransfer(usb_transfer_t *transfer);

/**
 * @brief Prepare a transfer object
 * @param transfer The transfer to setup
 * @param pipe The pipe for the transfer to use
 * @param buffer The buffer to use
 * @param length The amount of data to transmit
 * @param flags Transfer flags
 * @param callback Transfer callback
 * @param timeout The amount of time to wait (in ms)
 * @param priv Private pointer for the transfer
 */
static inline void usb_prepareTransfer(usb_transfer_t *transfer, usb_pipe_t *pipe, void *buffer, size_t length, unsigned int flags, usb_callback_t callback, int timeout, void *priv) {
    transfer->status = USB_NOT_STARTED;
    transfer->actual_length = 0;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->priv = priv;
    transfer->pipe = pipe;
    
    if (callback) {
        WORK_INIT(&transfer->callback, callback, transfer);
        transfer->has_work = true;   
    } else {
        transfer->has_work = false;
    }

    transfer->timeout = timeout;
    transfer->flags = flags;
}

/**
 * @brief Prepare a control transfer object
 * @param transfer The transfer to setup
 * @param dev The device to transfer to
 * @param req The device request
 * @param buffer The buffer to use
 * @param length The amount of data to transmit
 * @param flags Transfer flags
 * @param callback Transfer callback
 * @param timeout The amount of time to wait (in ms)
 * @param priv Private pointer for the transfer
 */
static inline void usb_prepareControlTransfer(usb_transfer_t *transfer, usb_device_t *device, usb_device_request_t *req, void *buffer, size_t length, unsigned int flags, usb_callback_t callback, int timeout, void *priv) {
    transfer->status = USB_NOT_STARTED;
    transfer->actual_length = 0;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->req = *req;
    transfer->priv = priv;
    transfer->pipe = device->control;
    
    if (callback) {
        WORK_INIT(&transfer->callback, callback, transfer);
        transfer->has_work = true;   
    } else {
        transfer->has_work = false;
    }
    
    transfer->timeout = timeout;
    transfer->flags = flags;
}

/**
 * @brief Perform a transfer on a USB device
 * @param transfer The transfer to execute
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_transfer(usb_transfer_t *transfer);

/**
 * @brief Perform syncronous transfer on a USB device
 * @param transfer The transfer to execute
 * @returns USB_SUCCESS on success
 */
static inline usb_status_t usb_transferSync(usb_transfer_t *transfer) {
    transfer->flags |= USB_TRANSFER_SYNC;
    return usb_transfer(transfer);
}

/**
 * @brief Open a control pipe
 * @param dev The device to open the control pipe for
 * @param pipe_out Output pointer for USB pipe
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_openControlPipe(usb_device_t *dev, usb_pipe_t **pipe_out);

/**
 * @brief Open a pipe on a USB device
 * @param endpoint The endpoint to open he pipe to
 * @param flags The flags to open the pipe with
 * @param pipe_out Output pointer for USB pipe
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_openPipe(usb_endpoint_t *endpoint, uint8_t flags, usb_pipe_t **pipe);

/**
 * @brief Complete a transfer (locked)
 * @param transfer The transfer to complete
 * 
 * Must be called with the pipe's lock held
 */
void usb_transferCompleteLocked(usb_transfer_t *transfer);

/**
 * @brief Complete a transfer
 * @param transfer The transfer to create
 * @param status The transfer status to complete with (since this is called from unlocked context)
 * 
 * @note Status should not be set while not holding the pipe lock.
 */
void usb_transferComplete(usb_transfer_t *transfer, usb_status_t status);

/**
 * @brief Abort a transfer
 * @param transfer The transfer to abort
 */
void usb_abortTransfer(usb_transfer_t *transfer);

/**
 * @brief Abort a pipe
 * @param pipe The pipe to abort
 */
void usb_abortPipe(usb_pipe_t *pipe);

/**
 * @brief Close a pipe
 * @param pipe The pipe to close
 */
void usb_closePipe(usb_pipe_t *pipe);

/**
 * @brief Register a new USB driver
 * @param driver The driver to register
 */
void usb_registerDriver(usb_driver_t *driver);

/**
 * @brief Find and attach driver for new device
 * @param dev The device to attach a driver for
 */
void usb_attachDriver(usb_device_t *device);

/**
 * @brief Perform a control request on a device with flags
 * @param device The device to perform the request on
 * @param req The request to perform
 * @param data Output buffer pointer
 * @param flags The flags to use for the transfer
 * @returns USB_SUCCESS on success
 * 
 * @note Syncronous transfer
 */
usb_status_t usb_requestFlags(usb_device_t *device, usb_device_request_t *req, void *data, uint32_t flags);

/**
 * @brief Perform a control request on a device
 * @param device The device to perform the request on
 * @param req The request to perform
 * @param data Output buffer pointer
 * @returns USB_SUCCESS on success
 * 
 * @note Syncronous transfer
 */
usb_status_t usb_request(usb_device_t *device, usb_device_request_t *req, void *data);

/**
 * @brief Retrieve a descriptor
 * @param device The device to get the descriptor from
 * @param type The type of descriptor to get
 * @param index The index of the descriptor
 * @param desc Output pointer for descriptor
 * @param desc_size Size of descriptor
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getDescriptor(usb_device_t *device, int type, int index, void *desc, size_t desc_size);

/**
 * @brief Retrieve a descriptor with flags
 * @param device The device to get the descriptor from
 * @param type The type of descriptor to get
 * @param index The index of the descriptor
 * @param desc Output pointer for descriptor
 * @param desc_size Size of descriptor
 * @param flags The flags to use
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getDescriptorFlags(usb_device_t *device, int type, int index, void *desc, size_t desc_size, uint32_t flags);

/**
 * @brief Get string from device
 * @param dev The device to get the string from
 * @param index The index of the string
 * @param buf The buffer to store the string in (at least USB_MAX_STRING_LENGTH long)
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_getString(usb_device_t *device, int index, char *buf);

/**
 * @brief Set configuration for device
 * @param dev The device to set the configuration for
 * @param config The config to select
 * @returns USB_SUCCESS on success
 */
usb_status_t usb_setConfiguration(usb_device_t *device, usb_configuration_t *config);

#endif
