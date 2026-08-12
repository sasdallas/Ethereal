/**
 * @file drivers/usb/xhci_new/xhci.c
 * @brief Primary xHCI controller driver
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
#include <kernel/subsystems/irq.h>
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/misc/util.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI", __VA_ARGS__)

/* HACK */
int xhci_controllers = 0;

/* Bus operations */
static usb_status_t xhci_open_pipe(usb_bus_t *bus, usb_pipe_t *pipe);
static void xhci_close_pipe(usb_bus_t *bus, usb_pipe_t *pipe);
static usb_status_t xhci_new_device(usb_bus_t *bus, usb_device_t *device);
static void xhci_remove_device(usb_bus_t *bus, usb_device_t *device);
static usb_status_t xhci_address_device(usb_bus_t *bus, usb_device_t *device);
static usb_status_t xhci_configure_control(usb_bus_t *bus, usb_device_t *device);
usb_bus_ops_t xhci_bus_ops = {
    .open_pipe = xhci_open_pipe,
    .close_pipe = xhci_close_pipe,
    .new_device = xhci_new_device,
    .remove_device = xhci_remove_device,
    .root_hub_control = xhci_root_hub_control,
    .address_device = xhci_address_device,
    .configure_control = xhci_configure_control,
};

/**
 * @brief Perform controller handoff
 */
static int xhci_handoff(xhci_t *xhci) {
    uint16_t ecp = xhci->caps->hccparams1.xECP;
    if (ecp == 0x0) {
        LOG(WARN, "Controller does not support xECP\n");
        return 0;
    }

    uintptr_t ext = xhci->mmio + (ecp * 4);

    while (1) {
        xhci_extended_capability_t *cap = (xhci_extended_capability_t*)ext;
        uint8_t next = XHCI_CAP_NEXT(cap);

        if (XHCI_CAP_ID(cap) == XHCI_EXT_CAP_USBLEGSUP) {
            LOG(DEBUG, "Performing BIOS handoff\n");
            
            volatile xhci_legsup_capability_t *leg = (typeof(leg))cap;
            leg->os_sem = 1;

            if (TIMEOUT((leg->bios_sem == 0), 10000)) {
                LOG(ERR, "BIOS/OS handoff failure (BIOS did not release semaphore)\n");
                return 1;
            }

            LOG(DEBUG, "BIOS handoff finished\n");
            return 0;
        }

        if (!next) break;
        ext += next * 4;
    }

    return 0;
}

/**
 * @brief xHCI reset controller
 */
static int xhci_reset(xhci_t *xhci) {
    if (TIMEOUT(!(xhci->op->usbsts & XHCI_USBSTS_CNR), 10000)) {
        LOG(ERR, "CNR in xHCI controller did not clear\n");
        return 1;
    }

    // Issue software reset
    xhci->op->usbcmd |= XHCI_USBCMD_HCRST;

    if (TIMEOUT(!(xhci->op->usbcmd & XHCI_USBCMD_HCRST), 10000)) {
        LOG(ERR, "HCRST in xHCI controller did not clear\n");
        return 1;
    }

    return 0;
}

/**
 * @brief xHCI init scratchpads
 */
static void xhci_initScratchpads(xhci_t *xhci) {
    uint32_t scratchpads = 0;
    scratchpads |= ((xhci->caps->hcsparams2.max_scratchpad_buffers_hi & 0x1F) << 5);
    scratchpads |= (xhci->caps->hcsparams2.max_scratchpad_buffers_lo) & 0x1F;

    LOG(DEBUG, "Number of scratchpads: %d\n", scratchpads);

    xhci->scratchpad = (uintptr_t*)dma_map(scratchpads * sizeof(uintptr_t));
    
    for (size_t i = 0; i < scratchpads; i++) {
        xhci->scratchpad[i] = pmm_allocatePage(ZONE_DEFAULT);
    }

    xhci->dcbaa[0] = arch_mmu_physical(NULL, (uintptr_t)xhci->scratchpad);
}

/**
 * @brief xHCI explore
 * Explores the xECP and finds port ranges
 */
static void xhci_explore(xhci_t *xhci) {
    uint16_t ecp = xhci->caps->hccparams1.xECP;
    assert(ecp != 0x0);
    uintptr_t ext = xhci->mmio + (ecp * 4);
    while (1) {
        xhci_extended_capability_t *cap = (xhci_extended_capability_t*)ext;
        uint8_t next = XHCI_CAP_NEXT(cap);

        if (XHCI_CAP_ID(cap) == XHCI_EXT_CAP_SUPPORTED) {
            // Supported protocol list
            volatile xhci_supported_prot_capability_t *sup = (xhci_supported_prot_capability_t*)cap;
            
            assert(sup->major == 2 || sup->major == 3);

            if (sup->name_string != 0x20425355) {
                LOG(ERR, "Supported capability ECP has invalid name string %08x\n", sup->name_string);
            } else {
                LOG(INFO, "Detected a xHCI USB %d.%d bus: available port range %d - %d\n", sup->major, sup->minor, sup->compat_port_offset, sup->compat_port_offset+sup->compat_port_count);

                xhci_bus_t *b = kmalloc(sizeof(xhci_bus_t));
                SPINLOCK_INIT(&b->lock);
                b->xhci = xhci;
                b->port_base = sup->compat_port_offset;
                b->port_count = sup->compat_port_count;
                b->port_map = kmalloc(BITMAP_TO_SIZE(sup->compat_port_count+1)); // bit 0 reserved for hub status
                b->port_change_map = kmalloc(BITMAP_TO_SIZE(sup->compat_port_count));
                b->rhub_pending = NULL;

                bitmap_fill(b->port_map, 0, b->port_count);
                bitmap_fill(b->port_change_map, 0, b->port_count);

                uint8_t revision = 0;
                uint16_t revision_raw = ((sup->major & 0xFF) << 8) | (sup->minor & 0xFF);

                switch (revision_raw) {
                    case 0x0301: revision = USB_REVISION_3_1; break;
                    case 0x0300: revision = USB_REVISION_3_0; break;
                    case 0x0200: revision = USB_REVISION_2_0; break;
                    default:
                        LOG(ERR, "Unsupported or invalid port range revision %d.%d\n", sup->major, sup->minor);
                        if (sup->major == 3) {
                            LOG(ERR, "Assuming USB 3.0\n");
                            revision = USB_REVISION_3_0;
                        } else {
                            LOG(ERR, "Assuming USB 2.0\n");
                            revision = USB_REVISION_2_0;
                        }

                        break;
                }

                usb_status_t status = usb_createBus(xhci->controller, revision, &xhci_bus_ops, b, &b->bus);
                if (USB_ERROR(status)) {
                    LOG(ERR, "Failed to create bus: %s\n", usb_strerror(status));
                    kfree(b);
                }
            }
        }

        if (next == 0x0) break;
        ext += next * 4;
    }
}

/**
 * @brief Submit a completion to the xHCI device
 */
static void xhci_submitCompletion(xhci_t *xhci, xhci_completion_t *completion) {
    spinlock_acquire(&xhci->completion_lock);
    queue_obj_push(&xhci->completions, completion);
    spinlock_release(&xhci->completion_lock);

    waitqueue_wakeup(&xhci->completion_waiters, 1);
}

/**
 * @brief Submit port status change event
 */
static void xhci_submitPortChange(xhci_t *xhci, xhci_bus_t *bus, int port_id) {
    // attempt to submit a completion if able to
    bool won = false;
    spinlock_acquire(&bus->lock);
    if (bitmap_test(bus->port_change_map, port_id-1) == false) {
        bitmap_set(bus->port_change_map, port_id-1);
        won = true;
    }

    // this one doesn't require sync
    bitmap_set(bus->port_map, port_id); // Bit 0 of this bitmap is for "hub status"
    spinlock_release(&bus->lock);

    if (won) {
        xhci_completion_t comp = {
            .type = XHCI_COMPLETION_PORT_CHANGE, 
            .port = {
                .id = port_id,
                .bus = bus
            },
        };

        xhci_submitCompletion(xhci, &comp);
    }
}

/**
 * @brief xHCI tasklet
 */
static void xhci_tasklet(void *context) {
    xhci_t *xhci = context;
    while (1) {
        xhci_trb_t *trb = xhci_dequeueRing(xhci->event_ring);
        if (!trb) break;

        if (trb->type == XHCI_EVENT_PORT_STATUS_CHANGE) {
            xhci_port_status_change_trb_t *port_status = (typeof(port_status))trb;
            int id = port_status->port_id;

            // locate the xHC bus this corresponds to
            // !!! this is hacky
            DLIST_FOREACH(usb_bus_t, ubus, &xhci->controller->buses, node) {
                xhci_bus_t *bus = ubus->priv;
                if (id >= bus->port_base && id < bus->port_base+bus->port_count) {
                    xhci_submitPortChange(xhci, bus, id-bus->port_base+1);
                }
            }
        } else if (trb->type == XHCI_EVENT_COMMAND_COMPLETION) {
            xhci_command_completion_trb_t *ctrb = (typeof(ctrb))trb;
            xhci_command_completion_trb_t *ctrbout = NULL;

            spinlock_acquireRaw(&xhci->command_lock);
            int r = queue_rb_pop(&xhci->command_trbs, (void**)&ctrbout);
            spinlock_releaseRaw(&xhci->command_lock);

            if (r != 0) {
                LOG(WARN, "No output command TRB?\n");
                continue;
            }

            memcpy(ctrbout, ctrb, sizeof(xhci_command_completion_trb_t));
            waitqueue_wakeup(&xhci->command_waiters, 1);
        } else if (trb->type == XHCI_EVENT_TRANSFER) {
            xhci_transfer_completion_trb_t *ttrb = (typeof(ttrb))trb;
            // LOG(INFO, "Transfer completed with completion code %d residual length %d slot %d ep %d\n", ttrb->completion_code, ttrb->transfer_len, ttrb->slot_id, ttrb->endpoint_id);

            usb_transfer_t *transfer = NULL;

            xhci_device_t *dev = xhci->devices[ttrb->slot_id];
            xhci_pipe_t *pipe = dev->pipes[ttrb->endpoint_id-1];

            // SEVERE HACK: mutexes cannot be acquired while in tasklet context, therefore
            //              because it is known that a transfer is supposed to be in the queue
            //              and that it was enqueued before the TRBs, and that it is theoretically
            //              safe, we dont lock when popping from the queue.
            //              this is extremely bad, but i dont particularly care.
            //              a malformed xHCI controller that sends a transfer event before any transfers
            //              are ready has worse problems anyways. multiple transfers are still protected by mutex.

            assert(!queue_rb_pop(&pipe->transfers, (void**)&transfer));

            transfer->actual_length = transfer->length - min(ttrb->transfer_len, transfer->length);
            
            if (ttrb->completion_code != 1 && ttrb->completion_code != XHCI_TRB_STATUS_SHORT_PACKET) {
                LOG(ERR, "Transfer failure with completion code %d\n", ttrb->completion_code);
            }

            xhci_completion_t comp = {
                .type = XHCI_COMPLETION_TRANSFER,
                .transfer = transfer,
                .transfer_status = xhci_convertTRBStatus(ttrb->completion_code),
            };

            xhci_submitCompletion(xhci, &comp);
        }
    }

    // Program ERDP
    uint64_t erdp = (xhci->event_ring->trb_phys + (xhci->event_ring->dequeue * sizeof(xhci_trb_t))) | XHCI_ERDP_EHB;
    xhci->runtime->irs[0].erdp = erdp;

    // Keep IE set. Writing zero to IP leaves any newly-pending interrupt set.
    xhci->op->usbsts = XHCI_USBSTS_EINT;
    xhci->runtime->irs[0].iman = XHCI_IMAN_INTERRUPT_ENABLE;
}

/**
 * @brief xHCI IRQ handler
 */
static int xhci_irq(irq_t *irq, void *context) {
    xhci_t *xhci = context;
    xhci->runtime->irs[0].iman = XHCI_IMAN_INTERRUPT_PENDING | XHCI_IMAN_INTERRUPT_ENABLE;

    tasklet_insert(&xhci->tasklet);
    return IRQ_HANDLED;
}

/**
 * @brief Process port change event
 */
static void xhci_processPortChange(xhci_t *xhci, xhci_bus_t *bus, int port) {
    // Dont touch anything, just notify the bus. Root hub driver takes care of the rest.
    spinlock_acquire(&bus->lock);
    bitmap_clear(bus->port_change_map, port-1);
    spinlock_release(&bus->lock);
}

/**
 * @brief xHCI completion handler thread
 * Completion handlers cannot be processed in the tasklet
 */
static void xhci_completionThread(void *arg) {
    xhci_t *xhci = arg;
    for (;;) {
        WAIT_QUEUE_CONDITION(&xhci->completion_waiters, queue_obj_empty(&xhci->completions) == false);

        spinlock_acquire(&xhci->completion_lock);
        xhci_completion_t comp;
        assert(queue_obj_pop(&xhci->completions, &comp) == 0);
        spinlock_release(&xhci->completion_lock);

        if (comp.type == XHCI_COMPLETION_TRANSFER) {
            // LOG(DEBUG, "Detected an XHCI_COMPLETION_TRANSFER event\n");
            usb_transferComplete(comp.transfer, comp.transfer_status);
        } else if (comp.type == XHCI_COMPLETION_PORT_CHANGE) {
            xhci_bus_t *bus = comp.port.bus;
            int port = comp.port.id;
            
            // process the port change event and prepare the port
            xhci_processPortChange(xhci, bus, port);

            // Notify the bus of the new data
            spinlock_acquire(&bus->lock);
            if (bus->rhub_pending) {
                usb_transfer_t *t = bus->rhub_pending;
                bus->rhub_pending = NULL;

                // bit 0 of the bitmap is reserved for hub status
                size_t l = min(((bus->port_count + 8) / 8), t->length);
                memcpy(t->buffer, bus->port_map, l);
                t->actual_length = l;

                // now the bitmap can be cleared
                bitmap_fill(bus->port_map, 0, bus->port_count+1);
                spinlock_release(&bus->lock);

                usb_transferComplete(t, USB_SUCCESS);
            } else {
                spinlock_release(&bus->lock);
            }
        }
    }
}

/**
 * @brief Send and wait for command
 */
int xhci_sendCommand(xhci_t *xhci, void *trb, xhci_command_completion_trb_t *trbout) {
    memset(trbout, 0, sizeof(xhci_command_completion_trb_t));
    
    spinlock_acquireRaw(&xhci->command_lock);
    wait_queue_node_t n;
    waitqueue_add(&xhci->command_waiters, &n);
    xhci_enqueueRing(xhci->cmd_ring, trb);
    queue_rb_push(&xhci->command_trbs, trbout);
    spinlock_releaseRaw(&xhci->command_lock);

    XHCI_DOORBELL(xhci, 0) = 0;

    while (1) {
        if (waitqueue_wait(&xhci->command_waiters, &n, -1) == 0) {
            waitqueue_remove(&xhci->command_waiters, &n);
            break;
        }

        waitqueue_add(&xhci->command_waiters, &n);
    }

    
    return 0;
}

/**
 * @brief Open pipe
 */
static usb_status_t xhci_open_pipe(usb_bus_t *bus, usb_pipe_t *pipe) {
    usb_device_t *dev = pipe->device;
    xhci_device_t *xdev = dev->hc_priv;

    if (dev->depth == 0) {
        // This is for the root hub
        // Depending on whether this device is a control endpoint, use corresponding operations
        if (USB_ENDP_TYPE(pipe->endp->desc.bmAttributes) == USB_ENDP_TYPE_CONTROL) {
            pipe->ops = &roothub_control_ops;
        } else if (USB_ENDP_TYPE(pipe->endp->desc.bmAttributes) == USB_ENDP_TYPE_INT) {
            pipe->ops = &xhci_roothub_intr_ops;
        } else {
            assert(0);
        }
        
        return USB_SUCCESS;
    }

    if (pipe->endp == &dev->control_ep) {
        pipe->ops = &xhci_control_ep_ops;
    } else if (USB_ENDP_TYPE(pipe->endp->desc.bmAttributes) == USB_ENDP_TYPE_INT) {
        pipe->ops = &xhci_intr_ep_ops;
    } else {
        assert(0 && "unhandled");
    }

    return xhci_configurePipe(xdev->xhci, pipe);
}

/**
 * @brief Close pipe
 */
static void xhci_close_pipe(usb_bus_t *bus, usb_pipe_t *pipe) {
    xhci_device_t *xdev = pipe->device->hc_priv;
    xhci_freePipe(xdev->xhci, pipe);
}

/**
 * @brief Get route string for device
 */
static usb_status_t xhci_getRoute(xhci_bus_t *bus, usb_device_t *dev, uint8_t *root_port, uint32_t *route_string) {
    uint32_t route = 0;
    usb_device_t *current = dev;

    while (current->port != NULL) {
        usb_port_t *port = current->port;
        usb_device_t *parent = current->hub->self;

        if (parent == bus->bus->root_hub) {
            *root_port = bus->port_base + port->number - 1;
            *route_string = route;
            return USB_SUCCESS;
        }

        unsigned int tier = current->depth - 2;
        if (tier >= 5) return USB_INVALID;

        route |= min(port->number, 15) << (tier * 4);
        current = parent;
    }

    return USB_INVALID;
}

/**
 * @brief New device
 */
static usb_status_t xhci_new_device(usb_bus_t *ubus, usb_device_t *device) {
    xhci_bus_t *bus = ubus->priv;
    xhci_t *xhci = bus->xhci;

    if (device->depth == 0) {
        // the root hub requires no configuration
        return USB_SUCCESS;
    }

    // Allocate a slot for the device
    xhci_enable_slot_trb_t slot_trb = {
        .type = XHCI_CMD_ENABLE_SLOT
    };
    
    xhci_command_completion_trb_t out;
    xhci_sendCommand(xhci, &slot_trb, &out);
    if (!TRB_SUCCESS(&out)) {
        LOG(ERR, "Failed to create slot for device: completion code 0x%x\n", out.cc);
        return USB_INTERNAL_ERROR;
    }

    // Create the device and the subfields
    xhci_device_t *dev = kzalloc(sizeof(xhci_device_t));
    dev->xhci = bus->xhci;
    dev->slot_id = out.slot_id;
    dev->input_context = (void*)dma_map(PAGE_SIZE);
    dev->device_context = (void*)dma_map(PAGE_SIZE);
    dev->highest_ep = 0;

    xhci->devices[out.slot_id] = dev;
    memset(dev->input_context, 0, PAGE_SIZE);
    memset(dev->device_context, 0, PAGE_SIZE);

    // Set it
    device->hc_priv = dev;

    // Calculate route string and root port
    usb_status_t status = xhci_getRoute(bus, device, &dev->root_port, &dev->route_string);
    if (USB_ERROR(status)) {
        LOG(ERR, "Get route failed: %s\n", usb_strerror(status));
        kfree(dev);
        return status;
    }

    // Apply to DCBAA
    xhci->dcbaa[dev->slot_id] = arch_mmu_physical(NULL, (uintptr_t)dev->device_context);

    LOG(DEBUG, "Allocated slot %d for device\n", dev->slot_id);

    return USB_SUCCESS;
}

/**
 * @brief Remove device
 */
static void xhci_remove_device(usb_bus_t *bus, usb_device_t *device) {
    xhci_device_t *xdev = device->hc_priv;
    xhci_t *xhci = xdev->xhci;

    // Remove the device from the slot array
    xhci->devices[xdev->slot_id] = NULL;

    // Request the xHC to disable slot
    xhci_disable_slot_trb_t disable_slot = {
        .slot_id = xdev->slot_id,
        .type = XHCI_CMD_DISABLE_SLOT
    };

    xhci_command_completion_trb_t ctrb;
    xhci_sendCommand(xhci, &disable_slot, &ctrb);

    if (!TRB_SUCCESS(&ctrb)) {
        LOG(WARN, "DISABLE SLOT failed with completion code: %d (leaking device)\n", ctrb.cc);
        return;
    }

    xhci->dcbaa[xdev->slot_id] = 0x0;
    dma_unmap((uintptr_t)xdev->input_context, PAGE_SIZE);
    dma_unmap((uintptr_t)xdev->device_context, PAGE_SIZE);

    kfree(xdev);
}

/**
 * @brief Address device
 */
static usb_status_t xhci_address_device(usb_bus_t *bus, usb_device_t *device) {
    // BSR=0 is handled in new_device anyways
    return USB_SUCCESS;
}

/**
 * @brief Configure control
 */
static usb_status_t xhci_configure_control(usb_bus_t *bus, usb_device_t *device) {
    if (device->depth == 0) {
        return USB_SUCCESS;
    }

    xhci_device_t *dev = device->hc_priv;
    xhci_t *xhci = dev->xhci;
    xhci_input_context_t *ic = XHCI_INPUT_CONTEXT(dev);
    xhci_endpoint_context_t *ec = XHCI_ENDPOINT_CONTEXT(dev, 1);
    
    memset(ic, 0, XHCI_CONTEXT_SIZE(xhci));
    ic->add_flags = (1 << 1);
    ic->drop_flags = 0;
    ec->max_packet_size = device->control_ep.mps;

    xhci_evaluate_context_trb_t eval = {
        .type = XHCI_CMD_EVALUATE_CONTEXT,
        .bsr = 0,
        .input_context = arch_mmu_physical(NULL, (uintptr_t)dev->input_context),
        .slot_id = dev->slot_id,
        .rsvd0 = 0,
        .rsvd1 = 0,
        .rsvd2 = 0
    };

    xhci_command_completion_trb_t ctrb;
    xhci_sendCommand(xhci, &eval, &ctrb);

    if (!TRB_SUCCESS(&ctrb)) {
        LOG(ERR, "Evaluate context failed with completion code %d\n", ctrb.cc);
        return USB_INTERNAL_ERROR;
    }

    return USB_SUCCESS;
}

/**
 * @brief Probe ports
 * Hack for QEMU (stupid)
 */
static void xhci_probe(xhci_t *xhci) {
    usb_controller_t *hc = xhci->controller;

    DLIST_FOREACH(usb_bus_t, ubus, &hc->buses, node) {
        xhci_bus_t *bus = ubus->priv;
        
        for (int i = 0; i < bus->port_count; i++) {
            volatile xhci_port_regs_t *regs = XHCI_PORTREGS(xhci, bus->port_base + i - 1);
            if ((regs->portsc & XHCI_PORTSC_CCS) == 0) {
                continue;
            }

            LOG(DEBUG, "Connection on logical port %d physical port %d\n", i, i + bus->port_base);
        
            // i+1 as while the port registers are 0 based, the changes arent
            xhci_submitPortChange(xhci, bus, i+1);
        }
    }
}

/**
 * @brief xHCI initialize
 */
static int xhci_init(pci_device_t *dev) {
    xhci_controllers += 1;

    uint16_t cmd;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &cmd);
    cmd &= ~(PCI_COMMAND_IO_SPACE);
    cmd |= (PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, cmd);

    // Prepare the BAR
    pci_bar_t *bar = pci_getBAR(dev, 0);
    assert(bar && PCI_BAR_IS_MEMORY(bar));

    // Create the xHCI device
    xhci_t *xhci = kzalloc(sizeof(xhci_t));
    memset(xhci, 0, sizeof(xhci_t));
    SPINLOCK_INIT(&xhci->command_lock);
    SPINLOCK_INIT(&xhci->completion_lock);
    TASKLET_INIT(&xhci->tasklet, "xhci tasklet", xhci_tasklet, xhci);
    QUEUE_OBJ_INIT(&xhci->completions, sizeof(xhci_completion_t), XHCI_MAX_COMPLETIONS);
    QUEUE_RB_INIT(&xhci->command_trbs, 5);
    WAIT_QUEUE_INIT(&xhci->completion_waiters);
    WAIT_QUEUE_INIT(&xhci->command_waiters);

    xhci->pci = dev;
    xhci->mmio = bar->mapped;
    xhci->caps = (volatile xhci_cap_regs_t*)(xhci->mmio);
    xhci->op = (volatile xhci_op_regs_t*)(xhci->mmio + xhci->caps->caplength);
    xhci->runtime = (volatile xhci_runtime_regs_t*)(xhci->mmio + xhci->caps->rtsoff);

    LOG(INFO, "Detected xHCI controller version %d.%d.%d\n", 
        xhci->caps->revision_major+1,
        xhci->caps->revision_minor >> 4,
        xhci->caps->revision_minor & 0x0F
    );

    uint32_t max_ports = xhci->caps->hcsparams1.max_ports;
    uint32_t max_slots = xhci->caps->hcsparams1.max_slots;
    uint32_t max_interrupters = xhci->caps->hcsparams1.max_interrupters;

    LOG(DEBUG, "Maximum ports: %d maximum slots: %d maximum interruptors: %d\n", max_ports, max_slots, max_interrupters);

    // Do the BIOS handoff
    if (xhci_handoff(xhci)) {
        LOG(ERR, "xHCI handoff failure\n");
        goto _error;
    }

    // Reset
    if (xhci_reset(xhci)) {
        LOG(ERR, "xHCI reset controller failure\n");
        goto _error;
    }

    // Page size check
    if ((xhci->op->pagesize & (1 << 0)) == 0) {
        LOG(ERR, "xHCI controller does not support 4096-byte pages\n");
        goto _error;
    }

    // TODO
    if (xhci->caps->hccparams1.ac64 == 0) {
        LOG(ERR, "xHCI controller does not support 64-bit addressing\n");
        goto _error;
    }

    // Create the DCBAA
    xhci->dcbaa = (uintptr_t*)dma_map(max_slots * 8);
    memset((void*)xhci->dcbaa, 0, max_slots * 8);
    xhci->op->dcbaap = arch_mmu_physical(NULL, (uintptr_t)xhci->dcbaa);

    // Prepare scratchpads
    xhci_initScratchpads(xhci);

    // Prepare interrupt
    int r = pci_allocateInterrupts(dev, 1, 1, PCI_IRQ_ALL);
    if (r != 1) {
        LOG(ERR, "Failed to allocate interrupts for xHCI controller (code %d)\n", r);
        goto _error;
    }

    pci_irq_t *irq = pci_getInterruptVector(dev, 0);
    irq_register(irq->vector, xhci_irq, IRQ_FLAG_SHARED, xhci, NULL);

    // Ring setup
    xhci->cmd_ring = xhci_createRing();
    xhci->event_ring = xhci_createRing();

    // Add a link TRB to the command ring
    xhci_link_trb_t *link = (xhci_link_trb_t*)&xhci->cmd_ring->trb[XHCI_RING_SIZE-1];
    link->ring_segment = xhci->cmd_ring->trb_phys;
    link->type = XHCI_TRB_TYPE_LINK;
    link->interrupter_target = 0;
    link->c = 1;
    link->tc = 1;
    link->ch = 0;
    link->ioc = 0;

    // Program CRCR
    xhci->op->crcr_lo = ((uint32_t)xhci->cmd_ring->trb_phys & 0xFFFFFFFF) | xhci->cmd_ring->cycle;
    xhci->op->crcr_hi = (uint32_t)(xhci->cmd_ring->trb_phys >> 32);

    // Prepare event ring segment
    // TODO: not waste page on alloc segment
    xhci->erst = dma_map(PAGE_SIZE);
    xhci_event_ring_entry_t *erst = (xhci_event_ring_entry_t*)xhci->erst;
    erst->rsba = xhci->event_ring->trb_phys;
    erst->rsz = XHCI_RING_SIZE;
    erst->rsvd0 = 0;

    // Program the interrupter
    xhci->runtime->irs[0].erstsz = 1;
    xhci->runtime->irs[0].erdp = xhci->event_ring->trb_phys | (1 << 3);
    xhci->runtime->irs[0].erstba = arch_mmu_physical(NULL, xhci->erst);

    LOG(DEBUG, "Rings initialized\n");
    
    // Program max enabled slots
    uint32_t config = xhci->op->config;
    config &= 0xFF;
    config |= max_slots;
    xhci->op->config =  config;

    // Before the controller is started up, it must be explored
    xhci->controller = usb_allocateController();
    xhci->controller->priv = (void*)xhci;
    usb_registerController(xhci->controller);
    xhci_explore(xhci);

    // Enable interrupts and fire up the controller
    xhci->op->usbcmd |= XHCI_USBCMD_INTE;
    xhci->runtime->irs[0].iman |= XHCI_IMAN_INTERRUPT_ENABLE;
    xhci->op->usbcmd |= XHCI_USBCMD_RS;
    LOG(DEBUG, "Starting xHCI controller.\n");
    while (xhci->op->usbsts & XHCI_USBSTS_HCH) arch_pause();
    LOG(DEBUG, "xHCI controller is running\n");

    // Spawn thread
    char procname[50];
    snprintf(procname, 50, "xhci%d", xhci_controllers-1);
    process_t *xhci_process = process_createKernel(procname, 0, xhci_completionThread, xhci);
    sched_insert(xhci_process->main_thread);

    // QEMU woes:   The xHCI driver incorrectly does not send Port Status Change TRBs when it starts after halting
    //              To fix, the ports must be probed at runtime
    xhci_probe(xhci);
    
    return 0;

_error:
    if (xhci->dcbaa) dma_unmap((uintptr_t)xhci->dcbaa, PAGE_SIZE);
    if (xhci->erst) dma_unmap(xhci->erst, PAGE_SIZE);
    
    // TODO
    // if (xhci->scratchpad)

    kfree(xhci);
    return 1;
}

/**
 * @brief xHCI scanner
 */
static int xhci_scan(pci_device_t *dev, void *context) {
    uint8_t progif;
    if (pci_readConfigByte(dev, PCI_PROGIF_OFFSET, &progif) == 0) {
        if (progif == 0x30) {
            // Valid xHCI controller detected
            return xhci_init(dev);
        }
    }

    return 0;
}

/**
 * @brief Driver init
 */
static int driver_init(int argc, char *argv[]) {
    // TODO: DRIVER_STATUS_NO_DEVICE

    pci_scan_parameters_t params = {
        .class_code = 0x0C,
        .subclass_code = 0x03,
        .id_list = NULL
    };

    if (pci_scanDevice(xhci_scan, &params, NULL)) {
        return DRIVER_STATUS_ERROR;
    }

    if (xhci_controllers > 0) {
        return DRIVER_STATUS_SUCCESS;
    } else {
        return DRIVER_STATUS_NO_DEVICE;
    }
}

/**
 * @brief Driver deinit
 */
static int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "xHCI driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
