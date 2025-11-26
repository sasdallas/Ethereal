/**
 * @file drivers/storage/ahci/ahci_vfs.c
 * @brief VFS methods for AHCI
 * 
 * @c ahci_port.c was getting too complicated
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "ahci.h"
#include <kernel/mem/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>
#include <string.h>

/**
 * @brief VFS read method for AHCI device
 * @warning !!! THIS MATH IS HORRIBLE DO NOT READ !!!
 */
ssize_t ahci_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    // Make sure offset and buffer are good
    if ((uint64_t)offset >= node->length || !buffer) {
        return 0;
    }

    // Get the port from the node
    if (!node->dev) return 0;
    ahci_port_t *port = (ahci_port_t*)node->dev;
    if (port->type != AHCI_DEVICE_SATA && port->type != AHCI_DEVICE_SATAPI) return 0;

    // Make sure offset + size is good
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    // Create an LBA, rounded size, and offset
    // For an offset of 0x5794, the offset would become 0x5600, and the buffer_offset would become 0x194
    // For a size of 0x34F (which is added to buffer_offset before rounding), it would become 0x400/0x600 (depending on buffer_offset) 
    
    uint64_t lba, buffer_offset;
    size_t size_rounded;
    
    if (port->type == AHCI_DEVICE_SATAPI) {
        // ATAPI devices have different block sizes
        uint64_t blocksize = port->atapi_block_size;

        lba = (offset - (offset % blocksize)) / blocksize;
        buffer_offset = offset - (lba % blocksize);
        size_rounded = (((size + buffer_offset) + blocksize) - (((size+buffer_offset)+blocksize) % blocksize));
    } else {
        // ATA devices are fixed with a 512-byte sector size
        lba = (offset - (offset % 512)) / 512;
        buffer_offset = offset - (lba * 512);
        size_rounded = (((size+buffer_offset) + 512) - (((size+buffer_offset) + 512) % 512));
    }

    dprintf_module(DEBUG, "DRIVER:AHCI", "Calculated LBA: %d (offset: %d) size_rounded: %d\n", lba, buffer_offset, size_rounded);

    // If our rounded size is below PAGE_SIZE, we can use the DMA buffer
    uint8_t *dmabuffer = (uint8_t*)((size_rounded <= PAGE_SIZE) ? port->dma_buffer : dma_map(size_rounded));

    // Now start reading
    if (port->type == AHCI_DEVICE_SATAPI) {
        // Read in the buffer (portOperate will fwd to portOperateATAPI)
        if (ahci_portOperate(port, AHCI_READ, lba, size_rounded / port->atapi_block_size, dmabuffer) != AHCI_SUCCESS) {
            // Error
            if ((uintptr_t)dmabuffer != port->dma_buffer) dma_unmap((uintptr_t)dmabuffer, size_rounded);
            return 0; 
        }
    } else {
        // Read in the buffer
        if (ahci_portOperate(port, AHCI_READ, lba, size_rounded / 512, dmabuffer) != AHCI_SUCCESS) {
            // Error
            if ((uintptr_t)dmabuffer != port->dma_buffer) dma_unmap((uintptr_t)dmabuffer, size_rounded);
            return 0; 
        }
    }

    // Copy the buffer with offset
    memcpy(buffer, dmabuffer + buffer_offset, size);
    if ((uintptr_t)dmabuffer != port->dma_buffer) dma_unmap((uintptr_t)dmabuffer, size_rounded);
    return size;
}

/**
 * @brief VFS write method for AHCI device
 * @warning !!! HORRIBLE CODING !!!
 */
ssize_t ahci_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    // Make sure offset and buffer are good
    if ((uint64_t)offset >= node->length || !buffer) {
        return 0;
    }

    // Get the port from the node
    if (!node->dev) return 0;
    ahci_port_t *port = (ahci_port_t*)node->dev;
    if (port->type != AHCI_DEVICE_SATA && port->type != AHCI_DEVICE_SATAPI) return 0;

    // Make sure offset + size is good
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    // TODO: ATAPI writes
    if (port->type == AHCI_DEVICE_SATAPI) {
        dprintf_module(ERR, "DRIVER:AHCI", "ATAPI writes are not supported\n");
        return 0;
    }

    // Create an LBA and rounded size
    uint64_t lba = (offset - (offset % 512)) / 512;
    size_t size_rounded = ((size + 512) - ((size + 512) % 512));

    // Allocate a write buffer
    uint8_t *write_buffer = (uint8_t*)((size_rounded > PAGE_SIZE) ? dma_map(size_rounded) : port->dma_buffer);
    memset(write_buffer, 0, size_rounded);

    // !!!: We have to read in a sector at the end of offset + size rounded to prevent the driver from overwriting existing bytes
    // !!!: There seems to be a better way to do this, instead of allocating more memory
    if (ahci_portOperate(port, AHCI_READ, lba, 1, write_buffer) != AHCI_SUCCESS) {
        if (write_buffer != (uint8_t*)port->dma_buffer) dma_unmap((uintptr_t)write_buffer, size_rounded);
        return 0;
    }
    
    if (ahci_portOperate(port, AHCI_READ, ((lba + size_rounded) / 512)-1, 1, write_buffer + (size_rounded-512)) != AHCI_SUCCESS) {
        if (write_buffer != (uint8_t*)port->dma_buffer) dma_unmap((uintptr_t)write_buffer, size_rounded);
        return 0;
    }

    // Copy bytes
    memcpy(write_buffer + (offset-(lba*512)), buffer, size);

    // Write
    if (ahci_portOperate(port, AHCI_WRITE, lba, size_rounded / 512, write_buffer) != AHCI_SUCCESS) {
        if (write_buffer != (uint8_t*)port->dma_buffer) dma_unmap((uintptr_t)write_buffer, size_rounded);
        return 0;
    }

    // Done
    if (write_buffer != (uint8_t*)port->dma_buffer) dma_unmap((uintptr_t)write_buffer, size_rounded);    
    return size;
}

/**
 * @brief Allocate a new filesystem node for AHCI
 */ 
fs_node_t *ahci_createNode(ahci_port_t *port) {
    fs_node_t *ret = kmalloc(sizeof(fs_node_t));
    memset(ret, 0, sizeof(fs_node_t));

    // Setup variables
    ret->dev = (void*)port;
    ret->length = port->size;
    ret->mask = 0770;
    ret->flags = VFS_BLOCKDEVICE;

    // Set methods
    ret->read = ahci_read;
    ret->write = ahci_write;

    return ret;
}