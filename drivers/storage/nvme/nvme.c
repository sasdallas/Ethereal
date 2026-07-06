/**
 * @file drivers/storage/nvme/nvme.c
 * @brief NVMe storage driver
 * 
 * This is revision 2.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "nvme.h"
#include <kernel/drivers/storage/drive.h>
#include <kernel/subsystems/irq.h>
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/task/process.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:NVME", __VA_ARGS__)

/* "Borrowed" from Astral's kernel-src/io/block/nvme.c */
#define GET_IO_QUEUE(nvme) (__atomic_add_fetch(&nvme->last_queue, 1, __ATOMIC_SEQ_CST) % (nvme)->nqueues)

/* NVMe ops */
static ssize_t nvme_read(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer);
static ssize_t nvme_write(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer);
static ssize_t nvme_read_range(drive_t *d, page_range_t *r);
static ssize_t nvme_write_range(drive_t *d, page_range_t *r);

drive_ops_t nvme_ops = {
    .read_sectors = nvme_read,
    .write_sectors = nvme_write,
    .flush = NULL,
    .ioctl = NULL,
    .read_range = nvme_read_range,
    .write_range = nvme_write_range,
};

/**
 * @brief NVMe tasklet
 */
void nvme_tasklet(void *context) {
    nvme_queue_t *queue = (nvme_queue_t*)context;

    spinlock_acquire(&queue->lock);

    nvme_cq_entry_t *cq = (nvme_cq_entry_t*)queue->cq.addr;
    int xfers = 0;
    while ((cq[queue->cq.index].sts & 1) == queue->cq.phase) {
        uint16_t cid = cq[queue->cq.index].cid;
        int sts = cq[queue->cq.index].sts >> 1;

        nvme_transfer_t *xfer = queue->entries[cid];
        xfer->status = sts;
        xfer->dw0 = cq[queue->cq.index].useless[0];
        if (xfer->thread != NULL) sleep_wakeup(xfer->thread);

        queue->entries[cid] = NULL;
        bitmap_clear(queue->ent_bitmap, cid);
        waitqueue_wakeup(&queue->ent_wait, 1);

        xfers++;

        // advance phase
        queue->cq.index = (queue->cq.index + 1) % queue->cq.size;
        if (!queue->cq.index) queue->cq.phase ^= 1;
    }

    if (xfers) {
        *queue->cq.doorbell = queue->cq.index;
    }

    spinlock_release(&queue->lock);
}

/**
 * @brief IRQ handler
 */
int nvme_irq(irq_t *irq, void *context) {
    nvme_queue_t *queue = context;
    
    // TODO: IRQ_NOT_SOURCE
    tasklet_insert(&queue->tsklet);
    return IRQ_HANDLED;
}

/**
 * @brief Create queue
 */
nvme_queue_t *nvme_createQueue(uint32_t *sqdbl, uint32_t *cqdbl) {
    nvme_queue_t *q = kmalloc(sizeof(nvme_queue_t));
    SPINLOCK_INIT(&q->lock);
    TASKLET_INIT(&q->tsklet, "NVMe", nvme_tasklet, (void*)q);
    WAIT_QUEUE_INIT(&q->ent_wait);
    bitmap_fill(q->ent_bitmap, 0, NVME_QUEUE_DEPTH);
    q->pending = 0;
    
    // Prime submission queue
    q->sq.addr = dma_map(NVME_QUEUE_DEPTH * sizeof(nvme_sq_entry_t));
    memset((void*)q->sq.addr, 0, NVME_QUEUE_DEPTH * sizeof(nvme_sq_entry_t));
    q->sq.index = 0;
    q->sq.phase = 0;
    q->sq.size = NVME_QUEUE_DEPTH;
    q->sq.doorbell = sqdbl;

    // Prime completion queue
    q->cq.addr = dma_map(NVME_QUEUE_DEPTH * sizeof(nvme_cq_entry_t));
    memset((void*)q->cq.addr, 0, NVME_QUEUE_DEPTH * sizeof(nvme_cq_entry_t));
    q->cq.index = 0;
    q->cq.phase = 1;
    q->cq.size = NVME_QUEUE_DEPTH;
    q->cq.doorbell = cqdbl;

    return q;
}

/**
 * @brief Submit transfer and wait
 */
int nvme_submitWait(nvme_queue_t *queue, nvme_transfer_t *transfer) {
    spinlock_acquire(&queue->lock);
    int f = -1;
    while (1) {
        f = bitmap_find_first(queue->ent_bitmap, NVME_QUEUE_DEPTH);
        if (f == -1) {
            // out of entries
            wait_queue_node_t n;
            waitqueue_add(&queue->ent_wait, &n);
            spinlock_release(&queue->lock);
            int w = waitqueue_wait(&queue->ent_wait, &n, -1);
            waitqueue_remove(&queue->ent_wait, &n);

            if (w != 0) {
                LOG(ERR, "waitqueue_wait returned %d\n", w);
                return w;
            }

            // retry
            spinlock_acquire(&queue->lock);
            continue;
        } else {
            break;
        }
    }

    bitmap_set(queue->ent_bitmap, f);
    queue->entries[f] = transfer;

    transfer->thread = current_cpu->current_thread;
    transfer->ent.cid = f;
    transfer->status = 0xFFFF;

    // submit
    nvme_sq_entry_t *sq = (nvme_sq_entry_t*)queue->sq.addr;
    sq[queue->sq.index] = transfer->ent;
    queue->sq.index = (queue->sq.index + 1) % queue->sq.size;
    
    asm volatile ("" ::: "memory");
    *queue->sq.doorbell = queue->sq.index;

    if (transfer->thread) sleep_prepareUninterruptible();
    spinlock_release(&queue->lock);
    if (transfer->thread) {
        sleep_enter();
        return (transfer->status == NVME_STATUS_SUCCESS) ? 0 : -EIO;
    } else {
        while (transfer->status == 0xFFFF) {
            asm volatile (  "sti\n"
                            "hlt\n");
        }
        
        return 0;
    }
}

/**
 * @brief Identify NVMe controller
 */
int nvme_identify(nvme_t *nvme, nvme_ident_t *id_out) {
    uintptr_t id_page = dma_map(PAGE_SIZE);
    memset((void*)id_page, 0, PAGE_SIZE);

    nvme_transfer_t xfer = {
        .ent = { .opc = NVME_OPC_IDENTIFY },
    };

    nvme_identify_command_t *cmd = (nvme_identify_command_t*)&xfer.ent.command;
    cmd->dptr.prp1 = arch_mmu_physical(NULL, id_page);
    cmd->cns = NVME_CNS_IDENTIFY_CONTROLLER;

    int r = nvme_submitWait(nvme->admin, &xfer);
    if (r != 0) {
        dma_unmap(id_page, PAGE_SIZE);
        return r;
    }
    
    if (xfer.status != NVME_STATUS_SUCCESS) {
        LOG(ERR, "Identify command failed with status %d\n", xfer.status);
        dma_unmap(id_page, PAGE_SIZE);
        return 1;
    }

    memcpy(id_out, (void*)id_page, sizeof(nvme_ident_t));
    dma_unmap(id_page, PAGE_SIZE);
    return r;
}

/**
 * @brief Allocate I/O queues with controller
 * @param nvme controller
 * @param queues The queues to allocate
 */
int nvme_allocateQueues(nvme_t *nvme, int nqueues) {
    nvme_transfer_t xfer = {
        .ent = {
            .opc = NVME_OPC_SET_FEATURES,
        }
    };

    nvme_set_features_command_t *cmd = (nvme_set_features_command_t*) &xfer.ent.command;
    cmd->fid = 0x07;
    
    int queues = nqueues - 1;
    cmd->cdw11 = ((queues << 16) & 0xFFFF0000) | ((queues) & 0x0000FFFF);

    int r = nvme_submitWait(nvme->admin, &xfer);
    if (r != 0) {
        return r;
    }
    
    if (xfer.status != NVME_STATUS_SUCCESS) {
        LOG(ERR, "Set features command failed with status %d\n", xfer.status);
        return -EIO;
    }

    int cqs = ((xfer.dw0 >> 16) & 0xffff) + 1;
    int sqs = ((xfer.dw0) & 0xffff) + 1;

    // QEMU responds with more than nqueues - I don't know if that's allowed but    
    return min(min(cqs,sqs), nqueues);
}

/**
 * @brief create I/O queue
 */
nvme_queue_t *nvme_createIOQueue(nvme_t *nvme, int id, irq_number_t irqvect) {
    nvme_queue_t *queue = nvme_createQueue(NVME_GET_DOORBELL_SQ(id), NVME_GET_DOORBELL_CQ(id));

    nvme_transfer_t create_cq_xfer = {
        .ent = { .opc = NVME_OPC_CREATE_CQ }
    };

    nvme_create_cq_command_t cq = {
        .dptr.prp1 = arch_mmu_physical(NULL, queue->cq.addr),
        .qsize = NVME_QUEUE_DEPTH-1,
        .qid = id,
        .iv = id,
        .ien = 1,
        .pc = 1
    };

    *((nvme_create_cq_command_t*)&create_cq_xfer.ent.command) = cq;

    if (nvme_submitWait(nvme->admin, &create_cq_xfer)) {
        // TODO leak
        LOG(ERR, "NVME_OPC_CREATE_CQ failure.\n");
        return NULL;
    }

    nvme_transfer_t create_sq_xfer = {
        .ent = { .opc = NVME_OPC_CREATE_SQ }
    };
    
    nvme_create_sq_command_t sq = {
        .dptr.prp1 = arch_mmu_physical(NULL, (uintptr_t)queue->sq.addr),
        .qsize = NVME_QUEUE_DEPTH - 1,
        .qid = id,
        .cqid = id,
        .qprio = 0,
        .pc = 1,
        .nvmsetid = 0,
    };

    *((nvme_create_sq_command_t*)&create_sq_xfer.ent.command) = sq;

    if (nvme_submitWait(nvme->admin, &create_sq_xfer)) {
        // TODO leak
        LOG(ERR, "NVME_OPC_CREATE_SQ failure.\n");
        return NULL;
    }

    return queue;
}

/**
 * @brief build PRP setup
 * @param ns Namespace
 * @param prp1 PRP1 pointer
 * @param prp2 PRP2 pointer
 * @param sectors The amount of sectors needed to transfer
 */
static void nvme_buildPRP(nvme_namespace_t *ns, uintptr_t dma, uint64_t *prp1, uint64_t *prp2, size_t sectors) {
    *prp1 = dma;

    if (sectors <= 1) {
        *prp2 = 0;
        return;
    }

    if (sectors <= 2) {
        *prp2 = (uint64_t)PAGE_ALIGN_DOWN(dma + PAGE_SIZE);
        return;
    }

    sectors -= 2;

    size_t narrays = sectors / (PAGE_SIZE / sizeof(uintptr_t));
    assert(narrays <= 1 && "PRP arrays greater than a page not impl");
    
    uintptr_t *dma_page = (uintptr_t*)dma_map(PAGE_SIZE);
    *prp2 = (uint64_t)dma_page;

    for (unsigned i = 0; i < sectors; i++) {
        dma_page[i] = arch_mmu_physical(NULL, (uintptr_t)dma + PAGE_SIZE*(i+1));
    }
}

/**
 * @brief NVMe read
 */
static ssize_t nvme_read(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer) {
    nvme_namespace_t *ns = (nvme_namespace_t*)d->driver;
    size_t s = PAGE_ALIGN_UP(min(PAGE_SIZE*16,sectors*d->sector_size));
    uintptr_t dma = dma_map(s);

    size_t offset = 0;
    while (offset < sectors) {
        uint16_t count = sectors - offset;
        if (count > s/d->sector_size) {
            count = s/d->sector_size;
        }
        
        nvme_transfer_t xfer = { .ent = { .opc = NVME_OPC_READ } };
        nvme_read_command_t *cmd = (nvme_read_command_t*)&xfer.ent.command;

        cmd->nsid = ns->nsid;
        cmd->slba = lba + offset;
        cmd->nlb = count - 1;

        uint64_t prp1 = 0;
        uint64_t prp2 = 0;
        nvme_buildPRP(ns, dma, &prp1, &prp2, count);
        cmd->dptr.prp1 = arch_mmu_physical(NULL, prp1);
        cmd->dptr.prp2 = arch_mmu_physical(NULL, prp2);

        int ioqueueidx = GET_IO_QUEUE(ns->controller);
        nvme_queue_t *ioqueue = ns->controller->ioqueues[ioqueueidx];
        if (nvme_submitWait(ioqueue, &xfer)) {
            LOG(ERR, "Read failed, NVME_OPC_IO_READ failed\n");
            dma_unmap(dma, s);
            return -EIO;
        }

        if (count > 2) dma_unmap(prp2, PAGE_SIZE);

        memcpy(buffer + offset * d->sector_size, (void*)dma, count * d->sector_size);
        offset += count;
    }

    dma_unmap(dma, s);
    return sectors;
}

/**
 * @brief NVMe read
 */
static ssize_t nvme_write(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer) {
    nvme_namespace_t *ns = (nvme_namespace_t*)d->driver;

    assert(IS_ALIGNED((uintptr_t)buffer, PAGE_SIZE));
    
    if (sectors <= PAGE_SIZE/512) {
        // debug fast path
        nvme_transfer_t xfer = { .ent = { .opc = NVME_OPC_WRITE } };
        nvme_read_command_t *cmd = (nvme_read_command_t*)&xfer.ent.command;

        cmd->nsid = ns->nsid;
        cmd->slba = lba;
        cmd->nlb = sectors;
        cmd->dptr.prp1 = arch_mmu_physical(NULL, (uintptr_t)buffer);

        int ioqueueidx = GET_IO_QUEUE(ns->controller);
        nvme_queue_t *ioqueue = ns->controller->ioqueues[ioqueueidx];

        if (nvme_submitWait(ioqueue, &xfer)) {
            LOG(ERR, "Write failed, NVME_OPC_IO_WRITE failed\n");
            return -EIO;
        }

        return 0;
    }

    size_t s = min(PAGE_SIZE*16,sectors*d->sector_size);
    uintptr_t dma = dma_map(s);

    size_t offset = 0;
    while (offset < sectors) {
        uint16_t count = sectors - offset;
        if (count > s/d->sector_size) {
            count = s/d->sector_size;
        }
        
        nvme_transfer_t xfer = { .ent = { .opc = NVME_OPC_WRITE } };
        nvme_read_command_t *cmd = (nvme_read_command_t*)&xfer.ent.command;

        memcpy((void*)dma, buffer + offset * d->sector_size, count * d->sector_size);
        cmd->nsid = ns->nsid;
        cmd->slba = lba + offset;
        cmd->nlb = count - 1;

        uint64_t prp1 = 0;
        uint64_t prp2 = 0;
        nvme_buildPRP(ns, dma, &prp1, &prp2, count);
        cmd->dptr.prp1 = arch_mmu_physical(NULL, prp1);
        cmd->dptr.prp2 = arch_mmu_physical(NULL, prp2);

        int ioqueueidx = GET_IO_QUEUE(ns->controller);
        nvme_queue_t *ioqueue = ns->controller->ioqueues[ioqueueidx];

        if (nvme_submitWait(ioqueue, &xfer)) {
            LOG(ERR, "Write failed, NVME_OPC_IO_WRITE failed\n");
            dma_unmap(dma, s);
            return -EIO;
        }

        if (count > 2) dma_unmap(prp2, PAGE_SIZE);

        offset += count;
    }

    dma_unmap(dma, s);
    return sectors;
}

static ssize_t nvme_io_range(drive_t *d, page_range_t *r, bool write) {
    nvme_namespace_t *ns = (nvme_namespace_t*)d->driver;
    nvme_t *ctrl = ns->controller;

    // If mdts is 0 no limit is enforecd
    size_t max_pages_per_chunk = r->npages; 
    if (ctrl->ident.mdts > 0) {
        if (ctrl->ident.mdts > sizeof(size_t)*8) {
            max_pages_per_chunk = SIZE_MAX;
        } else {
            max_pages_per_chunk = (1 << ctrl->ident.mdts);
        }
    }

    size_t pages_left = r->npages;
    size_t current_page_index = 0;
    uint64_t current_lba = r->offset / d->sector_size;
    int ret = 0;

    int ioqueueidx = GET_IO_QUEUE(ctrl);
    nvme_queue_t *ioqueue = ctrl->ioqueues[ioqueueidx];

    while (pages_left > 0) {
        size_t chunk_npages = (pages_left > max_pages_per_chunk) ? max_pages_per_chunk : pages_left;
        size_t chunk_sectors = (chunk_npages * PAGE_SIZE) / d->sector_size;

        nvme_transfer_t xfer = { .ent = { .opc = write ? NVME_OPC_WRITE : NVME_OPC_READ } };
        nvme_read_command_t *cmd = (nvme_read_command_t*)&xfer.ent.command;

        cmd->nsid = ns->nsid;
        cmd->slba = current_lba;
        cmd->nlb = chunk_sectors - 1;
        
        cmd->dptr.prp1 = r->pages[current_page_index];

        uintptr_t *semipage = NULL;

        if (chunk_npages <= 1) {
            cmd->dptr.prp2 = 0;
        } else if (chunk_npages <= 2) {
            cmd->dptr.prp2 = r->pages[current_page_index + 1];
        } else {
            size_t pages_per_arr = (PAGE_SIZE / sizeof(uintptr_t));
            size_t narrays = (chunk_npages + (pages_per_arr - 1)) / pages_per_arr;

            semipage = (uintptr_t*)dma_map(narrays * PAGE_SIZE);
            if (!semipage) {
                ret = -ENOMEM;
                break;
            }
            memset(semipage, 0, narrays * PAGE_SIZE);
            
            for (unsigned i = 0; i < chunk_npages - 1; i++) {
                semipage[i] = r->pages[current_page_index + 1 + i];
            }

            cmd->dptr.prp2 = arch_mmu_physical(NULL, (uintptr_t)semipage);
        }

        if (nvme_submitWait(ioqueue, &xfer)) {
            LOG(ERR, "I/O access failed on chunk at LBA 0x%llx (status = 0x%x)\n", current_lba, xfer.status);
            ret = -EIO;
        }

        if (semipage) {
            size_t pages_per_arr = (PAGE_SIZE / sizeof(uintptr_t));
            size_t narrays = (chunk_npages + (pages_per_arr - 1)) / pages_per_arr;
            dma_unmap((uintptr_t)semipage, narrays * PAGE_SIZE); 
        }

        if (ret < 0) {
            break;
        }

        pages_left -= chunk_npages;
        current_page_index += chunk_npages;
        current_lba += chunk_sectors;
    }

    return ret;
}

/**
 * @brief NVMe read range
 */
static ssize_t nvme_read_range(drive_t *d, page_range_t *r) {
    return nvme_io_range(d, r, false);
}

/**
 * @brief NVMe write range
 */
static ssize_t nvme_write_range(drive_t *d, page_range_t *r) {
    return nvme_io_range(d, r, true);
}

/**
 * @brief Init namespace
 */
void nvme_namespaceInit(nvme_t *nvme, uint32_t id, nvme_namespace_identify_t *nsident) {
    nvme_namespace_t *ns = kzalloc(sizeof(nvme_namespace_t));

    ns->controller = nvme;
    ns->nsid = id;

    drive_t *d = drive_create(DRIVE_TYPE_NVME, &nvme_ops);
    
    char sn[21] = { 0 };
    strncpy(sn, nvme->ident.sn, 20);

    char mn[41] = { 0 };
    strncpy(mn, nvme->ident.mn, 40);

    d->serial = strdup(sn);
    d->model = strdup(mn);
    d->sectors = nsident->nsze;

    uint64_t format = nsident->lbafN[nsident->flbas & 0x0F];
    uint64_t block_size = 1 << ((format >> 16) & 0xFF);
    d->sector_size = block_size;
    LOG(DEBUG, "Sector size: %d\n", d->sector_size);

    d->vendor = NULL;
    d->revision = NULL; // TODO: fill this

    d->driver = (void*)ns;

    drive_mount(d);
}

/**
 * @brief ID namespaces
 */
int nvme_identifyNamespaces(nvme_t *nvme) {
    uintptr_t namespace_page = dma_map(PAGE_SIZE);

    nvme_transfer_t xfer = { .ent = { .opc = NVME_OPC_IDENTIFY }, };
    nvme_identify_command_t *ident = (nvme_identify_command_t*)&xfer.ent.command;
    ident->dptr.prp1 = arch_mmu_physical(NULL, namespace_page);
    ident->cns = 0x02;

    if (nvme_submitWait(nvme->admin, &xfer)) {
        LOG(ERR, "NVME_OPC_IDENTIFY failed\n");
        return 1;
    }

    uint32_t nsids[PAGE_SIZE / sizeof(uint32_t)];
    memcpy(nsids, (void*)namespace_page, PAGE_SIZE);

    
    for (unsigned i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
        if (!nsids[i]) break;

        nvme_transfer_t xfer_ns = { .ent = { .opc = NVME_OPC_IDENTIFY }};

        nvme_identify_command_t *ident = (nvme_identify_command_t*)&xfer.ent.command;
        ident->dptr.prp1 = arch_mmu_physical(NULL, namespace_page);
        ident->cns = NVME_CNS_IDENTIFY_NAMESPACE;
        ident->nsid = nsids[i];

        LOG(DEBUG, "Identify namespace ID %x\n", nsids[i]);

            
        if (nvme_submitWait(nvme->admin, &xfer)) {
            LOG(ERR, "NVME_OPC_IDENTIFY failed\n");
            dma_unmap(namespace_page, PAGE_SIZE);
            return 1;
        }

        nvme_namespace_identify_t *ns = (nvme_namespace_identify_t*)namespace_page;

        uint64_t format = ns->lbafN[ns->flbas & 0x0F];
        uint64_t block_size = 1 << ((format >> 16) & 0xFF);

        LOG(DEBUG, "Block count: %d blocks\n", ns->nsze);
        LOG(DEBUG, "Block size: %d bytes\n", block_size);
        LOG(DEBUG, "Total: %d MiB\n", ns->nsze * block_size / (1 << 20));

        nvme_namespaceInit(nvme, nsids[i], ns);
    }

    dma_unmap(namespace_page, PAGE_SIZE);
    return 0;
}

/**
 * @brief Initialize NVMe controller
 */
int nvme_init(pci_device_t *dev) {
    LOG(INFO, "Initializing NVMe controller on bus %d slot %d function %d\n", dev->bus, dev->slot, dev->function);

    pci_bar_t *bar = pci_readBAR(dev->bus,dev->slot,dev->function,0);
    if (!bar || (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64)) {
        LOG(ERR, "Missing or invalid BAR0\n");
        return 0;
    }

    uintptr_t mmio_base = mmio_map(bar->address, bar->size);
    size_t mmio_size = bar->size;
    kfree(bar);

    // Prepare the controller cmd
    uint16_t cmd;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &cmd);
    cmd |= PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY_SPACE;
    cmd &= ~(PCI_COMMAND_IO_SPACE);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, cmd);

    // Create the NVMe structure
    nvme_t *nvme = kmalloc(sizeof(nvme_t));
    nvme->regs = (nvme_registers_t*)mmio_base;
    nvme->pcidev = dev;
    nvme->admin = NULL;
    nvme->ioqueues = NULL;
    nvme->nqueues = 0;
    nvme->last_queue = 0;

    LOG(DEBUG, "NVMe controller version %d.%d\n", (uint16_t)nvme->regs->vs.mjr, (uint8_t)nvme->regs->vs.mnr);
    LOG(DEBUG, "Command sets supported: %s%s%s\n", (nvme->regs->cap.css & NVME_CAP_CSS_NVME) ? "NVME " : "", (nvme->regs->cap.css & NVME_CAP_CSS_IO) ? "IO " : "", (nvme->regs->cap.css & NVME_CAP_CSS_ADMIN) ? "ADMIN" : "");
    LOG(DEBUG, "Page sizes: %d - %d\n", (1 << (12 + nvme->regs->cap.mpsmin)), (1 << (12 + nvme->regs->cap.mpsmax)));

    if ((nvme->regs->cap.css & NVME_CAP_CSS_NVME) == 0) {
        LOG(ERR, "Controller must support NVMe commands.\n");
        kfree(nvme);
        mmio_unmap(mmio_base, mmio_size);
        return 1;
    }  

    // Reset the NVMe controller
    nvme->regs->cc.en = 0;
    while (nvme->regs->csts.rdy) arch_pause_single();

    // NVMe controller is ready, apply configurations...
    nvme->regs->cc.ams = 0;
    nvme->regs->cc.mps = MMU_SHIFT - 12;
    nvme->regs->cc.css = 0;

    // Prepare admin queue
    nvme->admin = nvme_createQueue(NVME_GET_DOORBELL_SQ(0), NVME_GET_DOORBELL_CQ(0));
    nvme->regs->aqa.acqs = NVME_QUEUE_DEPTH-1;
    nvme->regs->aqa.asqs = NVME_QUEUE_DEPTH-1;
    nvme->regs->asq.asqb = arch_mmu_physical(NULL, (uintptr_t)nvme->admin->sq.addr);
    nvme->regs->acq.acqb = arch_mmu_physical(NULL, (uintptr_t)nvme->admin->cq.addr);

    // Allocate IRQs for us
    int ret = pci_allocateInterrupts(dev, 2, NVME_MAX_IO_QUEUES + 1, PCI_IRQ_ALL);   
    if (ret < 2) {
        LOG(ERR, "No available interrupts for device (ret=%d).\n", ret);
        goto _cleanup;
    }

    LOG(INFO, "Allocated %d interrupts successfully\n", ret);

    // Register interrupt for admin queue
    pci_irq_t *admin_irq = pci_getInterruptVector(dev, 0);
    irq_register(admin_irq->vector, nvme_irq, IRQ_FLAG_DEFAULT, nvme->admin, NULL);

    // Start controller
    nvme->regs->cc.en = 1;
    while (!(nvme->regs->csts.rdy)) arch_pause_single();

    // Identify
    if (nvme_identify(nvme, &nvme->ident)) {
        LOG(ERR, "Failed to identify controller.\n");
        goto _cleanup;
    }

    LOG(DEBUG, "Model name %s\n", nvme->ident.mn);

    // TODO verify IOCQES and IOSQES
    nvme->regs->cc.iocqes = 4;
    nvme->regs->cc.iosqes = 6;
    
    // Initialize all I/O queues
    int queues = min(ret-1, NVME_MAX_IO_QUEUES);
    LOG(INFO, "Initializing %d I/O queues.\n", queues);
    queues = nvme_allocateQueues(nvme, queues);
    if (queues < 0) {
        LOG(ERR, "Error %d allocating I/O queues.\n", queues);
        goto _cleanup;
    }

    LOG(INFO, "Controller allocated %d I/O queues.\n", queues);

    nvme->ioqueues = kmalloc(sizeof(nvme_queue_t*) * queues);
    nvme->nqueues = queues;
    for (int i = 0; i < queues; i++) {
        pci_irq_t *irq = pci_getInterruptVector(dev,i+1);
        nvme->ioqueues[i] = nvme_createIOQueue(nvme,i+1,irq->vector);
        irq_register(irq->vector, nvme_irq, IRQ_FLAG_DEFAULT, (void*)nvme->ioqueues[i], NULL);
    }

    // ID namespaces
    if (nvme_identifyNamespaces(nvme)) {
        LOG(ERR, "Error identifying NVMe namespaces\n");
        goto _cleanup;
    }

    return 0;

_cleanup:
    // TODO free queues
    pci_freeInterrupts(dev);
    kfree(nvme);
    mmio_unmap(mmio_base, mmio_size);
    return 1;
}

/**
 * @brief NVMe scan
 */
int nvme_scan(pci_device_t *dev, void *data) {
    return nvme_init(dev);
}

int driver_init(int argc, char *argv[]) {
    pci_scan_parameters_t params = {
        .class_code = 0x01,
        .subclass_code = 0x08
    };

    pci_scanDevice(nvme_scan, &params, NULL);
    return 0;
}

int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = { 
    .name = "NVMe Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
