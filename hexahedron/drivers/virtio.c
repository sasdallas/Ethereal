/**
 * @file hexahedron/drivers/virtio.c
 * @brief VirtIO driver layer
 * 
 * Provides some common accesses to VirtIO devices
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/virtio.h>
#include <kernel/subsystems/irq.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "VIRTIO", __VA_ARGS__)

/**
 * @brief VirtIO process device
 */
int virtio_scanDevice(pci_device_t *dev, void *context) {
    virtio_driver_t *drv = (virtio_driver_t*)context;
    LOG(INFO, "VirtIO is processing device %04x\n", dev->devid);

    // Enable bus mastering and memory space
    uint16_t cmd;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &cmd);
    cmd |= (1 << 2) | (1 << 1);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, cmd);

    // Allocate a new device
    virtio_device_t *viodev = kzalloc(sizeof(virtio_device_t));
    viodev->dev = dev;
    viodev->driver = drv;
    LIST_INIT(&viodev->vqs);

    // We need to process the capability list
    uint8_t cap_offset = 0;
    while (1) {
        uint8_t caps = pci_getNextCapability(dev, &cap_offset);
    
        if (caps == 0) {
            break;
        }

        if (caps != PCI_CAP_VENDOR_SPECIFIC) {
            continue;
        }

        virtio_cap_t cap;
        pci_readStructure(dev, cap_offset, &cap, sizeof(virtio_cap_t));
        
        pci_bar_t *bar = pci_getBAR(dev, cap.bar);
        assert(bar);

        if (cap.type == VIRTIO_PCI_CAP_COMMON_CFG) {
            LOG(DEBUG, "Found common VirtIO config at 0x%x\n", cap_offset);
            viodev->common = (volatile virtio_common_t*)(bar->mapped + cap.offset);
        } else if (cap.type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
            LOG(DEBUG, "Found notify VirtIO config at 0x%x\n", cap_offset);
            viodev->notify.notify = (volatile uint16_t*)(bar->mapped + cap.offset);
            pci_readConfigDword(dev, cap_offset+sizeof(virtio_cap_t), &viodev->notify.notify_off_multiplier);
        } else if (cap.type == VIRTIO_PCI_CAP_DEVICE_CFG) {
            LOG(DEBUG, "Found device VirtIO config at 0x%x\n", cap_offset);
            viodev->device = (void*)(bar->mapped + cap.offset);
        } else if (cap.type == VIRTIO_PCI_CAP_ISR_CFG) {
            LOG(DEBUG, "Found ISR VirtIO config at 0x%x\n", cap_offset);
            viodev->isr = (volatile uint8_t*)(bar->mapped + cap.offset);
        }
    }

    assert(viodev->common && viodev->notify.notify);

    // section 3.1.1
    viodev->common->device_status = 0;
    viodev->common->device_status |= VIRTIO_STATUS_DEVICE_ACK | VIRTIO_STATUS_DRIVER_LOADED;

    // Read device features
    uint64_t features = 0;
    viodev->common->device_feature_select = 0;
    features |= ((uint64_t)viodev->common->device_feature);
    viodev->common->device_feature_select = 1;
    features |= ((uint64_t)viodev->common->device_feature << 32);

    LOG(DEBUG, "Features supported: %016llx\n", features);

    if (!(features & VIRTIO_F_VERSION_1)) {
        assert(0 && "modern virtio device doesnt support modern protocol");
    }

    // Negotiate
    features &= drv->features;
    features |= VIRTIO_F_VERSION_1;
    viodev->features = features;

    // Try to validate if driver wants
    if (drv->ops.validate) {
        if (drv->ops.validate(viodev, features) == false) {
            // TODO maybe unmap PCI MMIO?
            LOG(ERR, "Driver cannot accept feature bitmask\n");
            kfree(viodev);
            return 0; // non-fatal
        }
    }

    // Good
    viodev->common->driver_feature_select = 0;
    viodev->common->driver_feature = (features & 0xFFFFFFFF);
    viodev->common->driver_feature_select = 1;
    viodev->common->driver_feature = ((features >> 32) & 0xFFFFFFFF);
    
    // Negotiation complete
    viodev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;

    if ((viodev->common->device_status & VIRTIO_STATUS_FEATURES_OK) == 0) {
        // TODO maybe unmap PCI MMIO?
        LOG(ERR, "Device rejected features\n");
        kfree(viodev);
        return 1; // ts is fucked
    }

    viodev->common->device_status |= VIRTIO_STATUS_DRIVER_READY;

    int r = viodev->driver->ops.init(viodev);
    if (r != 0) {
        LOG(WARN, "VirtIO driver initialization failed (%d)\n", r);
        viodev->common->device_status |= VIRTIO_STATUS_DRIVER_ERROR;
        return 1;
    }

    // device initialized ok
    return 0;
}

/**
 * @brief Register a VirtIO driver and scan for devices
 * @param driver The driver to register
 */
int virtio_register(virtio_driver_t *driver) {
    pci_id_mapping_t id_list[] = {
        { .vid = 0x1AF4, .devid = { 0x1040 + driver->id, PCI_NONE } },
        PCI_ID_MAPPING_END
    };

    pci_scan_parameters_t p = {
        .class_code = 0,
        .subclass_code = 0,
        .id_list = id_list
    };

    return pci_scanDevice(virtio_scanDevice, &p, (void*)driver);
}

/**
 * @brief virtqueue shared IRQ routine
 */
static int virtqueue_irqShared(irq_t *irq, void *context) {
    // TODO
    LOG(ERR, "irqShared is todo\n");
    return IRQ_ERROR;
}

/**
 * @brief virtqueue non-shared IRQ routine
 */
static int virtqueue_irqDevice(irq_t *irq, void *context) {
    virtio_device_t *dev = context;

    if (dev->driver->ops.device_irq) {
        dev->driver->ops.device_irq(dev);
    }

    return IRQ_HANDLED;
}

/**
 * @brief virtqueue non-shared IRQ routine
 */
static int virtqueue_irq(irq_t *irq, void *context) {
    virtqueue_t *vq = (virtqueue_t*)context;
    vq->callback(vq);
    return IRQ_HANDLED;
}

/**
 * @brief Setup specific virtqueue
 * @param dev The device
 * @param idx The index of the virtqueue
 * @param callback Callback for this virtqueue
 * @param irq IRQ vector for this virtqueue, or NULL if shared
 * @returns 0 on success
 */
static int virtqueue_setup(virtio_device_t *dev, unsigned int idx, virtqueue_callback_t cb, pci_irq_t *irq, bool shared) {
    dev->common->queue_select = idx;
    BARRIER();

    uint16_t q_size = dev->common->queue_size;
    assert(q_size != 0);

    // allocate a virtqueue
    virtqueue_t *vq = kmalloc(sizeof(virtqueue_t)); // todo slab this
    vq->dev = dev;
    vq->index = idx;
    vq->size = q_size;
    vq->nfree = q_size;
    vq->first_free = 0;
    vq->last_used = 0;
    vq->callback = cb;

    size_t desc_sz  = q_size * sizeof(virtio_desc_t);
    size_t avail_sz = sizeof(uint16_t) * 3 + (q_size * sizeof(uint16_t));
    size_t used_sz  = sizeof(uint16_t) * 3 + (q_size * sizeof(virtio_used_elem_t));
    size_t qbytes = desc_sz + avail_sz + used_sz;

    // map
    vq->mem = dma_map(qbytes);
    memset((void*)vq->mem, 0, PAGE_ALIGN_UP(qbytes));

    vq->desc = (volatile virtio_desc_t*)(vq->mem);
    vq->avail = (volatile virtio_avail_t*)ALIGN_UP((uintptr_t)vq->desc + desc_sz, 2);
    vq->used = (volatile virtio_used_t*)ALIGN_UP((uintptr_t)vq->avail + avail_sz, 4);

    // allocate tokens list
    vq->tokens = kzalloc(q_size * sizeof(void*));

    // build desc list
    for (uint16_t i = 0; i < q_size-1; i++) {
        vq->desc[i].next = i+1;
    }

    vq->desc[q_size-1].next = 0xFFFF;

    // compute notify address
    vq->notify = (volatile uint16_t*)((uintptr_t)dev->notify.notify + (idx * dev->notify.notify_off_multiplier));

    // write
    dev->common->queue_desc = arch_mmu_physical(NULL, (uintptr_t)vq->desc);
    dev->common->queue_driver = arch_mmu_physical(NULL, (uintptr_t)vq->avail);
    dev->common->queue_device = arch_mmu_physical(NULL, (uintptr_t)vq->used);
    if (shared ){
        dev->common->queue_msix_vector = (irq->type == PCI_IRQ_MSI_X) ? 0 : 0xFFFF;
    } else {
        dev->common->queue_msix_vector = (irq->type == PCI_IRQ_MSI_X) ? idx+1 : 0xFFFF;
    }

    dev->common->queue_enable = 1;

    if (irq) {
        irq_register(irq->vector, virtqueue_irq, IRQ_FLAG_DEFAULT, vq, NULL);
    }

    NODE_INIT(&vq->node, vq);
    list_append_node(&dev->vqs, &vq->node);
    return 0;
}


/**
 * @brief Allocate virtual queues for a device
 * @param dev The device to allocate queues for
 * @param nqueues The number of queues to allocate
 * @param callbacks An array of callbacks for each queue
 * @returns 0 on success
 */
int virtqueue_allocate(virtio_device_t *device, unsigned int nqueues, virtqueue_callback_t *callbacks) {
    // Allocate IRQs first
    // Two choices: Either we get enough IRQs such that each queue can have one, or we dont.
    // If we get the former we will only use the first IRQ and move everything to a virtqueue_irqShared()
    int got = pci_allocateInterrupts(device->dev, 1, nqueues+1, PCI_IRQ_ALL);
    assert(got > 0);

    bool single_handler = ((size_t)got != nqueues+1);

    // todo can make virtqueue_irqDevice return IRQ_NOT_SOURCE for some chained
    pci_irq_t *d = pci_getInterruptVector(device->dev, 0);
    if (single_handler) {
        irq_register(d->vector, virtqueue_irqShared, IRQ_FLAG_DEFAULT, (void*)device, NULL);
    } else {
        irq_register(d->vector, virtqueue_irqDevice, IRQ_FLAG_DEFAULT, (void*)device, NULL);
    }

    // Use interrupt 0
    device->common->msix_config = 0;
    BARRIER();
    assert(device->common->msix_config != 0xFFFF);

    if (single_handler) {
        // TODO Free others
        LOG(WARN, "Not enough IRQs for each queue (got %d, needed %d)\n", got, nqueues);
        LOG(WARN, "VirtIO will use shared dispatcher.\n");
    }

    for (unsigned i = 0; i < nqueues; i++) {
        pci_irq_t *irq = single_handler ? d : pci_getInterruptVector(device->dev, i+1);
        int r = virtqueue_setup(device, i, callbacks[i], irq, single_handler);
        if (r != 0) {
            return r;
        }
    }

    return 0;
}

/**
 * @brief Get virtqueue
 * @param dev The device to get the virtual queue from
 * @param idx Index of virtqueue
 * @returns The virtqueue structure
 */
virtqueue_t *virtqueue_get(virtio_device_t *device, int idx) {
    foreach(vqnode, &device->vqs) {
        if (!idx--) {
            return (virtqueue_t*)vqnode->value;
        }
    }

    assert(0);
}

/**
 * @brief Push to virtqueue
 * @param vq The virtqueue to push to
 * @param bufs An array of @c virtio_buffer_t structures
 * @param nbufs Number of virtio buffers
 * @param token Token of this transaction
 * @returns 0 on success
 */
int virtqueue_push(virtqueue_t *vq, virtio_buffer_t *bufs, unsigned int nbufs, void *token) {
    if (vq->nfree < nbufs || !nbufs) {
        return -ENOSPC;
    }

    uint16_t head = vq->first_free;
    uint16_t idx = head;
    uint16_t last_idx = head;

    for (unsigned i = 0; i < nbufs; i++) {
        last_idx = idx;
        
        vq->desc[idx].addr = bufs[i].paddr;
        vq->desc[idx].len = bufs[i].len;
        vq->desc[idx].flags = (bufs[i].write) ? VRING_DESC_F_WRITE : 0;

        if (i < (nbufs-1)) {
            vq->desc[idx].flags |= VRING_DESC_F_NEXT;
        }

        idx = vq->desc[idx].next;
    }

    // finalize chain + update statistics
    vq->desc[last_idx].flags &= ~VRING_DESC_F_NEXT;
    vq->first_free = idx;
    vq->nfree -= nbufs;
    vq->tokens[head] = token;

    // publish
    uint16_t avail = vq->avail->idx % vq->size;
    vq->avail->ring[avail] = head;

    BARRIER();
    vq->avail->idx++;

    return 0;
}

/**
 * @brief Kick a virtqueue
 * @param vq The virtqueue to kick
 */
void virtqueue_kick(virtqueue_t *vq) {
    BARRIER();
    *(vq->notify) = vq->index;
}

/**
 * @brief Pop from the virtqueue
 * @param vq The virtqueue to pop from
 * @param len Output length
 * @returns The token that was pushed
 */
void *virtqueue_pop(virtqueue_t *vq, unsigned int *len) {
    uint16_t last_used = vq->last_used;
    
    // check if there's anything to pop
    BARRIER();
    if (last_used == vq->used->idx) {
        return NULL;
    }

    uint16_t used_idx = last_used % vq->size;
    uint16_t head = (uint16_t)vq->used->ring[used_idx].id;
    if (len) *len = vq->used->ring[used_idx].len;

    void *t = vq->tokens[head];
    vq->tokens[head] = NULL;

    // return these to free list
    uint16_t idx = head;
    while (1) {
        vq->nfree++;

        if (!(vq->desc[idx].flags & VRING_DESC_F_NEXT)) {
            vq->desc[idx].next = vq->first_free;
            break;
        }

        idx = vq->desc[idx].next;
    }

    vq->first_free = head;
    vq->last_used++;

    return t;
}
