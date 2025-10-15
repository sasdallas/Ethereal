/**
 * @file drivers/storage/nvme/nvme.c
 * @brief NVMe driver
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
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/hal.h>
#include <kernel/arch/arch.h>
#include <kernel/drivers/storage/drive.h>
#include <kernel/misc/util.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:NVME", __VA_ARGS__)

#undef TIMEOUT
#pragma GCC diagnostic ignored "-Wpedantic"
#define TIMEOUT(a, b) ({ while (!(a)) arch_pause_single(); 0; })

/**
 * @brief Reset an NVMe device
 * @param nvme The NVMe device to reset
 */
int nvme_reset(nvme_t *nvme) {
    if (nvme->regs->cc.en) {
        // Wait for CSTS RDY
        if (TIMEOUT((nvme->regs->csts.rdy), 1000)) {
            return 1;
        }
    }

    nvme->regs->cc.en = 0;
    return TIMEOUT(!(nvme->regs->csts.rdy), 1000);
}

/**
 * @brief NVMe controller global IRQ handler
 * @param context NVMe object
 */
int nvme_irq(void *context) {
    nvme_t *nvme = (nvme_t*)context;
    nvme_irqQueue(nvme->admin_queue);
    return 0;
}

/**
 * @brief NVMe controller global IRQ handler
 * @param context NVMe object
 */
int nvme_irqIO(void *context) {
    nvme_t *nvme = (nvme_t*)context;
    nvme_irqQueue(nvme->io_queue);
    return 0;
}

/**
 * @brief Create the admin queue for the NVMe
 * @param nvme The NVMe object
 */
int nvme_createAdminQueue(nvme_t *nvme) {
    // Create the admin queue
    nvme->admin_queue = nvme_createQueue(NVME_ADMIN_QUEUE_DEPTH, NVME_GET_DOORBELL(0));
    
    // Program admin queue in
    nvme->regs->aqa.acqs = NVME_ADMIN_QUEUE_DEPTH-1;
    nvme->regs->aqa.asqs = NVME_ADMIN_QUEUE_DEPTH-1;
    nvme->regs->asq.asqb = mem_getPhysicalAddress(NULL, (uintptr_t)nvme->admin_queue->sq);
    nvme->regs->acq.acqb = mem_getPhysicalAddress(NULL, (uintptr_t)nvme->admin_queue->cq);

    return 0;
}

/**
 * @brief Submit entry and wait
 * @param nvme The NVMe object
 * @param queue The queue to submit to
 * @param entry The entry to submit
 */
int nvme_submitAndWait(nvme_t *nvme, nvme_queue_t *queue, nvme_sq_entry_t *entry) {
    nvme_submitQueue(queue, entry);
    
    // !!!: TEMPORARY
    while (!queue->completions->length) {
        arch_pause_single();
    }

    // Now let's wait for a completion event
    if (TIMEOUT((queue->completions->length), 1000)) {
        LOG(ERR, "NVMe command %x timed out\n", entry->opc);
        
        return 1;
    }

    // Get the completion event
    node_t *n = list_popleft(queue->completions);
    nvme_completion_t *comp = (nvme_completion_t*)n->value;
    kfree(n);

    if (comp->status != NVME_STATUS_SUCCESS) {
        LOG(ERR, "NVMe command %x failed with status: %d\n", entry->opc, comp->status);
        kfree(comp);
        return 1;
    }

    kfree(comp);

    return 0;
}

/**
 * @brief Create the I/O queue for the NVMe
 * @param nvme The NVMe object
 */
int nvme_createIOQueue(nvme_t *nvme) {
    nvme->io_queue = nvme_createQueue(NVME_IO_QUEUE_DEPTH, NVME_GET_DOORBELL(1));
    nvme_sq_entry_t entry = { .opc = NVME_OPC_CREATE_CQ };

    nvme_create_cq_command_t cq = {
        .dptr.prp1 = mem_getPhysicalAddress(NULL, (uintptr_t)nvme->io_queue->cq),
        .qsize = NVME_IO_QUEUE_DEPTH - 1,
        .qid = 1,
        .iv = 1,
        .ien = 1,
        .pc = 1,
    };

    memcpy(&entry.command, &cq, sizeof(nvme_create_cq_command_t));

    if (nvme_submitAndWait(nvme, nvme->admin_queue, &entry)) {
        LOG(ERR, "NVME_OPC_CREATE_CQ failed\n");
        return 1;
    }

    entry.opc = NVME_OPC_CREATE_SQ;
    nvme_create_sq_command_t sq = {
        .dptr.prp1 = mem_getPhysicalAddress(NULL, (uintptr_t)nvme->io_queue->sq),
        .qsize = NVME_IO_QUEUE_DEPTH - 1,
        .qid = 1,
        .cqid = 1,
        .qprio = 0,
        .pc = 1,
        .nvmsetid = 0,
    };

    memcpy(&entry.command, &sq, sizeof(nvme_create_sq_command_t));

    
    if (nvme_submitAndWait(nvme, nvme->admin_queue, &entry)) {
        LOG(ERR, "NVME_OPC_CREATE_SQ failed\n");
        return 1;
    }

    uint8_t i = pci_enableMSI(nvme->dev->bus, nvme->dev->slot, nvme->dev->function);
    assert(i != 0xFF);

    hal_registerInterruptHandler(i, nvme_irqIO, nvme);
    return 0;
}

/**
 * @brief Identify the NVMe controller
 * @param nvme The NVMe object
 */
int nvme_identify(nvme_t *nvme) {
    LOG(DEBUG, "Sending IDENTIFY request to NVMe drive\n");

    // Allocate a DMA page to hold the identification data
    uintptr_t id_page = mem_allocateDMA(PAGE_SIZE);
    memset((void*)id_page, 0, PAGE_SIZE);

    nvme_sq_entry_t entry = { .opc = NVME_OPC_IDENTIFY };
    nvme_identify_command_t *cmd = (nvme_identify_command_t*)&entry.command;

    // Setup identify cmd
    cmd->dptr.prp1 = mem_getPhysicalAddress(NULL, id_page);
    cmd->cns = NVME_CNS_IDENTIFY_CONTROLLER;
    
    // Submit
    nvme_submitQueue(nvme->admin_queue, &entry);

    while (!nvme->admin_queue->completions->length) {
        arch_pause_single();
    }

    // Now let's wait for a completion event
    if (TIMEOUT((nvme->admin_queue->completions->length), 1000)) {
        LOG(ERR, "NVME_CMD_IDENTIFY timed out\n");
        mem_freeDMA(id_page, PAGE_SIZE);
        return 1;
    }

    // Get the completion event
    node_t *n = list_popleft(nvme->admin_queue->completions);
    nvme_completion_t *comp = (nvme_completion_t*)n->value;
    kfree(n);

    if (comp->status != NVME_STATUS_SUCCESS) {
        LOG(ERR, "NVME_CMD_IDENTIFY failed with status: %d\n", comp->status);
        kfree(comp);
        mem_freeDMA(id_page, PAGE_SIZE);
        return 1;
    }

    kfree(comp);

    // We should have the ID
    nvme_ident_t *ident = (nvme_ident_t*)id_page;
    nvme->ident = ident;
    LOG(DEBUG, "model: %s\n", ident->mn);


    return 0;
}

/**
 * @brief Read sectors
 */
ssize_t nvme_read(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer) {
    nvme_namespace_t *ns = (nvme_namespace_t*)d->driver;
    size_t sector_offset = 0;
    while (sector_offset < sectors) {
        uint16_t count = sectors - sector_offset;
        if (count > PAGE_SIZE / d->sector_size) {
            count = PAGE_SIZE / d->sector_size;
        }

        nvme_sq_entry_t ent = { .opc = NVME_OPC_READ };
        nvme_read_command_t *cmd = (nvme_read_command_t*)&ent.command;

        cmd->nsid = ns->nsid;
        cmd->dptr.prp1 = mem_getPhysicalAddress(NULL, ns->dma_region);
        cmd->slba = lba + sector_offset;
        cmd->nlb = count - 1;
        
        if (nvme_submitAndWait(ns->controller, ns->controller->io_queue, &ent)) {
            LOG(ERR, "Read failed, NVME_OPC_IO_READ failed\n");
            return -EIO;
        }

        memcpy(buffer + sector_offset * d->sector_size, (void*)ns->dma_region, count * d->sector_size);

        sector_offset += count;
    }

    return sectors;
}

/**
 * @brief Initialize NVMe namespace
 * @param nvme The NVMe drive
 * @param nsid Namespace ID
 * @param nsident Namespace identification
 */
int nvme_namespaceInit(nvme_t *nvme, uint32_t nsid, nvme_namespace_identify_t *nsident) {
    nvme_namespace_t *ns = kzalloc(sizeof(nvme_namespace_t));

    ns->controller = nvme;
    ns->nsid = nsid;
    ns->dma_region = mem_allocateDMA(PAGE_SIZE);

    drive_t *d = drive_create(DRIVE_TYPE_NVME);
    
    char sn[21] = { 0 };
    strncpy(sn, nvme->ident->sn, 20);

    char mn[41] = { 0 };
    strncpy(mn, nvme->ident->mn, 40);

    d->serial = strdup(sn);
    d->model = strdup(mn);
    d->sectors = nsident->nsze;

    uint64_t format = nsident->lbafN[nsident->flbas & 0x0F];
    uint64_t block_size = 1 << ((format >> 16) & 0xFF);
    d->sector_size = block_size;

    d->vendor = NULL;
    d->revision = NULL; // TODO: fill this

    d->read_sectors = nvme_read;

    d->driver = (void*)ns;

    drive_mount(d);


    return 0;
}

/**
 * @brief Identify NVMe namespaces
 * @param nvme The NVMe drive to identify namespaces on
 */
int nvme_identifyNamespaces(nvme_t *nvme) {
    uintptr_t namespace_page = mem_allocateDMA(PAGE_SIZE);

    nvme_sq_entry_t e = { .opc = NVME_OPC_IDENTIFY, };
    nvme_identify_command_t *ident = (nvme_identify_command_t*)&e.command;
    ident->dptr.prp1 = mem_getPhysicalAddress(NULL, namespace_page);
    ident->cns = 0x02;

    if (nvme_submitAndWait(nvme, nvme->admin_queue, &e)) {
        LOG(ERR, "NVME_OPC_IDENTIFY failed\n");
        return 1;
    }

    uint32_t nsids[PAGE_SIZE / sizeof(uint32_t)];
    memcpy(nsids, (void*)namespace_page, PAGE_SIZE);

    
    for (unsigned i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
        if (!nsids[i]) break;

        e.opc = NVME_OPC_IDENTIFY;
        nvme_identify_command_t *ident = (nvme_identify_command_t*)&e.command;
        ident->dptr.prp1 = mem_getPhysicalAddress(NULL, namespace_page);
        ident->cns = NVME_CNS_IDENTIFY_NAMESPACE;
        ident->nsid = nsids[i];

        LOG(DEBUG, "Identify namespace ID %x\n", nsids[i]);

            
        if (nvme_submitAndWait(nvme, nvme->admin_queue, &e)) {
            LOG(ERR, "NVME_OPC_IDENTIFY failed\n");
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

    return 0;
}


/**
 * @brief Start the NVMe controller
 * @param nvme The NVMe controller
 */
int nvme_start(nvme_t *nvme) {
    // Configure controller parameters
    nvme->regs->cc.ams = 0;
    nvme->regs->cc.mps = MEM_PAGE_SHIFT - 12;
    nvme->regs->cc.css = 0;

    // Enable controller and wait until ready
    nvme->regs->cc.en = 1;
    return TIMEOUT((nvme->regs->csts.rdy), 1000);
}

/**
 * @brief Initialize an NVMe device
 * @param dev The PCI device
 */
int nvme_init(pci_device_t *dev) {
    LOG(INFO, "Initializing NVMe controller on bus %d slot %d function %d\n", dev->bus, dev->slot, dev->function);

    // Get the BAR
    pci_bar_t *bar = pci_readBAR(dev->bus, dev->slot, dev->function, 0);

    if (!bar) {
        LOG(ERR, "NVMe controller does not have BAR0\n");
        return 0;
    }

    if (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64) {
        LOG(ERR, "NVMe controller has non-memory BAR0\n");
        return 0;
    }

    // Let's map the BAR in
    uintptr_t mmio_base = mem_mapMMIO(bar->address, bar->size);

    // Create the NVMe structure
    nvme_t *nvme = kzalloc(sizeof(nvme_t));
    nvme->regs = (nvme_registers_t*)mmio_base;
    nvme->dev = dev;

    LOG(DEBUG, "NVMe controller version %d.%d\n", (uint16_t)nvme->regs->vs.mjr, (uint8_t)nvme->regs->vs.mnr);
    LOG(DEBUG, "Command sets supported: %s%s%s\n", (nvme->regs->cap.css & NVME_CAP_CSS_NVME) ? "NVME " : "", (nvme->regs->cap.css & NVME_CAP_CSS_IO) ? "IO " : "", (nvme->regs->cap.css & NVME_CAP_CSS_ADMIN) ? "ADMIN" : "");
    LOG(DEBUG, "Page sizes: %d - %d\n", (1 << (12 + nvme->regs->cap.mpsmin)), (1 << (12 + nvme->regs->cap.mpsmax)));

    // Reset the NVMe controller
    if (nvme_reset(nvme)) {
        LOG(ERR, "Timed out while resetting NVMe controller\n");
        goto _nvme_cleanup;
    }

    // Enable interrupts
    uint8_t irq = pci_enableMSI(dev->bus, dev->slot, dev->function);
    if (irq == 0xFF || hal_registerInterruptHandler(irq, nvme_irq, (void*)nvme)) {
        LOG(DEBUG, "MSI unavailable, fallback to pin interrupt\n");
        irq = pci_getInterrupt(dev->bus, dev->slot, dev->function);

        LOG(DEBUG, "Got IRQ%d\n", irq);
        if (hal_registerInterruptHandler(irq, nvme_irq, (void*)nvme)) {
            LOG(ERR, "Error registering IRQs\n");
            goto _nvme_cleanup;
        }
    }

    // Create admin queue
    if (nvme_createAdminQueue(nvme)) {
        LOG(ERR, "Error creating admin queue\n");
        goto _nvme_cleanup;
    }

    // Enable NVMe
    if (nvme_start(nvme)) {
        LOG(ERR, "Error starting NVMe\n");
        goto _nvme_cleanup;
    }

    // Identify NVMe
    if (nvme_identify(nvme)) {
        LOG(ERR, "Error identifying NVMe\n");
        goto _nvme_cleanup;
    }

    // Configure IOSQES and IOCQES
    nvme->regs->cc.iocqes = 4;
    nvme->regs->cc.iosqes = 6;

    // Create I/O queue
    if (nvme_createIOQueue(nvme)) {
        LOG(ERR, "Error creating I/O queue\n");
        goto _nvme_cleanup;
    }

    // Identify namespaces
    if (nvme_identifyNamespaces(nvme)) {
        LOG(ERR, "Error identifying NVMe namespaces\n");
        goto _nvme_cleanup;
    }

    kfree(bar);
    return 0;

_nvme_cleanup:
    mem_unmapMMIO((uintptr_t)nvme->regs, bar->size);
    kfree(nvme);
    return 1;
}

/**
 * @brief PCI scan method for NVMe device
 */
int nvme_scan(pci_device_t *dev, void *data) {
    // Should just be an NVMe device
    return nvme_init(dev);
}

/**
 * @brief Init method
 */
int driver_init(int argc, char *argv[]) {
    pci_scan_parameters_t params = {
        .class_code = 0x01,
        .subclass_code = 0x08
    };

    pci_scanDevice(nvme_scan, &params, NULL);
    return 0;
}

/**
 * @brief Deinit method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = { 
    .name = "NVMe Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};