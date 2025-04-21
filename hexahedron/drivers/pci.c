/**
 * @file hexahedron/drivers/x86/pci.c
 * @brief PCI driver for x86
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/pci.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>


#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "PCI", __VA_ARGS__)

/**
 * @brief Read a specific offset from the PCI configuration space
 * 
 * Uses configuration space access mechanism #1.
 * List of offsets is header-specific except for general header layout, see pci.h
 * 
 * @param bus The bus of the PCI device to read from
 * @param slot The slot of the PCI device to read from
 * @param func The function of the PCI device to read (if the device supports multiple functions)
 * @param offset The offset to read from
 * @param size The size of the value you want to read from. Do note that you'll have to typecast to this (max uint32_t).
 * 
 * @returns Either PCI_NONE if an invalid size was specified, or a value according to @c size
 */
uint32_t pci_readConfigOffset(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, int size) {
    if (size != 1 && size != 2 && size != 4) return PCI_NONE;
    
    // Generate the address
    uint32_t address = PCI_ADDR(bus, slot, func, offset);

    // Write it to PCI_CONFIG_ADDRESS
    outportl(PCI_CONFIG_ADDRESS, address);

    // Read what came out
    uint32_t out = inportl(PCI_CONFIG_DATA);

    // Depending on size, handle it
    if (size == 1) {
        return (out >> ((offset & 3) * 8)) & 0xFF;
    } else if (size == 2) {
        return (out >> ((offset & 2) * 8)) & 0xFFFF;
    } else {
        return out;
    }
}

/**
 * @brief Write to a specific offset in the PCI configuration space
 * 
 * @param bus The bus of the PCI device to write to
 * @param slot The slot of the PCI device to write to
 * @param func The function of the PCI device to write (if the device supports multiple functions)
 * @param offset The offset to write to
 * @param value The value to write
 * 
 * @returns 0 on success
 */
int pci_writeConfigOffset(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    // Generate the address
    uint32_t address = PCI_ADDR(bus, slot, func, offset);
    
    // Write it to PCI_CONFIG_ADDRESS
    outportl(PCI_CONFIG_ADDRESS, address);

    // Write value
    outportl(PCI_CONFIG_DATA, value);

    // Done
    return 0;
}


/**
 * @brief Auto-determine a BAR type and read it using the configuration space
 * 
 * Returns a pointer to an ALLOCATED @c pci_bar_t structure. You MUST free this structure
 * when you're finished with it!
 * 
 * @param bus Bus of the PCI device
 * @param slot Slot of the PCI device
 * @param func Function of the PCI device
 * @param bar The number of the BAR to read
 * 
 * @returns A @c pci_bar_t structure or NULL  
 */
pci_bar_t *pci_readBAR(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar) {
    // First, we should get the header type
    uint8_t header_type = pci_readConfigOffset(bus, slot, func, PCI_HEADER_TYPE_OFFSET, 1) & PCI_HEADER_TYPE;
    
    // Make sure it's valid
    if (header_type != PCI_HEADER_TYPE_GENERAL && header_type != PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE) {
        LOG(DEBUG, "Invalid or unsupported header type while reading BAR: 0x%x\n", pci_readConfigOffset(bus, slot, func, PCI_HEADER_TYPE_OFFSET, 1));
        return NULL; // Invalid device
    }

    // Check the limits of the BAR for the header type
    if (bar > 5 || (header_type == PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE && bar > 1)) {
        return NULL; // Invalid BAR
    }

    // BARs are defined as having the same base offset across our two supported header types
    // So all we have to do to calculate the existing address is PCI_GENERAL_BAR0_OFFSET + (bar * 4)
    uint8_t offset = PCI_GENERAL_BAR0_OFFSET + (bar * 0x4);
    
    // Now go ahead and disable I/O and memory access in the command register (we'll restore it at the end)
    uint32_t restore_command = pci_readConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, 4);
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command & ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE));

    // Read in the BAR
    uint32_t bar_address = pci_readConfigOffset(bus, slot, func, offset, 4);

    // Read in the BAR size by writing all 1s (seeing which bits we can set)
    pci_writeConfigOffset(bus, slot, func, offset, 0xFFFFFFFF);
    uint32_t bar_size = pci_readConfigOffset(bus, slot, func, offset, 4);
    pci_writeConfigOffset(bus, slot, func, offset, bar_address);

    // Now we just need to parse it. Allocate memory for the response  
    pci_bar_t *bar_out = kmalloc(sizeof(pci_bar_t));

    // Switch and parse.
    // PCI_BAR_MEMORY16 is currently unsupported, but PCI_BAR_MEMORY64 and PCI_BAR_MEMORY16 are part of the same type field.
    if (bar_address & PCI_BAR_MEMORY64 && !(bar_address & PCI_BAR_MEMORY16)) {
        // NOTE: This hasn't been tested yet. I think it might work okay but I'm not sure.
        // NOTE: If you have any way of testing this, please let me know :)

        // This is a 64-bit memory space BAR
        bar_out->type = PCI_BAR_MEMORY64;

        // Read the rest of the address
        uint32_t bar_address_high = pci_readConfigOffset(bus, slot, func, offset + 1, 4);
        
        // And the rest of the size
        pci_writeConfigOffset(bus, slot, func, offset + 1, 0xFFFFFFFF);
        uint32_t bar_size_high = pci_readConfigOffset(bus, slot, func, offset + 1, 4);
        pci_writeConfigOffset(bus, slot, func, offset + 1, bar_address_high);

        // Now put the values in
        bar_out->address = (bar_address & 0xFFFFFFF0) | ((uint64_t)(bar_address_high & 0xFFFFFFFF) << 32);
        bar_out->size = ~((((uint64_t)bar_size_high << 32)) | (bar_size & ~0xF)) + 1;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    } else if (bar_address & PCI_BAR_IO_SPACE) {
        // This is an I/O space BAR
        bar_out->type = PCI_BAR_IO_SPACE;
        bar_out->address = (bar_address & 0xFFFFFFFC);
        bar_out->size = ~(bar_size & ~0x3) + 1;
        bar_out->prefetchable = 0;
    } else if (bar_address & PCI_BAR_MEMORY16) {
        // This is a 16-bit memory space BAR (unsupported)
        LOG(ERR, "Unimplemented support for 16-bit BARs!!!\n");
        pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command);
        kfree(bar_out);
        return NULL;
    } else {
        // This is a 32-bit memory space BAR
        bar_out->address = bar_address & 0xFFFFFFF0;
        bar_out->size = ~(bar_size & ~0xF) + 1;
        bar_out->type = PCI_BAR_MEMORY32;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    }

    // Restore BAR
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command);
    
    // Done!
    return bar_out;
}

/**
 * @brief PCI callback for when a hit was found on the device
 * 
 * @param callback The callback function
 * @param data User-specified data
 * @param type The type of the device as requested
 * @param bus The bus of the device
 * @param slot The slot of the device
 * @param function The function of the device
 * 
 * @returns Whatever the callback function returns
 */
static int pci_scanHit(pci_callback_t callback, void *data, int type, uint8_t bus, uint8_t slot, uint8_t function) {
    // Read vendor and device ID
    uint16_t vendor_id = pci_readVendorID(bus, slot, function);
    uint16_t device_id = pci_readDeviceID(bus, slot, function);

    // Call
    return callback(bus, slot, function, vendor_id, device_id, data);
}

/**
 * @brief Scan and find a PCI device on a specific function
 * 
 * This is required because of @c PCI_TYPE_BRIDGE - also a remnant of the Ethereal Operating System scanner
 * 
 * @param callback The callback to use
 * @param data Any user data to pass to callback
 * @param type The type of the device. Set to -1 to ignore type field
 * @param bus The bus to use
 * @param slot The slot of the bus
 * @param func The function of the device
 * 
 * @returns 0 on failure, 1 on successfully found
 */
int pci_scanFunction(pci_callback_t callback, void *data, int type, uint8_t bus, uint8_t slot, uint8_t func) {
    if (type == -1 || type == pci_readType(bus, slot, func)) {
        // Found it
        return pci_scanHit(callback, data, type, bus, slot, func);
    }

    if (pci_readType(bus, slot, func) == PCI_TYPE_BRIDGE) {
        // It's a bridge, do more work
        // TODO: Add header definitions for type 0x1, we're using a hardcoded PCI_SECONDARY_BUS_OFFSET (0x19) 
        return pci_scanBus(callback, data, type, pci_readConfigOffset(bus, slot, func, 0x19, 1));
    }

    return 0;
}

/**
 * @brief Scan and find a PCI device on a certain slot
 * 
 * @param callback The callback to use
 * @param data Any user data to pass to callback
 * @param type The type of the device. Set to -1 to ignore type field
 * @param bus The bus to use
 * @param slot The slot of the bus
 * 
 * @returns 0 on failure, 1 on successfully found
 */
int pci_scanSlot(pci_callback_t callback, void *data, int type, uint8_t bus, uint8_t slot) {
    // Does the device even exist?
    if ((uint16_t)pci_readConfigOffset(bus, slot, 0, PCI_VENID_OFFSET, 2) == PCI_NONE) return 0;

    // First make sure this even supports multi-function
    uint8_t htype = (uint8_t)pci_readConfigOffset(bus, slot, 0, PCI_HEADER_TYPE_OFFSET, 1);
    if (!(htype & PCI_HEADER_TYPE_MULTIFUNCTION)) {
        // Use function 0
        return pci_scanFunction(callback, data, type, bus, slot, 0);
    }

    // It does, scan each one.
    for (uint32_t func = 0; func < PCI_MAX_FUNC; func++) {
        // Does this one exist?
        if (pci_readConfigOffset(bus, slot, func, PCI_VENID_OFFSET, 2) != PCI_NONE) {
            int ret = pci_scanFunction(callback, data, type, bus, slot, func);
            if (ret) return 1; // Found it!
        }
    }

    return 0;
}

/**
 * @brief Scan and find a PCI device on a certain bus
 * 
 * @param callback The callback to use
 * @param data Any user data to pass to callback
 * @param type The type of the device. Set to -1 to ignore type field
 * @param bus The bus to check
 * 
 * @returns 0 on failure, 1 on successfully found
 */
int pci_scanBus(pci_callback_t callback, void *data, int type, uint8_t bus) {
    // We don't need to check the validity of each as pci_scanSlot does that
    for (uint32_t slot = 0; slot < PCI_MAX_SLOT; slot++) {
        if (pci_scanSlot(callback, data, type, bus, slot)) return 1;
    }

    return 0;
}

/**
 * @brief Scan and find a PCI device. Calls a callback function that can be used to determine the device more closely.
 * 
 * @see pci_callback_t for params/return value.
 * 
 * @param callback The callback function to call. 
 * @param data Any user data to pass to callback.
 * @param type The type of the device. Set to -1 to ignore type field.
 * 
 * @returns 0 on failure, 1 on successfully found
 */
int pci_scan(pci_callback_t callback, void *data, int type) {
    // Start scanning
    for (uint32_t bus = 0; bus < PCI_MAX_BUS; bus++) {
        int r = pci_scanBus(callback, data, type, bus);
        if (r) return r;
    }
    
    return 0;
}

/**
 * @brief Read the type of the PCI device (class code + subclass)
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the type
 */
uint16_t pci_readType(uint8_t bus, uint8_t slot, uint8_t func) {
    return (pci_readConfigOffset(bus, slot, func, PCI_CLASSCODE_OFFSET, 1) << 8) | pci_readConfigOffset(bus, slot, func, PCI_SUBCLASS_OFFSET, 1);
}

/**
 * @brief Read the vendor ID of a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the vendor ID
 */
uint16_t pci_readVendorID(uint8_t bus, uint8_t slot, uint8_t func) {
    return pci_readConfigOffset(bus, slot, func, PCI_VENID_OFFSET, 2);
}

/**
 * @brief Read the device ID of a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the device ID
 */
uint16_t pci_readDeviceID(uint8_t bus, uint8_t slot, uint8_t func) {
    return pci_readConfigOffset(bus, slot, func, PCI_DEVID_OFFSET, 2);
}

/**
 * @brief Get the interrupt registered to a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the interrupt ID
 */
uint8_t pci_getInterrupt(uint8_t bus, uint8_t slot, uint8_t func) {
    // TODO: Make sure header type is 1?
    return pci_readConfigOffset(bus, slot, func, PCI_GENERAL_INTERRUPT_OFFSET, 1);
}