/**
 * @file drivers/storage/ahci/ahci.c
 * @brief AHCI driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "ahci.h"
#include <kernel/drivers/storage/drive.h>
#include <kernel/subsystems/irq.h>
#include <kernel/processor_data.h>
#include <kernel/loader/driver.h>
#include <kernel/drivers/clock.h>
#include <kernel/task/process.h>
#include <kernel/drivers/pci.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:AHCI", __VA_ARGS__)

static ssize_t ahci_read_range(drive_t *d, page_range_t *r);
static ssize_t ahci_write_range(drive_t *d, page_range_t *r);
static ssize_t ahci_read_sectors(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer);
static ssize_t ahci_write_sectors(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer);

#define AHCI_REORDER_BYTES(buffer, size)     for (int i = 0; i < size-1; i+=2) { uint8_t tmp = ((uint8_t*)buffer)[i+1]; ((uint8_t*)buffer)[i+1] = ((uint8_t*)buffer)[i]; ((uint8_t*)buffer)[i] = tmp; }


drive_ops_t ahci_ops = {
    .read_sectors = ahci_read_sectors,
    .write_sectors = ahci_write_sectors,
    .read_range = ahci_read_range,
    .write_range = ahci_write_range,
    .ioctl = NULL,
    .flush = NULL
};

/**
 * @brief AHCI handle port error
 */
void ahci_handlePortError(ahci_port_t *port, int ccs) {
    // One of two things happened:
    // Either a serial ATA error which is recoverable,
    // or a software error (which is not)

    uint32_t is = port->regs->is;
    if (is & (AHCI_PORT_IS_UFS | AHCI_PORT_IS_OFS | AHCI_PORT_IS_TFES)) {
        // Software error, not recoverable
        __atomic_fetch_or(&port->slot_errors, (1 << ccs), __ATOMIC_RELAXED);
    } else {
        // Recoverable error
        LOG(WARN, "Recovering AHCI controller, IS=%x CCS=%d.\n", is, ccs);

        // First stop command engine
        port->regs->cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);
        while ((port->regs->cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR))) arch_pause_single();

        // Clear SERR
        port->regs->serr = port->regs->serr;

        // Restart CCS, if valid
        if (port->regs->ci) {
            port->regs->ci &= ~(1 << ccs);
        } 

        // Restart command engine
        port->regs->cmd &= ~(AHCI_PORT_CMD_FRE);
        while ((port->regs->cmd & (AHCI_PORT_CMD_FR)) == 0) arch_pause_single();
        port->regs->cmd &= ~(AHCI_PORT_CMD_ST);
        while ((port->regs->cmd & (AHCI_PORT_CMD_CR)) == 0) arch_pause_single();

        // Send all slots
        port->regs->ci = __atomic_load_n(&port->slot_bitmap, __ATOMIC_RELAXED);
    }
}

/**
 * @brief AHCI tasklet
 */
void ahci_tasklet(void *context) {
    ahci_port_t *p = (ahci_port_t*)context;

    if (p->regs->tfd & AHCI_PORT_TFD_STS_ERR) {
        // TFD was set, this current command is aborted
        int error_slot = AHCI_PORT_CMD_CCS(p->regs->cmd);
        LOG(ERR, "Slot %d encountered an error while executing.\n", error_slot);
        ahci_handlePortError(p, error_slot);
    } else {
        // Clear SERR if it was set
        if (p->regs->serr) p->regs->serr = p->regs->serr;
    }

    // Notify
    uint32_t ci = p->regs->ci;
    uint32_t bmap = __atomic_load_n(&p->slot_bitmap, __ATOMIC_RELAXED);

    while (bmap) {
        int s = __builtin_ctzl(bmap);
        if (ci & (1 << s)) {
            bmap &= ~(1 << s);
            continue;
        };

        // LOG(INFO, "Slot %d complete.\n", s);
        bmap &= ~(1 << s);

        __atomic_fetch_or(&p->slot_complete, (1 << s), __ATOMIC_RELAXED);
        if (p->waiters[s]) {
            sleep_wakeup(p->waiters[s]);
        }
    }

    // ready to go!
    p->regs->is = p->regs->is;
}

/**
 * @brief AHCI IRQ handler
 */
int ahci_irq(irq_t *irq, void *context) {
    ahci_t *a = (ahci_t*)context;

    uint32_t pending = a->ghc->is;
    if (pending == 0) {
        return IRQ_NOT_SOURCE;
    }

    while (pending) {
        int i = __builtin_ctzl(pending);
        pending &= ~(1 << i);


        if (a->ports[i] != NULL) {
            tasklet_insert(&a->ports[i]->tasklet);
        }

        volatile ahci_hba_port_t *p = GHC_PORT(a->ghc, i);
        p->is = p->is;
    }

    a->ghc->is = 0xffffffff;
    return IRQ_HANDLED;
}

/**
 * @brief Claim a slot for AHCI
 */
static int ahci_claimSlot(ahci_port_t *port) {
    // look ma, no locks, just waitqueues
    wait_queue_node_t n;
    waitqueue_add(&port->slot_wait, &n);

    // TODO: adjust memory ordering
    uint32_t cur = __atomic_load_n(&port->slot_bitmap, __ATOMIC_RELAXED);
    uint32_t new;
    int slot = 0;
    do {
        while (cur == ~0U) {
            int w = waitqueue_wait(&port->slot_wait, &n, -1);
            waitqueue_remove(&port->slot_wait, &n);

            if (w != 0) {
                return w;
            }

            // re-add the node
            waitqueue_add(&port->slot_wait, &n);

            cur = __atomic_load_n(&port->slot_bitmap, __ATOMIC_RELAXED);
        }

        slot = __builtin_ctzl(~cur);
        new = cur | (1U << slot);
    } while (!__atomic_compare_exchange_n(&port->slot_bitmap, &cur, new, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));

    waitqueue_remove(&port->slot_wait, &n);
    __atomic_fetch_and(&port->slot_errors, ~(1U << slot), __ATOMIC_RELAXED);
    __atomic_fetch_and(&port->slot_complete, ~(1U << slot), __ATOMIC_RELAXED);
    return slot;
}

/**
 * @brief Submit command into port and wait
 * @param port The port to enqueue into
 * @param cmd The command to enqueue
 */
static int ahci_submitWait(ahci_port_t *port, ahci_cmd_header_t *cmd) {
    int slot = ahci_claimSlot(port);

    // LOG(DEBUG, "claimed slot %d\n", slot);

    // Because we own the slot we can write to it with no problems
    void *hdr = (void*)(port->cmd_region + (sizeof(ahci_cmd_header_t) * slot));
    memcpy(hdr, cmd, sizeof(ahci_cmd_header_t));

    thread_t *thr = current_cpu->current_thread;
    port->waiters[slot] = current_cpu->current_thread;

    if (thr) {
        sleep_prepare();
    }
    
    // barrier this so that the OR operation is synced
    BARRIER();
    port->regs->ci |= (1U << slot);

    // enter sleep
    if (thr) {
        sleep_enter();
    } else {
        while (!(port->slot_complete & (1 << slot))) {
            asm volatile (  "sti\n"
                            "hlt\n");
        }
    }

    // check for error
    int r = 0;
    if (__atomic_fetch_and(&port->slot_errors, ~(1U << slot), __ATOMIC_RELAXED) & (1 << slot)) {
        LOG(WARN, "Error occurred.\n");
        r = -EIO;
    }   

    // release the bit
    __atomic_fetch_and(&port->slot_bitmap, ~(1U << slot), __ATOMIC_RELEASE);
    waitqueue_wakeup(&port->slot_wait, 1);
    return r;
}

/**
 * @brief Allocate a command table
 * @param prdts Number of PRDTs to allocate
 */
ahci_cmd_tbl_t *ahci_allocateCommand(int prdts) {
    size_t sz = sizeof(ahci_cmd_tbl_t) + (prdts * sizeof(ahci_prdt_t));
    ahci_cmd_tbl_t *tbl = (ahci_cmd_tbl_t*)dma_map(sz);
    memset(tbl, 0, sz);
    return tbl;
}

/**
 * @brief Free a command table
 * @param prdts Number of PRDTs that were allocated
 */
void ahci_freeCommand(ahci_cmd_tbl_t *cmd, int prdts) {
    size_t sz = sizeof(ahci_cmd_tbl_t) + (prdts * sizeof(ahci_prdt_t));
    dma_unmap((uintptr_t)cmd, sz);
}

/**
 * @brief Send identify command
 * @param port The port to send the identify command to
 * @param ident Output identity
 */
static int ahci_identify(ahci_port_t *port, ahci_ident_t *id_out) {
    // need a page for cmdtbl and a page for the ident
    ahci_cmd_tbl_t *cmdtblpage = ahci_allocateCommand(1);
    uintptr_t identpage = dma_map(PAGE_SIZE);

    // Configure the FIS
    cmdtblpage->cfis.type = AHCI_FIS_TYPE_REG_H2D;
    cmdtblpage->cfis.command = ATA_CMD_IDENTIFY;
    cmdtblpage->cfis.c = 1;

    // Fill PRDT
    AHCI_PRDT_SET_BASE(cmdtblpage, 0, arch_mmu_physical(NULL,identpage));
    AHCI_PRDT_SET_BYTES(cmdtblpage, 0, 512);
    AHCI_PRDT_SET_IOC(cmdtblpage, 0, 1);

    // Configure cmd header
    ahci_cmd_header_t hdr = {
        .cfl = sizeof(ahci_fis_h2d_t) / sizeof(uint32_t),
        .w = 0,
        .prdtl = 1
    };

    AHCI_CMDHDR_LINK_TBL(&hdr, cmdtblpage);

    int ret = ahci_submitWait(port, &hdr);
    if (ret != 0) {
        goto _cleanup;
    }

    memcpy(id_out, (void*)identpage, sizeof(ahci_ident_t));

_cleanup:
    ahci_freeCommand(cmdtblpage, 1);
    dma_unmap(identpage,PAGE_SIZE);
    return ret;
}

/**
 * @brief Reset port
 */
static int ahci_resetPort(ahci_t *ahci, ahci_port_t *port) {
    LOG(INFO, "Reset AHCI port.\n");
    port->regs->cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);

    for (int i = 0; i < 50; i++) {
        if (((port->regs->cmd) & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_CR)) == 0) {
            break;
        }

        clock_sleep(10);
    }

    if (((port->regs->cmd) & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_CR)) == 0) {
        // port did reset successfully
        return 0;
    }

    // todo this means we have to send a FIS to software reset the port
    LOG(ERR, "Port %d failed to reset\n", port->num);
    return 1;
}

/**
 * @brief Init port
 */
static int ahci_initPort(ahci_t *ahci, int pnum, volatile ahci_hba_port_t *hbaport) {
    LOG(INFO, "Initializing port %d...\n", pnum);

    // allocate
    ahci_port_t *port = kmalloc(sizeof(ahci_port_t));
    WAIT_QUEUE_INIT(&port->slot_wait);
    TASKLET_INIT(&port->tasklet, "ahci tasklet", ahci_tasklet, port);
    port->controller = ahci;
    port->regs = hbaport;
    port->num = pnum;
    port->slot_bitmap = 0;
    port->slot_errors = 0;
    port->slot_complete = 0;

    // do we actually care
    if (AHCI_PORT_SSTS_DET(port->regs->ssts) != AHCI_PORT_DET_PRESENT || AHCI_PORT_SSTS_IPM(port->regs->ssts) != AHCI_PORT_IPM_ACTIVE) {
        LOG(INFO, "No device detected on port %d\n", pnum);
        kfree(port);
        return 1;
    }

    // according to spec (10.1.1) we need to make sure that these are 0s or reset the port
    if (((port->regs->cmd) & AHCI_PORT_CMD_ST) ||
        ((port->regs->cmd) & AHCI_PORT_CMD_CR) ||
        ((port->regs->cmd) & AHCI_PORT_CMD_FRE) ||
        ((port->regs->cmd) & AHCI_PORT_CMD_FR)) {
        if (ahci_resetPort(ahci, port)) {
            LOG(ERR, "Port reset failed and port was not idle.\n");
            kfree(port);
            return -1;
        }
    }

    // allocate command list and rebase controller
    size_t cmd_list_size = ((sizeof(ahci_cmd_header_t) * ahci->nslots));
    uintptr_t cmd_list_region = dma_map(cmd_list_size);
    uintptr_t fis = ahci->fis_region + (256 * pnum);
    memset((void*)cmd_list_region, 0, cmd_list_size);
    memset((void*)fis,0,256);
    port->cmd_region = cmd_list_region;

    uintptr_t cmd_list_region_phys = arch_mmu_physical(NULL, cmd_list_region);
    uintptr_t fis_phys  = arch_mmu_physical(NULL, fis);
    port->regs->clb = (cmd_list_region_phys & 0xFFFFFFFF);
    port->regs->clbu = ((cmd_list_region_phys >> 32) & 0xFFFFFFFF);
    port->regs->fb = (fis_phys & 0xFFFFFFFF);
    port->regs->fbu = ((fis_phys >> 32) & 0xFFFFFFFF);

    LOG(DEBUG, "FIS region %p, command list region %p\n", fis_phys, cmd_list_region_phys);

    // No IRQs
    port->regs->ie = 0;
    port->regs->is = 0xFFFFFFFF;

    // Ready to start command engine now
    port->regs->cmd |= AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE;

    // TODO Timeout
    while ((port->regs->cmd & (AHCI_PORT_CMD_CR)) == 0) {
        arch_pause_single();
    }
    while ((port->regs->cmd & (AHCI_PORT_CMD_FR)) == 0) {
        arch_pause_single();
    }

    // SUD is done by the BIOS

    // Clear PxSERR
    port->regs->serr = 0xFFFFFFFF;

    // do we actually care
    if (port->regs->sig != SATA_SIG_ATA) {
        LOG(INFO, "Non-ATA or Non-ATAPI device on port %d (support for SATAPI drives not implemented)\n", pnum);
        LOG(INFO, "Signature is 0x%x\n", port->regs->sig);

        // Stop port
        port->regs->cmd &= ~(AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST);
        clock_sleep(25);
        dma_unmap(cmd_list_region, cmd_list_size);

        kfree(port);
        return 1;
    }

    // Wait until AHCI port is ready
    // todo figure out the actual value to wait for
    for (int i = 0; i < 50; i++) {
        if ((port->regs->tfd & (AHCI_PORT_TFD_STS_ERR | AHCI_PORT_TFD_STS_DRQ | AHCI_PORT_TFD_STS_BSY)) == 0) {
            break;
        }

        clock_sleep(25);
    }


    if ((port->regs->tfd & (AHCI_PORT_TFD_STS_ERR | AHCI_PORT_TFD_STS_DRQ | AHCI_PORT_TFD_STS_BSY)) != 0) {
        // todo proper cleanup
        LOG(ERR, "AHCI drive timed out while initializing.\n");
        kfree(port);
        return 1;
    }

    LOG(INFO, "AHCI port %d initialized (detected an %s drive)\n", pnum, ((port->regs->sig == SATA_SIG_ATA) ? "ATA" : "ATAPI"));

    // enable IRQs
    port->regs->is = 0xFFFFFFFF;
    port->regs->ie = 0xFFFFFFFF;

    // set it
    ahci->ports[pnum] = port;
    return 0;
}

/**
 * @brief Mount the port to the drive subsystem
 */
static int ahci_mountPort(ahci_t *ahci, ahci_port_t *port) {
    if (ahci_identify(port, &port->ident)) {
        LOG(ERR, "Failed to identify port.\n");
        return 1;
    } 

    assert (!(port->ident.flags & (1 << 15)));

    size_t sectors = 0;
    if ((port->ident.command_sets & (1 << 26))) {
        LOG(INFO, "LBA48 style addressing detected\n");
        sectors = (port->ident.sectors_lba48 & 0x0000FFFFFFFFFFFF);
    } else {
        LOG(INFO, "CHS-style addressing\n");
        sectors = port->ident.sectors;
    }

    // TODO: SATAPI

    // Get model/serial
    AHCI_REORDER_BYTES(&port->ident.model, 40);
    AHCI_REORDER_BYTES(&port->ident.serial, 20);
    char model[41];
    char serial[21];
    strncpy(model, port->ident.model, 40);
    model[40] = 0;
    strncpy(serial, port->ident.serial, 20);
    serial[20] = 0;
    *(strchr(serial, ' ')) = 0;

    drive_t *d = drive_create(DRIVE_TYPE_SATA, &ahci_ops);
    d->model = strdup(model);
    d->serial = strdup(serial);
    d->sectors = sectors;
    d->sector_size = 512; // TODO
    d->driver = (void*)port;

    // TODO: Vendor, etc.
    drive_mount(d);
    return 0;
}

/**
 * @brief AHCI do I/O on range
 */
static ssize_t ahci_io_range(drive_t *d, page_range_t *r, bool write) {
    ahci_port_t *p = d->driver;
    assert(r->offset < (loff_t)d->sectors*512);

    // TODO: if we get lucky enough and have contiguous pages in memory we could lower the amount of PRDTs
    // TODO: maybe in storage subsys rev 3

    ahci_cmd_tbl_t *tbl = ahci_allocateCommand(r->npages);
    tbl->cfis.c = 1;
    tbl->cfis.type = AHCI_FIS_TYPE_REG_H2D;
    
    uint64_t lba = r->offset / 512;
    size_t sectors = (r->npages * PAGE_SIZE) / 512;

    if (lba >= (1 << 28) || sectors > 0xFF) {
        tbl->cfis.command = (write) ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    } else {
        tbl->cfis.command = (write) ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA;
        tbl->cfis.device |= ((lba >> 24) & 0x0f);
    }

    // Construct LBA
    tbl->cfis.lba0 = (lba >> 0) & 0xff;
    tbl->cfis.lba1 = (lba >> 8) & 0xff;
    tbl->cfis.lba2 = (lba >> 16) & 0xff;    
    tbl->cfis.lba3 = (lba >> 24) & 0xff;
    tbl->cfis.lba4 = (lba >> 32) & 0xff;
    tbl->cfis.lba5 = (lba >> 40) & 0xff;
    tbl->cfis.device |= 1 << 6;

    tbl->cfis.countl = sectors & 0xFF;
    tbl->cfis.counth = (sectors>>8) & 0xFF;
    
    // Build PRDT list
    size_t brem = r->npages * PAGE_SIZE;
    size_t dsize = d->sectors*d->sector_size;
    if (brem > dsize) {
        assert(brem-dsize < PAGE_SIZE);
        brem = dsize;
    }

    for (unsigned i = 0; i < r->npages; i++) {
        AHCI_PRDT_SET_BASE(tbl, i, r->pages[i]);
        
        size_t bytes_now = brem;
        if (bytes_now > PAGE_SIZE) bytes_now = PAGE_SIZE;
        AHCI_PRDT_SET_BYTES(tbl, i, bytes_now);

        if (i == r->npages-1) {
            // last PRDT
            AHCI_PRDT_SET_IOC(tbl, i, 1);
        }
        
        brem -= bytes_now;
    }

    ahci_cmd_header_t hdr = {
        .cfl = sizeof(ahci_fis_h2d_t) / 4,
        .prdtl = r->npages,
        .w = write
    };

    AHCI_CMDHDR_LINK_TBL(&hdr, tbl);
    int ret = ahci_submitWait(p, &hdr);
    ahci_freeCommand(tbl, r->npages);
    return ret;
}

/**
 * @brief AHCI read range
 */
static ssize_t ahci_read_range(drive_t *d, page_range_t *r) {
    return ahci_io_range(d, r, false);
}

/**
 * @brief AHCI write range
 */
static ssize_t ahci_write_range(drive_t *d, page_range_t *r) {
    return ahci_io_range(d, r, true);
}


/**
 * @brief AHCI IO sectors
 */
static ssize_t ahci_io_sectors(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer, bool write) {
    ahci_port_t *p = d->driver;

    // TODO: if we get lucky enough and have contiguous pages in memory we could lower the amount of PRDTs
    // TODO: maybe in storage subsys rev 3

    size_t pgs = PAGE_ALIGN_UP(sectors * 512) / PAGE_SIZE;
    ahci_cmd_tbl_t *tbl = ahci_allocateCommand(pgs);
    tbl->cfis.c = 1;
    tbl->cfis.type = AHCI_FIS_TYPE_REG_H2D;

    if (lba >= (1 << 28) || sectors > 0xFF) {
        tbl->cfis.command = (write) ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    } else {
        tbl->cfis.command = (write) ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA;
        tbl->cfis.device |= ((lba >> 24) & 0x0f);
    }

    // Construct LBA
    tbl->cfis.lba0 = (lba >> 0) & 0xff;
    tbl->cfis.lba1 = (lba >> 8) & 0xff;
    tbl->cfis.lba2 = (lba >> 16) & 0xff;    
    tbl->cfis.lba3 = (lba >> 24) & 0xff;
    tbl->cfis.lba4 = (lba >> 32) & 0xff;
    tbl->cfis.lba5 = (lba >> 40) & 0xff;
    tbl->cfis.device |= 1 << 6;

    tbl->cfis.countl = sectors & 0xFF;
    tbl->cfis.counth = (sectors>>8) & 0xFF;
    
    // Build PRDT list

    size_t brem = sectors * 512;
    for (unsigned i = 0; i < pgs; i++) {
        AHCI_PRDT_SET_BASE(tbl, i, arch_mmu_physical(NULL, (uintptr_t)buffer + (i * PAGE_SIZE)));
        
        size_t bytes_now = brem;
        if (bytes_now > PAGE_SIZE) bytes_now = PAGE_SIZE;
        AHCI_PRDT_SET_BYTES(tbl, i, bytes_now);

        if (i == pgs-1) {
            // last PRDT
            AHCI_PRDT_SET_IOC(tbl, i, 1);
        }
        
        brem -= PAGE_SIZE;
    }

    ahci_cmd_header_t hdr = {
        .cfl = sizeof(ahci_fis_h2d_t) / 4,
        .prdtl = pgs,
        .w = write
    };

    AHCI_CMDHDR_LINK_TBL(&hdr, tbl);
    int ret = ahci_submitWait(p, &hdr);
    ahci_freeCommand(tbl, pgs);
    return ret < 0 ? ret : (ssize_t)sectors;
}

static ssize_t ahci_read_sectors(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer)  {
    return ahci_io_sectors(d,lba,sectors,buffer,false);
}


static ssize_t ahci_write_sectors(drive_t *d, uint64_t lba, size_t sectors, uint8_t *buffer) {
    return ahci_io_sectors(d,lba,sectors,buffer,true);
}


/**
 * @brief Perform handoff
 */
static int ahci_handoff(ahci_t *ahci) {
    // Handoff is only needed if BOH is set in extended cap
    if ((ahci->ghc->cap2 & AHCI_CAP2_BOH) == 0) {
        return 0;
    }

    LOG(DEBUG, "Doing BIOS/OS handoff\n");
    if ((ahci->ghc->bohc & AHCI_BOH_OOS)) {
        // The OS already owns this
        return 0;
    }

    ahci->ghc->bohc |= AHCI_BOH_OOS;

    if ((ahci->ghc->bohc & AHCI_BOH_BOS) == 0) {
        return 0;
    }   

    // Wait approx 25 ms
    clock_sleep(25);

    // Now?
    if ((ahci->ghc->bohc & AHCI_BOH_BOS) || (ahci->ghc->bohc & AHCI_BOH_BB)) {
        LOG(WARN, "BIOS is still busy\n");
        clock_sleep(2000);
        
        if ((ahci->ghc->bohc & AHCI_BOH_BOS) || (ahci->ghc->bohc & AHCI_BOH_BB)) {
            LOG(ERR, "BIOS/OS handoff failed, BIOS did not release controller.\n");
            return 1;
        }
    }

    LOG(DEBUG, "BIOS released controller.\n");
    return 0;
}

/**
 * @brief AHCI reset
 */
static int ahci_reset(ahci_t *ahci) {
    ahci->ghc->ghc |= AHCI_GHC_HR;
    
    // TODO: Timeout
    while (ahci->ghc->ghc & AHCI_GHC_HR) {
        arch_pause_single();
    }

    LOG(INFO, "Controller reset ok\n");
    return 0;
}

/**
 * @brief Initialize AHCI controller
 */
int ahci_init(pci_device_t *dev) {
    LOG(INFO, "Found AHCI controller at %d.%d.%d\n", dev->bus, dev->slot, dev->function);

    // Enable memory space and busmastering DMA
    uint16_t cmd;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &cmd);
    cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    cmd &= ~(PCI_COMMAND_IO_SPACE);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, cmd);

    // Map BAR5
    pci_bar_t *bar = pci_readBAR(dev->bus, dev->slot, dev->function, 5);
    if (!bar || (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64)) {
        LOG(ERR, "AHCI device does not have BAR5 or it is invalid.\n");
        return 1;
    }

    size_t mmio_size = bar->size;
    uintptr_t mmio = mmio_map(bar->address, mmio_size);
    kfree(bar);

    // Prepare controller
    ahci_t *ahci = kmalloc(sizeof(ahci_t));
    memset(ahci, 0, sizeof(ahci_t));
    ahci->pcidev = dev;
    ahci->ghc = (volatile ahci_ghc_t*)mmio;
    ahci->nslots = AHCI_CAP_NCS(ahci->ghc->cap);
    // ahci->nports = AHCI_CAP_NPRT(ahci->ghc->cap);
    ahci->nports = 32; // use ports[] array a bitmap
    
    if (ahci->nslots < 32) {
        assert(0 && "nslots < 32, not supported!!!!");
    }

    // Enable AHCI mode
    ahci->ghc->ghc |= AHCI_GHC_AE;

    // BIOS
    if (ahci_handoff(ahci)) {
        LOG(ERR, "AHCI handoff failure.\n");
        kfree(ahci);
        mmio_unmap(mmio, mmio_size);
        return 1;
    }

    // Reset controller
    // if (ahci_reset(ahci)) {
    //     LOG(ERR, "AHCI reset failure.\n");
    //     kfree(ahci);
    //     mmio_unmap(mmio, mmio_size);
    //     return 1;
    // }

    // Get the IRQ handler running
    int irqs = pci_allocateInterrupts(dev, 1, 1, PCI_IRQ_ALL);
    if (irqs != 1) {
        LOG(ERR, "Failed to register interrupts for AHCI controller (return code %d)\n", irqs);
        kfree(ahci);
        mmio_unmap(mmio, mmio_size);
        return 1;
    }

    pci_irq_t *irq = pci_getInterruptVector(dev, 0);
    if (irq_register(irq->vector, ahci_irq, IRQ_FLAG_DEFAULT, ahci, NULL) < 0) {
        LOG(ERR, "Failed to register interrupt handler.\n");
        kfree(ahci);
        mmio_unmap(mmio, mmio_size);
        return 1;
    }

    // IRQs are not ok now
    ahci->ghc->ghc &= ~(AHCI_GHC_IE);

    // todo
    if ((ahci->ghc->cap & AHCI_CAP_64BIT) == 0) {
        LOG(ERR, "This controller is not capable of 64-bit DMA\n");
        return 1;
    }

    // Allocate a FIS region
    ahci->fis_region = dma_map(PAGE_SIZE*2);
    
    int pi = ahci->ghc->pi; // total number of working ports
    LOG(DEBUG, "Controller supports %d slots and %d ports (port mask 0x%x)\n", ahci->nslots, AHCI_CAP_NPRT(ahci->ghc->cap), pi);
    
    // Initialize part one
    int ports_initted = 0;
    for (int i = 0; i < ahci->nports; i++) {
        if (!(pi & (1 << i))) {
            LOG(INFO, "Port %d is not active.\n", i);
            continue;
        }

        int rval = ahci_initPort(ahci, i, GHC_PORT(ahci->ghc,i));

        // rval == 1 for a port that is useless, rval = 0 for a port that is good, rval < 0 for error
        if (rval < 0) {
            // TODO not bail on the entire init
            LOG(ERR, "Failed to initialize port %d\n", i);
            dma_unmap(ahci->fis_region, PAGE_SIZE*2);
            kfree(ahci);
            mmio_unmap(mmio, mmio_size);
            return 1;
        }

        if (rval == 0) {
            ports_initted++;
        }
    }

    LOG(INFO, "Initialized %d ports successfully.\n", ports_initted);

    if (!ports_initted) {
        // TODO hotplug
        LOG(INFO, "No devices were found, disabling AHCI controller.\n");
        ahci->ghc->ghc &= ~(AHCI_GHC_AE);
        return 0;
    }

    // Enable IRQs
    ahci->ghc->is = 0xFFFFFFFF;
    ahci->ghc->ghc |= AHCI_GHC_IE;

    // Initialize part 2
    for (int i = 0; i < ahci->nports; i++) {
        if (ahci->ports[i]) {
            if (ahci_mountPort(ahci, ahci->ports[i])) {
                // todo not bail on entire init
                // todo cleanup
                LOG(ERR, "Failed to detect port %d\n", i);
                return 1;
            }
        }
    }


    return 0;
}

/**
 * @brief AHCI scan
 */
int ahci_scan(pci_device_t *dev, void *device) {
    // dev must have progif of 0x1
    uint8_t pif;
    pci_readConfigByte(dev, PCI_PROGIF_OFFSET, &pif);
    if (pif == 0x1) {
        return ahci_init(dev);
    } else {
        return 0;
    }
}

/**
 * @brief Initialize driver
 */
int driver_init(int argc, char *argv[]) {
    pci_scan_parameters_t params = {
        .id_list = NULL,
        .class_code = 0x01,
        .subclass_code = 0x06
    };

    return pci_scanDevice(ahci_scan, &params, NULL);
}

/**
 * @brief Deinitialize driver
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "AHCI Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
