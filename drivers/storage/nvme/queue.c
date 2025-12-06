/**
 * @file drivers/storage/nvme/queue.c
 * @brief NVMe queue system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "nvme.h"
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:NVME", __VA_ARGS__)

/**
 * @brief Create a new NVMe queue
 * @param depth Queue depth
 * @param doorbell NVMe doorbell
 * @returns NVMe queue
 */
nvme_queue_t *nvme_createQueue(size_t depth, nvme_doorbell_t *doorbell) {
    nvme_queue_t *queue = kzalloc(sizeof(nvme_queue_t));
    queue->depth = depth;
    queue->doorbell = doorbell;
    queue->cq_phase = 1;

    // Allocate sq
    queue->sq = dma_map(depth * sizeof(nvme_sq_entry_t));
    memset((void*)queue->sq, 0, depth * sizeof(nvme_sq_entry_t));

    // Allocate cq
    queue->cq = dma_map(depth * sizeof(nvme_cq_entry_t));
    memset((void*)queue->cq, 0, depth * sizeof(nvme_cq_entry_t));

    queue->completions = list_create("nvme queue completions");

    return queue;
}

/**
 * @brief Submit a command to an NVMe queue
 * @param queue The queue to submit the command to
 * @param entry The submission queue entry
 * @returns CID on success
 */
uint16_t nvme_submitQueue(nvme_queue_t *queue, nvme_sq_entry_t *entry) {
    spinlock_acquire(&queue->lock);

    uint16_t cid = queue->cid_last++;
    entry->cid = cid;
    if (queue->cid_last >= sizeof(size_t)*8) queue->cid_last = 0;

    // Get the actual entry
    nvme_sq_entry_t *entry_actual = &(((nvme_sq_entry_t*)(queue->sq))[queue->sq_tail]);
    memcpy(entry_actual, entry, sizeof(nvme_sq_entry_t));

    // Advance sq_idx
    queue->sq_tail = (queue->sq_tail + 1) % queue->depth;
    queue->doorbell->sq_tail = queue->sq_tail;

    spinlock_release(&queue->lock);
    return cid;
}

/**
 * @brief Handle an IRQ in a queue
 * @param queue The queue to handle the IRQ for
 */
void nvme_irqQueue(nvme_queue_t *queue) {
    LOG(DEBUG, "NVME: IRQ detected on queue %p\n", queue);

    // Start processing entries
    nvme_cq_entry_t *cq_list = ((nvme_cq_entry_t*)(queue->cq));
    while ((cq_list[queue->cq_head].sts & 1) == queue->cq_phase) {
        LOG(DEBUG, "CID %04x completed\n", cq_list[queue->cq_head].cid);

        // Send completion event
        nvme_completion_t *completion = kmalloc(sizeof(nvme_completion_t));
        completion->status = cq_list[queue->cq_head].sts >> 1;
        completion->cid = cq_list[queue->cq_head].cid;
        list_append(queue->completions, (void*)completion);

        // TODO: Wakeup CID waiters

        queue->cq_head = (queue->cq_head + 1) % queue->depth;
        if (!queue->cq_head) queue->cq_phase ^= 1;
    }

    queue->doorbell->cq_hdbl = queue->cq_head;
}