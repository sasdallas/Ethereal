/**
 * @file hexahedron/include/kernel/drivers/virtio.h
 * @brief VirtIO driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_VIRTIO_H
#define DRIVERS_VIRTIO_H

/**** INCLUDES ****/
#include <kernel/drivers/pci.h>
#include <kernel/misc/util.h>
#include <structs/list.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define VIRTIO_STATUS_DEVICE_ACK            0x01
#define VIRTIO_STATUS_DRIVER_LOADED         0x02
#define VIRTIO_STATUS_DRIVER_READY          0x04
#define VIRTIO_STATUS_FEATURES_OK           0x08
#define VIRTIO_STATUS_DRIVER_ERROR          0x40
#define VIRTIO_STATUS_DRIVER_FAILED         0x80

#define VIRTIO_PCI_CAP_COMMON_CFG 1
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
#define VIRTIO_PCI_CAP_ISR_CFG 3
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
#define VIRTIO_PCI_CAP_PCI_CFG 5

#define VIRTIO_F_INDIRECT_DESC              (1ULL << 28)
#define VIRTIO_F_EVENT_IDX                  (1ULL << 29)
#define VIRTIO_F_VERSION_1                  (1ULL << 32)
#define VIRTIO_F_ACCESS_PLATFORM            (1ULL << 33)
#define VIRTIO_F_RING_PACKED                (1ULL << 34)

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

/**** TYPES ****/

typedef struct virtio_common {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
} __attribute__((packed)) virtio_common_t;

typedef struct {
    uint8_t id;
    uint8_t next;
    uint8_t len;
    uint8_t type;
    uint8_t bar;
    uint8_t pad[3];
    uint32_t offset;
    uint32_t length;
} __attribute__((packed)) virtio_cap_t;

struct virtio_driver;
struct virtqueue;

typedef struct virtio_device {
    pci_device_t *dev;
    struct virtio_driver *driver;
    uint64_t features;

    // virtio capabilities
    volatile virtio_common_t *common;
    
    struct {
        volatile uint16_t *notify; 
        uint32_t notify_off_multiplier;
    } notify;
    
    void *device;

    volatile uint8_t *isr;

    list_t vqs;
    void *private;
} virtio_device_t;

typedef struct virtio_driver {
    char *name;
    uint8_t id;

    // wanted features from the device
    // if validate is present, it will be called
    uint64_t features;

    // TODO: more ops
    struct {
        int (*init)(virtio_device_t *dev);
        bool (*validate)(virtio_device_t *dev, uint64_t features);
        void (*device_irq)(virtio_device_t *dev);
    } ops;
} virtio_driver_t;

/* Virtqueue stuff */

typedef struct virtio_buffer {
    uintptr_t paddr;
    size_t len;
    bool write;
} virtio_buffer_t;

typedef struct virtio_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) virtio_desc_t;

typedef struct virtio_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtio_avail_t;

typedef struct virtio_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtio_used_elem_t;

typedef struct virtio_used {
    uint16_t flags;
    uint16_t idx;
    virtio_used_elem_t ring[];
} __attribute__((packed)) virtio_used_t;

typedef void (*virtqueue_callback_t)(struct virtqueue *queue);

typedef struct virtqueue {
    node_t node;
    virtio_device_t *dev;

    uintptr_t mem;
    volatile virtio_desc_t *desc;
    volatile virtio_avail_t *avail;
    volatile virtio_used_t *used;

    uint16_t index;
    uint16_t nfree;
    uint16_t size;
    uint16_t first_free;
    uint16_t last_used;

    volatile uint16_t *notify;

    void **tokens;
    virtqueue_callback_t callback;

    void *priv;
} virtqueue_t;

/**** MACROS ****/

#define VIRTIO_HAS_FEATURE(dev,feat) (((dev)->features & (feat)) != 0)
#define VIRTIO_QUEUE_FREE(q) ((q)->nfree)

/**** FUNCTIONS ****/

/**
 * @brief Register a VirtIO driver and scan for devices
 * @param driver The driver to register
 */
int virtio_register(virtio_driver_t *driver);

/**
 * @brief Allocate virtual queues for a device
 * @param dev The device to allocate queues for
 * @param nqueues The number of queues to allocate
 * @param callbacks An array of callbacks for each queue
 * @returns 0 on success
 */
int virtqueue_allocate(virtio_device_t *device, unsigned int nqueues, virtqueue_callback_t *callbacks);

/**
 * @brief Get virtqueue
 * @param dev The device to get the virtual queue from
 * @param idx Index of virtqueue
 * @returns The virtqueue structure
 */
virtqueue_t *virtqueue_get(virtio_device_t *device, int idx);

/**
 * @brief Push to virtqueue
 * @param vq The virtqueue to push to
 * @param bufs An array of @c virtio_buffer_t structures
 * @param nbufs Number of virtio buffers
 * @param token Token of this transaction
 * @returns 0 on success
 */
int virtqueue_push(virtqueue_t *vq, virtio_buffer_t *bufs, unsigned int nbufs, void *token);

/**
 * @brief Kick a virtqueue
 * @param vq The virtqueue to kick
 */
void virtqueue_kick(virtqueue_t *vq);

/**
 * @brief Pop from the virtqueue
 * @param vq The virtqueue to pop from
 * @param len Output length
 * @returns The token that was pushed
 */
void *virtqueue_pop(virtqueue_t *vq, unsigned int *len);


#endif
