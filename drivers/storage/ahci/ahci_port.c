/**
 * @file drivers/storage/ahci/ahci_port.c
 * @brief AHCI port
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "ahci.h"
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/fs/drivefs.h>
#include <kernel/debug.h>
#include <string.h>

// HAL
#ifdef __ARCH_I386__
#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/interrupt.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/interrupt.h>
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:AHCI", __VA_ARGS__)

/* Log port */
#define LOG_PORT(status, port, ...) LOG(status, "[PORT%d] ", port->port_num); \
                                    dprintf(NOHEADER, __VA_ARGS__)




/**
 * @brief Disable a port
 * @param port The port to disable
 * @returns 0 on success
 */
static int ahci_portDisable(ahci_port_t *port) {
    // Is port already disabled?
    if (!(port->port->cmd & HBA_PORT_PXCMD_ST)) {
        LOG_PORT(WARN, port, "Tried to disable already disabled port\n");
        return 1;
    }

    // Disable port and wait for CR to clear
    port->port->cmd &= ~(HBA_PORT_PXCMD_ST);

    // Wait for CR to clear
    int timeout = TIMEOUT(!(port->port->cmd & HBA_PORT_PXCMD_CR), 500000);
    if (timeout) {
        // DMA engine didn't stop correcty
        LOG_PORT(ERR, port, "Stopping DMA engine timed out.\n");
        return 1;
    }

    // Success
    return 0;
}

/**
 * @brief Enable a port
 * @param port The port to enable
 * @returns 0 on success
 */
static int ahci_portEnable(ahci_port_t *port) {
    if (port->port->cmd & HBA_PORT_PXCMD_ST) {
        // Port already running?
        LOG_PORT(DEBUG, port, "Port already running, cannot start\n");
        return AHCI_ERROR;
    }

    if (!(port->port->cmd & HBA_PORT_PXCMD_FRE)) {
        // To set ST we need FRE to be set
        LOG_PORT(ERR, port, "Tried to enable port but FRE is not set\n");
        return AHCI_ERROR;
    }

    // Wait until CR is clear
    int cr_clear = TIMEOUT(!(port->port->cmd & HBA_PORT_PXCMD_CR), 100000);
    if (cr_clear) {
        LOG_PORT(ERR, port, "Failed to stop DMA engine\n");
        return AHCI_ERROR;
    }

    port->port->cmd |= HBA_PORT_PXCMD_ST;
    return AHCI_SUCCESS;
}

/**
 * @brief Find an unused command header
 * @param port The port to search for an unused command header on
 * @returns Command header index or -1
 */
static int ahci_portFindUnusedHeader(ahci_port_t *port) {
    uint32_t ci = port->port->ci;
    for (int i = 0; i < 32; i++) {
        if (!(ci & (1 << i))) {
            // Found one!
            return i;
        }
    }

    return -1;
}

/**
 * @brief Dump port state
 * @param port The port to dump the state of
 */
static void ahci_dumpPortState(ahci_port_t *port) {
    LOG_PORT(DEBUG, port, "PORT DUMP STATE:\n");
    LOG_PORT(DEBUG, port, "\tIS %08x\n", port->port->is);
    LOG_PORT(DEBUG, port, "\tIE %08x\n", port->port->ie);
    LOG_PORT(DEBUG, port, "\tCMD %08x\n", port->port->cmd);
    LOG_PORT(DEBUG, port, "\tTFD %08x\n", port->port->tfd);
    LOG_PORT(DEBUG, port, "\tSIG %08x\n", port->port->sig);
    LOG_PORT(DEBUG, port, "\tSSTS %08x\n", port->port->ssts);
    LOG_PORT(DEBUG, port, "\tSCTL %08x\n", port->port->sctl);
    LOG_PORT(DEBUG, port, "\tSERR %08x\n", port->port->serr);
    LOG_PORT(DEBUG, port, "\tSACT %08x\n", port->port->sact);
    LOG_PORT(DEBUG, port, "\tCI %08x\n", port->port->ci);
    LOG_PORT(DEBUG, port, "\tSNTF %08x\n", port->port->sntf);
    LOG_PORT(DEBUG, port, "\tFBS %08x\n", port->port->fbs);

    uint64_t clb = (uint64_t)AHCI_HIGH(port->port->clbu) | (port->port->clb);
    uint64_t fb = (uint64_t)AHCI_HIGH(port->port->fbu) | (port->port->fb);

    LOG_PORT(DEBUG, port, "\tFIS base: %016llX\n", fb);
    LOG_PORT(DEBUG, port, "\tCommand list base: %016llX\n", clb);


    // Dump error
    if (port->port->serr & 0xFFFFFFFF) {
        LOG_PORT(DEBUG, port, "ERROR STATE OF PORT:\n");
        if (port->port->serr & HBA_PORT_PXSERR_X) {
            LOG_PORT(ERR, port, "\t- PxSERR: Exchanged\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_F) {
            LOG_PORT(ERR, port, "\t- PxSERR: Unknown FIS type\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_T) {
            LOG_PORT(ERR, port, "\t- PxSERR: Transport state transition error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_S) {
            LOG_PORT(ERR, port, "\t- PxSERR: Link sequence error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_H) {
            LOG_PORT(ERR, port, "\t- PxSERR: Handshake error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_C) {
            LOG_PORT(ERR, port, "\t- PxSERR: CRC error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_B) {
            LOG_PORT(ERR, port, "\t- PxSERR: 10B to 8B decode error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_W) {
            LOG_PORT(ERR, port, "\t- PxSERR: Comm wake\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_I) {
            LOG_PORT(ERR, port, "\t- PxSERR: Phy internal error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_N) {
            LOG_PORT(ERR, port, "\t- PxSERR: PhyRdy change\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_E) {
            LOG_PORT(ERR, port, "\t- PxSERR: Internal error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_P) {
            LOG_PORT(ERR, port, "\t- PxSERR: Protocol error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_C) {
            LOG_PORT(ERR, port, "\t- PxSERR: Persistent communication or data integrity error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_T) {
            LOG_PORT(ERR, port, "\t- PxSERR: Transient data integrity error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_M) {
            LOG_PORT(ERR, port, "\t- PxSERR: Recovered communications error\n");
        } else if (port->port->serr & HBA_PORT_PXSERR_ERR_I) {
            LOG_PORT(ERR, port, "\t- PxSERR: Recovered data integrity error\n");
        }

        LOG_PORT(INFO, port, "Resetting port due to PxSERR error\n");
        if (ahci_portEnable(port) != AHCI_SUCCESS) {
            LOG_PORT(ERR, port, "Failed to enable port.\n");
        }
    
        // Enable port interrupts
        port->port->ie = 0xFFFFFFFF;
    
        // Ok, now stop the port again.
        if (ahci_portDisable(port) != AHCI_SUCCESS) {
            LOG_PORT(ERR, port, "Failed to disable port\n");
        }
    }

    // Dump D2HFIS
    LOG_PORT(DEBUG, port, "D2HFIS:\n");
    LOG_PORT(DEBUG, port, "\tFIS_TYPE %02x\n", port->fis->rfis.fis_type);
    LOG_PORT(DEBUG, port, "\tPMPORT %x\n", port->fis->rfis.pmport);
    LOG_PORT(DEBUG, port, "\tINTERRUPT %i\n", port->fis->rfis.i);
    LOG_PORT(DEBUG, port, "\tSTATUS %02x\n", port->fis->rfis.status);
    LOG_PORT(DEBUG, port, "\tERROR %02x\n", port->fis->rfis.error);

    // Dump DMA setup FIS
    LOG_PORT(DEBUG, port, "DSFIS:\n");
    LOG_PORT(DEBUG, port, "\tFIS_TYPE %02x\n", port->fis->dsfis.fis_type);
    LOG_PORT(DEBUG, port, "\tTRANSFERCOUNT %08x\n", port->fis->dsfis.TransferCount);
    LOG_PORT(DEBUG, port, "\tINTERRUPT %i\n", port->fis->dsfis.i);
    LOG_PORT(DEBUG, port, "\tDMABUFOFFSET %08x\n", port->fis->dsfis.DMAbufOffset);
    LOG_PORT(DEBUG, port, "\tDMABUFID %016llx\n", port->fis->dsfis.DMAbufferID);
}

/**
 * @brief Fill the PRDT of a command header
 * @param port The port to fill the PRDT of
 * @param data The data to fill the PRDT with
 * @param size The size of the data to fill the PRDT with
 * @returns Amount of PRDs filled
 */
static int ahci_portFillPRDT(ahci_port_t *port, ahci_cmd_header_t *header, uintptr_t *data, size_t size) {
    if (!data || !size) return AHCI_ERROR;

    // Get the command table
    ahci_cmd_table_t *table = port->cmd_table;

    // Clear the command table
    memset(table, 0, sizeof(ahci_cmd_table_t));

    // Clear the PRDs in the comand table
    memset(table->prdt_entry, 0, sizeof(ahci_prdt_entry_t) * AHCI_PRDT_COUNT);

    uintptr_t *buffer = data;
    size_t remaining = size;
    int prds_filled = 0;
    for (int entry = 0; entry < AHCI_PRDT_COUNT; entry++) {
        if (remaining <= 0) break;
        AHCI_SET_ADDRESS(table->prdt_entry[entry].dba, buffer);
        
        // temp check
        if (mem_getPhysicalAddress(NULL, (uintptr_t)buffer) & 1)  {
            LOG_PORT(WARN, port, "Data not aligned properly: %p\n", buffer);
        }

        // Calculate amount of bytes
        size_t bytes = remaining;
        if (bytes > AHCI_PRD_MAX_BYTES) bytes = AHCI_PRD_MAX_BYTES;

        // Set the length of the data
        table->prdt_entry[entry].dbc = bytes - 1; 

        // Update
        remaining -= bytes;
        buffer += bytes;
        prds_filled++;
    }

    if (remaining) {
        LOG_PORT(ERR, port, "Failed to fill PRDT - too many bytes (%d bytes left to fill)\n");
        return 0;
    }

    LOG_PORT(DEBUG, port, "Filled %d PRDs\n", prds_filled);

    // All filled
    return prds_filled;
}

/**
 * @brief Wait for a transfer to complete
 * @param port The port to wait on
 * @param timeout The timeout to use
 * @returns AHCI_SUCCESS on success, anything else is a failure
 */
static int ahci_portWaitTransfer(ahci_port_t *port, int timeout, int header) {
    while (1) {
        if (!(port->port->ci & (1 << header))) {
            return AHCI_SUCCESS;
        }

        // Decrement timeout
        timeout--;
        if (timeout <= 0) break;

        // Check for errors in the IS flag
        if (port->port->is & HBA_PORT_PXIS_TFES) {
            LOG_PORT(ERR, port, "Transfer failure - dumping port state.\n");
            ahci_dumpPortState(port);
            return AHCI_ERROR;
        }

    }

    LOG_PORT(ERR, port, "Transfer failure - timeout while waiting\n");
    return AHCI_TIMEOUT;   
}

/**
 * @brief Read the identification space of a port
 * @param port The port to read the identification space of
 * @param ident Pointer to identification space (output)
 * @returns AHCI_SUCCESS on success
 */
static int ahci_readIdentificationSpace(ahci_port_t *port, ata_ident_t *ident) {
    // Get the command header
    int free_command_header = ahci_portFindUnusedHeader(port);
    if (free_command_header < 0) {
        // No free command headers
        LOG_PORT(ERR, port, "No free command headers\n");
        return AHCI_ERROR;
    }

    ahci_cmd_header_t *header = (ahci_cmd_header_t*)&port->cmd_list[free_command_header];

    // Setup header
    header->cfl = sizeof(ahci_fis_h2d_t) / sizeof(uint32_t);
    header->prdtl = ahci_portFillPRDT(port, header, (uintptr_t*)(void*)ident, sizeof(ata_ident_t));
    header->w = 0;
    header->a = 0;
    header->p = 1;

    // Create the FIS
    ahci_fis_h2d_t *fis = (ahci_fis_h2d_t*)(&(port->cmd_table->cfis));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->command = (port->type == AHCI_DEVICE_SATAPI) ? ATA_CMD_IDENTIFY_PACKET : ATA_CMD_IDENTIFY;
    fis->c = 1; // Specify that this FIS is a command
    fis->control = 0x08;
    

    // Wait for device to not be busy
    int timeout = TIMEOUT(!(port->port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)), 1000000);
    if (timeout) {
        LOG_PORT(ERR, port, "Timeout waiting for existing command to process (BSY/DRQ set)\n");
        return AHCI_ERROR;
    }

    // Send the command!
    port->port->ci = (1 << free_command_header);

    // Wait for the transfer to complete
    int transfer = ahci_portWaitTransfer(port, 10000000, free_command_header);
    if (transfer != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Failed to read drive identification space\n");
        LOG_PORT(DEBUG, port, "header->prdtl: %04x\n", header->prdtl);
        ahci_dumpPortState(port);
        return AHCI_ERROR;
    }

    // Transfer succeeded
    return AHCI_SUCCESS;
}

/**
 * @brief Read the capacity of an ATAPI AHCI drive
 * @param port The port to read the capacity of
 * @param lba The LBA to output to
 * @param block_size The block size to output to
 */
int ahci_readCapacity(ahci_port_t *port, uint32_t *lba, uint32_t *block_size) {
    if (port->type != AHCI_DEVICE_SATAPI) return AHCI_ERROR;

    // Get the command header
    int free_command_header = ahci_portFindUnusedHeader(port);
    if (free_command_header < 0) {
        // No free command headers
        LOG_PORT(ERR, port, "No free command headers\n");
        return AHCI_ERROR;
    }

    ahci_cmd_header_t *header = (ahci_cmd_header_t*)&port->cmd_list[free_command_header];

    // Construct the read capacity packet
    uint16_t read_capacity_packet[6] = { ATAPI_READ_CAPACITY, 0, 0, 0, 0, 0 };

    // Output buffer
    uint16_t capacity[8];

    // Setup header
    header->cfl = sizeof(ahci_fis_h2d_t) / sizeof(uint32_t);
    header->prdtl = ahci_portFillPRDT(port, header, (uintptr_t*)(void*)capacity, 8);
    header->w = 0;
    header->a = 1;
    header->p = 1;

    // Create the FIS
    ahci_fis_h2d_t *fis = (ahci_fis_h2d_t*)(&(port->cmd_table->cfis));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->command = ATA_CMD_PACKET;
    fis->c = 1; // Specify that this FIS is a command

    

    // Copy ATAPI comamnd
    memset((void*)port->cmd_table->acmd, 0, 16);
    memcpy((void*)port->cmd_table->acmd, read_capacity_packet, 12);

    // Wait for device to not be busy
    int timeout = TIMEOUT(!(port->port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)), 1000000);
    if (timeout) {
        LOG_PORT(ERR, port, "Timeout waiting for existing command to process (BSY/DRQ set)\n");
        return AHCI_ERROR;
    }

    // Send the command!
    port->port->ci = (1 << free_command_header);

    // Wait for the transfer to complete
    int transfer = ahci_portWaitTransfer(port, 10000000, free_command_header);
    if (transfer != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Failed to read drive capacity space\n");
        ahci_dumpPortState(port);
        return AHCI_ERROR;
    }

    LOG(DEBUG, "%04x %04x %04x %04x\n", capacity[0], capacity[1], capacity[2], capacity[3]);

    // Get LBA
    // htonl source: https://github.com/klange/toaruos/blob/master/modules/ata.c#L686
    #define htonl(l)  ( (((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
	
    // Convert LBA and block size
    uint32_t _lba, _block_size;
	memcpy(&_lba, &capacity[0], sizeof(uint32_t));
    dprintf(DEBUG, "Raw lba: %x\n", htonl(_lba));
	*lba = htonl(_lba);
    
	memcpy(&_block_size, &capacity[2], sizeof(uint32_t));
	*block_size = htonl(_block_size);
    dprintf(DEBUG, "Raw block size: %x\n", htonl(_block_size));

    // Transfer succeeded
    return AHCI_SUCCESS;
}

/**
 * @brief Initialize a port
 * @param ahci AHCI controller
 * @param port_number The number of the port to initialize
 * @returns @c ahci_port_t structure or NULL
 */
ahci_port_t *ahci_portInitialize(ahci_t *ahci, int port_number) {
    // Allocate port structure
    ahci_port_t *port = kmalloc(sizeof(ahci_port_t));
    port->port_num = port_number;
    port->parent = ahci;
    port->port = &ahci->mem->ports[port_number];

    // Calculate how much memory we need to allocate in total
    size_t memory_amount = sizeof(ahci_received_fis_t) + sizeof(ahci_cmd_table_t);
    memory_amount += (sizeof(ahci_cmd_header_t) * AHCI_CMD_HEADER_COUNT);
    memory_amount += (sizeof(ahci_prdt_entry_t) * AHCI_PRDT_COUNT);

    // Allocate DMA buffer (this will be used for small reads and writes)
    port->dma_buffer = mem_allocateDMA(PAGE_SIZE);
    
    // Now get that memory from the kernel heap
    uintptr_t port_buffer = mem_allocateDMA(memory_amount);
    memset((void*)port_buffer, 0, memory_amount);

    // Start by allocating the command list
    port->cmd_list = (ahci_cmd_header_t*)port_buffer;
    port_buffer += sizeof(ahci_cmd_header_t) * AHCI_CMD_HEADER_COUNT; // TODO: Checks for alignment

    // Allocate the FIS receive area
    port->fis = (ahci_received_fis_t*)port_buffer;
    port_buffer += sizeof(ahci_received_fis_t);

    // 128-byte align
    port_buffer += 0x80;
    port_buffer &= ~(0x7F);
    
    // Allocate the command table
    port->cmd_table = (ahci_cmd_table_t*)port_buffer;
    port_buffer += sizeof(ahci_cmd_table_t) + (sizeof(ahci_prdt_entry_t) * AHCI_PRDT_COUNT);

    // Debug
    LOG_PORT(DEBUG, port, "CMDLIST = %p FIS = %p CMDTABLE = %p\n", port->cmd_list, port->fis, port->cmd_table);
    LOG_PORT(DEBUG, port, "CMDLISTPHYS = %p FISPHYS = %p CMDTABLEPHYS = %p\n", mem_getPhysicalAddress(NULL, (uintptr_t)port->cmd_list), mem_getPhysicalAddress(NULL, (uintptr_t)port->fis), mem_getPhysicalAddress(NULL, (uintptr_t)port->cmd_table));


    // Now point AHCI registers to our structures
    AHCI_SET_ADDRESS(port->port->clb, port->cmd_list)
    AHCI_SET_ADDRESS(port->port->fb, port->fis);

    // Populate our command list
    for (int i = 0; i < AHCI_CMD_HEADER_COUNT; i++) {
        AHCI_SET_ADDRESS(port->cmd_list[i].ctba, port->cmd_table);
        port->cmd_list[i].prdtl = AHCI_PRDT_COUNT;
    }

    // Then, initialize the port.
    // Clear IRQ status bits and error register
    port->port->is = port->port->is;
    port->port->serr = port->port->serr;

    // Power up device
    port->port->cmd |= HBA_PORT_PXCMD_POD;
    
    // Spin up
    port->port->cmd |= HBA_PORT_PXCMD_SUD;
    
    // ICC active
    port->port->cmd = (port->port->cmd & ~HBA_PORT_PXCMD_ICC) | (1 << 28); // Set 28th bit = ICC active

    // AHCI spec thinks it best that we set FRE now that FB/FBU points to our Receive FIS
    port->port->cmd |= HBA_PORT_PXCMD_FRE;

    // Can't start the port yet, need to enable interrupts and do some more stuff
    // But first the HBA needs to enable its interrupts after probing.
    return port;
}


/**
 * @brief Finish port initialization
 * @param port The port to finish initializing
 * @returns AHCI_SUCCESS on device found, AHCI_NO_DEVICE on device not found, AHCI_ERROR on error
 */
int ahci_portFinishInitialization(ahci_port_t *port) {
    LOG_PORT(DEBUG, port, "Finishing port initialization\n");

    if (ahci_portEnable(port) != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Failed to enable port.\n");
        return AHCI_ERROR;
    }

    // Enable port interrupts
    port->port->ie = 0xFFFFFFFF;

    // Ok, now stop the port again.
    if (ahci_portDisable(port) != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Failed to disable port\n");
        return AHCI_ERROR;
    }

    // Wait for BSY and DRQ to clear.
    int timeout = TIMEOUT(!(port->port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)), 1000000);
    if (timeout) {
        // AHCI specification says that if this fails to try a COMRESET
        LOG_PORT(INFO, port, "Timeout detected, performing COMRESET\n");

        // Invoke COMRESET (clears other bits)
        port->port->sctl = (HBA_PORT_SCTL_DET_DISABLE | HBA_PORT_SCTL_IPM_PARTIAL | HBA_PORT_SCTL_IPM_SLUMBER);

        // Wait 20ms (spec says 1ms)
        clock_sleep(1);

        // Clear DET
        port->port->sctl = (port->port->sctl & ~HBA_PORT_PXSCTL_DET) | HBA_PORT_SCTL_DET_NONE;
    }

    // Enable
    if (ahci_portEnable(port)) {
        LOG_PORT(ERR, port, "Failed to enable port\n");
        return AHCI_ERROR;
    }

    // Wait for DET to be 3 (HBA_PORT_SSTS_DET_PRESENT)
    timeout = TIMEOUT((port->port->ssts & HBA_PORT_SSTS_DET_PRESENT), 5000);
    if (timeout) {
        LOG_PORT(INFO, port, "No device present on port\n");
        port->type = AHCI_DEVICE_NONE;
        return AHCI_SUCCESS;
    }

    // Now we need to get the type of the device
    switch (port->port->sig) {
        case SATA_SIG_ATA:
            LOG_PORT(DEBUG, port, "Detected a SATA device on port\n");
            port->type = AHCI_DEVICE_SATA;
            break;
        
        case SATA_SIG_ATAPI:
            LOG_PORT(DEBUG, port, "Detected a SATAPI device on port\n");
            port->type = AHCI_DEVICE_SATAPI;
            break;
        
        case SATA_SIG_SEMB:
            LOG_PORT(DEBUG, port, "Detected an enclosure management bridge on port\n");
            port->type = AHCI_DEVICE_SEMB;
            break;
        
        case SATA_SIG_PM:
            LOG_PORT(DEBUG, port, "Detected a port multiplier on port\n");
            port->type = AHCI_DEVICE_PM;
            break;
    }

    if (port->type != AHCI_DEVICE_SATA && port->type != AHCI_DEVICE_SATAPI) {
        // Done here
        LOG_PORT(INFO, port, "Port initialized successfully\n");
        return AHCI_SUCCESS;
    }

    // If the device is ATAPI, we should also set the command bit
    if (port->type == AHCI_DEVICE_SATAPI) {
        port->port->cmd |= HBA_PORT_PXCMD_ATAPI;
    } else {
        port->port->cmd &= ~(HBA_PORT_PXCMD_ATAPI);
    }

    // Now we should read the drive information, e.g. sector size and count
    // Allocate identification space
    port->ident = kmalloc(sizeof(ata_ident_t));
    memset(port->ident, 0, sizeof(ata_ident_t));

    // Read identification space in
    if (ahci_readIdentificationSpace(port, port->ident) != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Failed to read identification space.\n");
        return AHCI_ERROR;
    }

    // Reorder bytes
    ATA_REORDER_BYTES(port->ident->model, 40);
    ATA_REORDER_BYTES(port->ident->serial, 20);
    ATA_REORDER_BYTES(port->ident->firmware, 8);

    // Null-terminate
    port->ident->model[39] = 0;
    port->ident->serial[19] = 0;
    port->ident->firmware[7] = 0;
    LOG_PORT(DEBUG, port, "Model %s - serial %s firmware %s\n", port->ident->model, port->ident->serial, port->ident->firmware);

    // Now we need to get the capacities of each drive on the port
    if (port->type == AHCI_DEVICE_SATA) {
        // SATA drives have this embedded in the identification space
        // Check the device's addressing type
        if ((port->ident->command_sets & (1 << 26))) {
            // LBA48 addressing
            port->size = (port->ident->sectors_lba48 & 0x0000FFFFFFFFFFFF) * 512;
        } else {
            // CHS or LBA28 addressing
            port->size = port->ident->sectors * 512;
        }

        LOG_PORT(DEBUG, port, "Capacity: %d MB\n", (port->size) / 1024 / 1024);
    } else {
        // TODO: ATAPI, fix capacity reading
        LOG_PORT(ERR, port, "ATAPI devices are currently unsupported by the AHCI controller\n");

        uint32_t lba, block_size;
        if (ahci_readCapacity(port, &lba, &block_size) != AHCI_SUCCESS) {
            LOG_PORT(ERR, port, "Failed to read capacity\n");
            return AHCI_ERROR;
        }

        port->atapi_block_size = block_size;
        port->size = (lba + 1) * block_size;

        if (port->atapi_block_size == 0) {
            LOG_PORT(ERR, port, "Invalid block size. No medium present? This is probably a bug.\n");
            return AHCI_ERROR;
        }

        LOG_PORT(DEBUG, port, "Capacity: %d MB\n", port->size / 1024 / 1024);

        return AHCI_SUCCESS; // Return success but do not allow usage
    }

    // Create node
    fs_node_t *node = ahci_createNode(port);
    
    // Mount node
    if (port->type == AHCI_DEVICE_SATA) {
        drive_mount(node, DRIVE_TYPE_IDE_HD);
    } else {
        drive_mount(node, DRIVE_TYPE_CDROM);
    }

    // All done! Port should be initialized
    return AHCI_SUCCESS;
}

/**
 * @brief Execute a request for a specific port (ATAPI)
 * @param port The port to execute the request for
 * @param operation The operation to do
 * @param lba The LBA
 * @param sectors The amount of sectors to operate on
 * @param buffer The buffer to use (assume DMA)
 * @returns Error code
 */
int ahci_portOperateATAPI(ahci_port_t *port, int operation, uint64_t lba, size_t sectors, uint8_t *buffer) {
    if (port->type == AHCI_DEVICE_SATA) return ahci_portOperate(port, operation, lba, sectors, buffer);
    else if (port->type != AHCI_DEVICE_SATAPI) return AHCI_ERROR;

    // TODO

    return AHCI_ERROR;
}

/**
 * @brief Execute a request for a specific port
 * @param port The port to execute the request for
 * @param operation The operation to do
 * @param lba The LBA
 * @param sectors The amount of sectors to operate on
 * @param buffer The buffer to use (assume DMA)
 * @returns Error code
 */
int ahci_portOperate(ahci_port_t *port, int operation, uint64_t lba, size_t sectors, uint8_t *buffer) {
    if (!port || !sectors) return AHCI_ERROR;
    if (port->type == AHCI_DEVICE_SATAPI) return ahci_portOperateATAPI(port, operation, lba, sectors, buffer);
    else if (port->type != AHCI_DEVICE_SATA) return AHCI_ERROR;

    // First we need to construct the AHCI request
    // Find a free command slot
    int free_header = ahci_portFindUnusedHeader(port);
    if (free_header < 0) {
        LOG_PORT(ERR, port, "No free command headers found.\n");
        return AHCI_ERROR;
    }

    // Construct header
    ahci_cmd_header_t *header = (ahci_cmd_header_t*)&port->cmd_list[free_header];
    header->cfl = sizeof(ahci_fis_h2d_t) / sizeof(uint32_t);
    header->prdtl = ahci_portFillPRDT(port, header, (uintptr_t*)buffer, sectors * 512);
    header->a = 0;
    header->w = (operation == AHCI_WRITE);
    header->p = 1;

    // Create the FIS
    ahci_fis_h2d_t *fis = (ahci_fis_h2d_t*)(&(port->cmd_table->cfis));
    memset(fis, 0, sizeof(ahci_fis_h2d_t));

    // Setup FIS variables
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;

    // Create LBA data
    // Decide what type of LBA to use
    int lba48 = 0;
    uint8_t lba_data[6] = { 0 };
    if (lba >= 0x10000000) {
        // LBA48 addressing
        if (!(port->ident->command_sets & (1 << 26))) {
            // Device does not support LBA48
            LOG_PORT(ERR, port, "Attempted to read LBA 0x%llX but drive does not support 48-bit LBA\n", lba);
            return AHCI_ERROR;
        }

        lba48 = 1;
        lba_data[3] = (lba & 0x00000000FF000000) >> 24;
        lba_data[4] = (lba & 0x000000FF00000000) >> 32;
        lba_data[5] = (lba & 0x0000FF0000000000) >> 40;
    }

    // Setup the remaining values (or the only ones for LBA28)
    lba_data[0] = (lba & 0x00000000000000FF) >> 0;
    lba_data[1] = (lba & 0x000000000000FF00) >> 8;
    lba_data[2] = (lba & 0x0000000000FF0000) >> 16;

    // Write LBA data to FIS
    fis->lba0 = lba_data[0];
    fis->lba1 = lba_data[1];
    fis->lba2 = lba_data[2];
    fis->lba3 = lba_data[3];
    fis->lba4 = lba_data[4];
    fis->lba5 = lba_data[5];
    fis->device = (1 << 6); // Set LBA bit

    // Set count
    fis->countl = (sectors & 0xFF);
    fis->counth = (sectors >> 8) & 0xFF;

    // Choose command
    if (operation == AHCI_READ && lba48 == 0) fis->command = ATA_CMD_READ_DMA;
    if (operation == AHCI_READ && lba48 == 1) fis->command = ATA_CMD_READ_DMA_EXT;
    if (operation == AHCI_WRITE && lba48 == 0) fis->command = ATA_CMD_WRITE_DMA;
    if (operation == AHCI_WRITE && lba48 == 1) fis->command = ATA_CMD_WRITE_DMA_EXT;

    // Wait for port to not be busy
    // Wait for device to not be busy
    int timeout = TIMEOUT(!(port->port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)), 1000000);
    if (timeout) {
        LOG_PORT(ERR, port, "Timeout waiting for existing command to process (BSY/DRQ set)\n");
        return AHCI_ERROR;
    }

    // Send the command!
    port->port->ci = (1 << free_header);

    // Wait for the transfer to complete
    int transfer = ahci_portWaitTransfer(port, 10000000, free_header);
    if (transfer != AHCI_SUCCESS) {
        LOG_PORT(ERR, port, "Received status code %d while waiting for transfer - failed to %s LBA 0x%llX with %d sectors (LBA48: %i, CMD: 0x%x)\n", transfer, (operation == AHCI_READ) ? "read" : "write", lba, sectors, lba48, fis->command);
        return AHCI_ERROR;
    }

    // All done!
    return AHCI_SUCCESS;
}

/**
 * @brief Handle a port IRQ
 * @param port The port
 */
void ahci_portIRQ(ahci_port_t *port) {
    // Clear port interrupts
    uint32_t is = port->port->is;
    port->port->is = is;


    // Is there an error?
    if (port->port->is & 0x7F800000) {
        // Yeah, figure out which it is
        LOG_PORT(ERR, port, "Detected an error on port\n");

        // Clear SERR
        uint32_t serr = port->port->serr;
        port->port->serr = serr;

        // TODO: Reset port on error (IMPORTANT)
        if (port->port->is & HBA_PORT_PXIS_TFES) {
            LOG_PORT(ERR, port, "Port detected task file error\n");
        }
        
        if (port->port->is & HBA_PORT_PXIS_HBFS) {
            LOG_PORT(ERR, port, "Port detected host bus fatal error\n");
        }

        if (port->port->is & HBA_PORT_PXIS_HBDS) {
            LOG_PORT(ERR, port, "Port detected host bus data error\n");
        }

        if (port->port->is & HBA_PORT_PXIS_IFS)  {
            LOG_PORT(ERR, port, "Port detected interface fatal error\n");
        }

        if (port->port->is & HBA_PORT_PXIS_INFS)  {
            LOG_PORT(ERR, port, "Port detected interface non-fatal error\n");
        }

        if (port->port->is & HBA_PORT_PXIS_OFS)  {
            LOG_PORT(ERR, port, "Port detected overflow error\n");
        }

        if (port->port->is & HBA_PORT_PXIS_IPMS)  {
            LOG_PORT(ERR, port, "Port detected invalid port multiplier\n");
        }
    }
}

