/**
 * @file drivers/usb/xhci_new/xhci_ring.c
 * @brief xHCI ring implementation
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
#define LOG(status, ...) dprintf_module(status, "DRIVER:XHCI:RING", __VA_ARGS__)

xhci_ring_t *xhci_createRing() {
    xhci_ring_t *r = kmalloc(sizeof(xhci_ring_t));
    MUTEX_INIT(&r->lock);
    r->trb = (xhci_trb_t*)dma_map(PAGE_SIZE);
    memset(r->trb, 0, PAGE_SIZE);
    r->trb_phys = arch_mmu_physical(NULL, (uintptr_t)r->trb);
    r->enqueue = 0;
    r->dequeue = 0;
    r->cycle = true;
    return r;    
}

xhci_trb_t *xhci_dequeueRing(xhci_ring_t *ring) {
    // this is called from tasklet context, locking mutex is forbidden
    xhci_trb_t *trb = &ring->trb[ring->dequeue];

    if (trb->c != ring->cycle) {
        return NULL;
    }

    ring->dequeue += 1;
    if (ring->dequeue >= XHCI_RING_SIZE) {
        ring->dequeue = 0;
        ring->cycle = !ring->cycle;
    }

    return trb;
}

void xhci_enqueueRing(xhci_ring_t *ring, xhci_trb_t *trb) {
    trb->c = ring->cycle;

    // LOG(DEBUG, "enqueue trb (param=%016llx sts=%08x control=%08x): 0x%016llX\n", trb->parameter, trb->status, trb->control, ring->trb_phys + (ring->enqueue * sizeof(xhci_trb_t)));

    memcpy(&ring->trb[ring->enqueue++], trb, sizeof(xhci_trb_t));

    if (ring->enqueue >= XHCI_RING_SIZE-1) {
        xhci_link_trb_t *link = (xhci_link_trb_t*)(&ring->trb[XHCI_RING_SIZE-1]);
        link->type = XHCI_TRB_TYPE_LINK;
        link->tc = 1;
        link->ring_segment = ring->trb_phys;
        link->interrupter_target = 0;
        link->ch = 0;
        link->ioc = 0;
        link->c = ring->cycle;

        ring->enqueue = 0;
        ring->cycle = !ring->cycle;
    }
}

void xhci_freeRing(xhci_ring_t *ring) {
    dma_unmap((uintptr_t)ring->trb, PAGE_SIZE);
    kfree(ring);
}
