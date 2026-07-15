/**
 * @file hexahedron/drivers/pci.c
 * @brief PCI driver
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
#include <kernel/subsystems/irq.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <kernel/arch/arch.h>
#include <kernel/init.h>
#include <assert.h>
#include <string.h>
#include <kernel/misc/args.h>

#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/x86/local_apic.h>
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "PCI", __VA_ARGS__)

/* PCI bus array */
pci_bus_t pci_bus_list[PCI_MAX_BUS];

/* Macro to get PCI devices */
#define PCI_DEVICE(bus, slot, func) (&(pci_bus_list[bus].slots[slot].functions[func]))

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

#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)

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

#else
    return PCI_NONE;
#endif
}

/**
 * @brief Write to a specific offset in the PCI configuration space
 * 
 * @param bus The bus of the PCI device to write to
 * @param slot The slot of the PCI device to write to
 * @param func The function of the PCI device to write (if the device supports multiple functions)
 * @param offset The offset to write to
 * @param value The value to write
 * @param size How big of a value to write
 * 
 * @returns 0 on success
 */
int pci_writeConfigOffset(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value, int size) {
#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
    // Generate the address
    uint32_t address = PCI_ADDR(bus, slot, func, offset);

    uint32_t value_fixed = value;
    if (size == 1) {
        value_fixed = pci_readConfigOffset(bus, slot, func, offset & ~0x3, 4);
        
        uint32_t b_offset = (offset & 3) * 8;
        value_fixed &= ~(0xFF << b_offset);
        value_fixed |= (uint32_t)value << b_offset;

        // Redo address
        address = PCI_ADDR(bus, slot, func, (offset & ~3));
    } else if (size == 2) {
        value_fixed = pci_readConfigOffset(bus, slot, func, offset & ~0x3, 4);
        
        uint32_t b_offset = (offset & 3) * 8;
        value_fixed &= ~(0xFFFF << b_offset);
        value_fixed |= (uint32_t)value << b_offset;

        // Redo address
        address = PCI_ADDR(bus, slot, func, (offset & ~3));
    }

    // Write it to PCI_CONFIG_ADDRESS
    outportl(PCI_CONFIG_ADDRESS, address);

    // Write value
    outportl(PCI_CONFIG_DATA, value_fixed);
#endif

    // Done
    return 0;
}

/**
 * @brief Read config of device
 * @param dev The device to read the config of
 * @param offset Offset to read at
 * @param output Output to store
 * @param size The size to read in bytes
 */
int pci_readConfig(pci_device_t *dev, uint8_t offset, uint32_t *output, int size) {
    // Generate the address
    uint32_t address = PCI_ADDR(dev->bus, dev->slot, dev->function, offset);

    // Write it to PCI_CONFIG_ADDRESS
    outportl(PCI_CONFIG_ADDRESS, address);

    // Read what came out
    uint32_t out = inportl(PCI_CONFIG_DATA);

    // Depending on size, handle it
    if (size == 1) {
        *(uint8_t*)output = (out >> ((offset & 3) * 8)) & 0xFF;
    } else if (size == 2) {
        *(uint16_t*)output = (out >> ((offset & 2) * 8)) & 0xFFFF;
    } else {
        *(uint32_t*)output = out;
    }

    return 0;
}

/**
 * @brief Write config of device
 * @param dev The device to read the config of
 * @param offset The offset to write at
 * @param value The value to write
 * @param size The size of the value to write
 */
int pci_writeConfig(pci_device_t *dev, uint8_t offset, uint32_t value, int size) {
    // Generate the address
    uint32_t address = PCI_ADDR(dev->bus, dev->slot, dev->function, offset);

    uint32_t value_fixed = value;
    if (size == 1) {
        pci_readConfig(dev, offset & ~0x3, &value_fixed, 4);
        
        uint32_t b_offset = (offset & 3) * 8;
        value_fixed &= ~(0xFF << b_offset);
        value_fixed |= (uint32_t)value << b_offset;

        // Redo address
        address = PCI_ADDR(dev->bus, dev->slot, dev->function, (offset & ~3));
    } else if (size == 2) {
        pci_readConfig(dev, offset & ~0x3, &value_fixed, 4);
        
        uint32_t b_offset = (offset & 3) * 8;
        value_fixed &= ~(0xFFFF << b_offset);
        value_fixed |= (uint32_t)value << b_offset;

        // Redo address
        address = PCI_ADDR(dev->bus, dev->slot, dev->function, (offset & ~3));
    }

    // Write it to PCI_CONFIG_ADDRESS
    outportl(PCI_CONFIG_ADDRESS, address);

    // Write value
    outportl(PCI_CONFIG_DATA, value_fixed);
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
    uint32_t restore_command = pci_readConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, 2);
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command & ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE), 2);

    // Read in the BAR
    uint32_t bar_address = pci_readConfigOffset(bus, slot, func, offset, 4);

    // Read in the BAR size by writing all 1s (seeing which bits we can set)
    pci_writeConfigOffset(bus, slot, func, offset, 0xFFFFFFFF, 4);
    uint32_t bar_size = pci_readConfigOffset(bus, slot, func, offset, 4);
    bar_size = ~bar_size + 1;
    pci_writeConfigOffset(bus, slot, func, offset, bar_address, 4);

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
        uint32_t bar_address_high = pci_readConfigOffset(bus, slot, func, offset + 4, 4);
    
        // Now put the values in
        bar_out->address = (bar_address & 0xFFFFFFF0) | ((uint64_t)(bar_address_high & 0xFFFFFFFF) << 32);
        bar_out->size = bar_size;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    } else if (bar_address & PCI_BAR_IO_SPACE) {
        // This is an I/O space BAR
        bar_out->type = PCI_BAR_IO_SPACE;
        bar_out->address = (bar_address & 0xFFFFFFFC);
        bar_out->size = bar_size;
        bar_out->prefetchable = 0;
    } else if (bar_address & PCI_BAR_MEMORY16) {
        // This is a 16-bit memory space BAR (unsupported)
        LOG(ERR, "Unimplemented support for 16-bit BARs!!!\n");
        pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command, 2);
        kfree(bar_out);
        return NULL;
    } else {
        // This is a 32-bit memory space BAR
        bar_out->address = bar_address & 0xFFFFFFF0;
        bar_out->size = bar_size;
        bar_out->type = PCI_BAR_MEMORY32;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    }

    // Restore BAR
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, restore_command, 2);
    
    // Done!
    return bar_out;
}


/**
 * @brief Get a BAR structure
 * @param dev The device to get
 * @param idx The index of the BAR to get
 * @returns A BAR structure, or NULL
 * 
 * Do note that if you choose to not map the BAR it will be read again, as only
 * memory-mapped BARs are stored in a PCI device.
 */
pci_bar_t *pci_getBAR(pci_device_t *dev, uint8_t bar) {
    assert(bar <= 5);
    if (dev->bar[bar].valid) {
        return &dev->bar[bar];
    }

    // First, we should get the header type
    uint8_t header_type = 0xFF;
    pci_readConfigByte(dev, PCI_HEADER_TYPE_OFFSET, &header_type);
    header_type &= PCI_HEADER_TYPE;

    // Make sure it's valid
    if (header_type != PCI_HEADER_TYPE_GENERAL && header_type != PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE) {
        LOG(DEBUG, "Invalid or unsupported header type while reading BAR: 0x%x\n", header_type);
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
    uint16_t restore_command;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &restore_command);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, restore_command & ~(PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE));

    // Read in the BAR
    uint32_t bar_address;
    pci_readConfigDword(dev, offset, &bar_address);

    // Read in the BAR size by writing all 1s (seeing which bits we can set)
    pci_writeConfigDword(dev, offset, 0xFFFFFFFF);
    
    uint32_t bar_size;
    pci_readConfigDword(dev, offset, &bar_size);

    bar_size = ~bar_size + 1;
    pci_writeConfigDword(dev, offset, bar_address);

    // Now we just need to parse it. Allocate memory for the response  
    pci_bar_t *bar_out = &dev->bar[bar];

    // Switch and parse.
    // PCI_BAR_MEMORY16 is currently unsupported, but PCI_BAR_MEMORY64 and PCI_BAR_MEMORY16 are part of the same type field.
    if (bar_address & PCI_BAR_MEMORY64 && !(bar_address & PCI_BAR_MEMORY16)) {
        // NOTE: This hasn't been tested yet. I think it might work okay but I'm not sure.
        // NOTE: If you have any way of testing this, please let me know :)

        // This is a 64-bit memory space BAR
        bar_out->type = PCI_BAR_MEMORY64;

        // Read the rest of the address
        uint32_t bar_address_high;
        pci_readConfigDword(dev, offset + 4, &bar_address_high);
    
        // Now put the values in
        bar_out->address = (bar_address & 0xFFFFFFF0) | ((uint64_t)(bar_address_high & 0xFFFFFFFF) << 32);
        bar_out->size = bar_size;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    } else if (bar_address & PCI_BAR_IO_SPACE) {
        // This is an I/O space BAR
        bar_out->type = PCI_BAR_IO_SPACE;
        bar_out->address = (bar_address & 0xFFFFFFFC);
        bar_out->size = bar_size;
        bar_out->prefetchable = 0;
    } else if (bar_address & PCI_BAR_MEMORY16) {
        // This is a 16-bit memory space BAR (unsupported)
        LOG(ERR, "Unimplemented support for 16-bit BARs!!!\n");
        assert(0);
    } else {
        // This is a 32-bit memory space BAR
        bar_out->address = bar_address & 0xFFFFFFF0;
        bar_out->size = bar_size;
        bar_out->type = PCI_BAR_MEMORY32;
        bar_out->prefetchable = (bar_address & 0x8) ? 1 : 0;
    }

    bar_out->valid = true;

    if (bar_out->type != PCI_BAR_IO_SPACE) {
        // Map the memory
        bar_out->mapped = mmio_map(bar_out->address, bar_out->size);
    }

    // Restore BAR
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, restore_command);
    
    // Done!
    return bar_out;
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


/* NOTE: The below will be rewritten once the Hexahedron IRQ handler rewrite is finished */

/**
 * @brief Disable MSI for a device
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * @returns 0 on success
 */
int pci_disableMSI(uint8_t bus, uint8_t slot, uint8_t func) {
    // Get a pointer to the capability list
    uint8_t cap_list_off = pci_readConfigOffset(bus, slot, func, PCI_GENERAL_CAPABILITIES_OFFSET, 1);

    // Start parsing
    while (cap_list_off) {
        uint16_t cap = pci_readConfigOffset(bus, slot, func, cap_list_off, 2);
        if ((cap & 0xFF) == 0x05) {
            break;
        }

        cap_list_off = (cap >> 8) & 0xFC;
    }

    if (!cap_list_off) {
        return 0; // No MSI support
    }

    // Disable MSI
    uint16_t ctrl = pci_readConfigOffset(bus, slot, func, cap_list_off + 0x02, 2);
    ctrl &= ~(1 << 0);
    pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x02, ctrl, 2);
    return 0;
}

/**
 * @brief Disable MSI-X for a device
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * @returns 0 on success
 */
int pci_disableMSIX(uint8_t bus, uint8_t slot, uint8_t func) {
    // Get a pointer to the capability list
    uint8_t cap_list_off = pci_readConfigOffset(bus, slot, func, PCI_GENERAL_CAPABILITIES_OFFSET, 1);

    // Start parsing
    while (cap_list_off) {
        uint16_t cap = pci_readConfigOffset(bus, slot, func, cap_list_off, 2);
        if ((cap & 0xFF) == 0x11) {
            // MSI-X
            break;
        }

        cap_list_off = (cap >> 8) & 0xFC;
    }

    if (!cap_list_off) {
        return 0; // No MSI-X support
    }

    // Disable MSI-X
    uint16_t ctrl = pci_readConfigOffset(bus, slot, func, cap_list_off + 0x02, 2);
    ctrl &= ~(1 << 15);
    pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x02, ctrl, 2);
    return 0;
}

/**
 * @brief Disable pin interrupts
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * @returns 0 on success
 */
int pci_disablePinInterrupts(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_readConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, 2);
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, cmd | (PCI_COMMAND_INTERRUPT_DISABLE), 2);
    return 0;
}

/* Prototype */
static void pci_probeBus(uint8_t bus);

/**
 * @brief Probe function
 * @param bus The bus of the device
 * @param slot The slot number
 * @param function The function number
 */
static void pci_probeFunction(uint8_t bus, uint8_t slot, uint8_t function) {
    // Is this a valid device?
    if (pci_readConfigOffset(bus, slot, function, PCI_VENID_OFFSET, 2) == PCI_NONE) return;

    // Yes, initialize this device.
    pci_device_t *dev = PCI_DEVICE(bus, slot, function);
    dev->valid = 1;
    dev->bus = bus;
    dev->slot = slot;
    dev->function = function;
    dev->vid = pci_readConfigOffset(bus, slot, function, PCI_VENID_OFFSET, 2);
    dev->devid = pci_readConfigOffset(bus, slot, function, PCI_DEVID_OFFSET, 2);
    dev->class_code = pci_readConfigOffset(bus, slot, function, PCI_CLASSCODE_OFFSET, 1);
    dev->subclass_code = pci_readConfigOffset(bus, slot, function, PCI_SUBCLASS_OFFSET, 1);
    dev->driver = NULL;
    dev->msi_offset = -1;
    dev->msix_offset = -1;
    dev->irqs = NULL;

    // LOG(DEBUG, "Found device %04x:%04x on bus %02x slot %02x func %02x\n", dev->vid, dev->devid, dev->bus, dev->slot, dev->function);

    // Do we need to initialize another bus?
    if (dev->class_code == 0x06 && dev->subclass_code == 0x04) {
        // PCI-to-PCI bridge
        uint8_t secondary_bus = pci_readConfigOffset(bus, slot, function, 0x19, 1);
        pci_probeBus(secondary_bus);
    }
}

/**
 * @brief Probe device
 * @param bus Bus number
 * @param slot Slot number
 */
static void pci_probeSlot(uint8_t bus, uint8_t slot) {
    // Check vendor ID
    if (pci_readConfigOffset(bus, slot, 0, PCI_VENID_OFFSET, 2) == PCI_NONE) return;

    // Check the first function
    pci_probeFunction(bus, slot, 0);

    // Are we multi-function?
    uint8_t type = pci_readConfigOffset(bus, slot, 0, PCI_HEADER_TYPE_OFFSET, 1);
    if (type & PCI_HEADER_TYPE_MULTIFUNCTION) {
        // Yes, probe each function
        for (int func = 1; func < PCI_MAX_FUNC; func++) {
            pci_probeFunction(bus, slot, func);
        }
    }
}

/**
 * @brief Probe bus
 * @param bus Bus number
 */
static void pci_probeBus(uint8_t bus) {
    for (int slot = 0; slot < PCI_MAX_SLOT; slot++) {
        pci_probeSlot(bus, slot);
    }
}

/**
 * @brief Initialize and probe for PCI devices
 */
void pci_init() {
    // Perform PCI probing
    // Check if this bus is multi-function
    uint8_t header_type = pci_readConfigOffset(0, 0, 0, PCI_HEADER_TYPE_OFFSET, 1);
    if (header_type & PCI_HEADER_TYPE_MULTIFUNCTION) {
        for (int func = 0; func < 8; func++) {
            uint16_t vid = pci_readConfigOffset(0, 0, func, PCI_VENID_OFFSET, 2);
            if (vid != PCI_NONE) {
                pci_probeBus(func);
            }
        }
    } else {
        pci_probeBus(0);
    }

    LOG(INFO, "PCI probing completed\n");
}



/* Prototype */
int pci_scanBus(uint8_t bus, pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data);

/**
 * @brief Scan for PCI devices on a function
 * @param bus The bus to scan
 * @param slot The slot to scan
 * @param function The function to scan
 * @param callback Callback function
 * @param parameters Scan parameters (optional, leave as NULL if you don't care)
 * @param data Driver-specific data to pass along
 */
int pci_scanFunction(uint8_t bus, uint8_t slot, uint8_t function, pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data) {
    pci_device_t *dev = PCI_DEVICE(bus, slot, function);
    if (!dev->valid) return 0;

    // Do we need to initialize another bus?
    if (dev->class_code == 0x06 && dev->subclass_code == 0x04) {
        // PCI-to-PCI bridge
        uint8_t secondary_bus = pci_readConfigOffset(bus, slot, function, 0x19, 1);
        pci_scanBus(secondary_bus, callback, parameters, data);
    }

    // Check to see if this matches parameters
    if (parameters) {
        if (parameters->class_code && dev->class_code != parameters->class_code) return 0;
        if (parameters->subclass_code && dev->subclass_code != parameters->subclass_code) return 0;
    
        if (parameters->id_list) {
            pci_id_mapping_t *map = parameters->id_list;
            
            while (map) {
                if (map->vid == PCI_NONE) return 0;

                // Matching VID?
                if (map->vid == dev->vid) {
                    // For each devid
                    uint16_t *devid = map->devid;

                    if (*devid == PCI_NONE) {
                        // The first and only devid in the list is PCI_NONE, accept all that have VID
                        break;
                    }

                    int found = 0;
                    while (*devid != PCI_NONE) {
                        if (*devid == dev->devid) { found = 1; break; }
                        devid++;
                    }

                    // Did we find it?
                    if (!found) return 0;
                    
                    // Yes
                    break;
                
                } 

                map++;
            }
        }
    }

    return callback(dev, data);
}

/**
 * @brief Scan for PCI devices on a slot
 * @param bus The bus to scan
 * @param slot The slot to scan
 * @param callback Callback function
 * @param parameters Scan parameters (optional, leave as NULL if you don't care)
 * @param data Driver-specific data to pass along
 */
int pci_scanSlot(uint8_t bus, uint8_t slot, pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data) {
    // Check vendor ID
    if (pci_readConfigOffset(bus, slot, 0, PCI_VENID_OFFSET, 2) == PCI_NONE) return 0;

    // Check the first function
    if (pci_scanFunction(bus, slot, 0, callback, parameters, data)) return 1;

    // Are we multi-function?
    uint8_t type = pci_readConfigOffset(bus, slot, 0, PCI_HEADER_TYPE_OFFSET, 1);
    if (type & PCI_HEADER_TYPE_MULTIFUNCTION) {
        // Yes, probe each function
        for (int func = 1; func < PCI_MAX_FUNC; func++) {
            if (pci_scanFunction(bus, slot, func, callback, parameters, data)) {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Scan for PCI devices on a bus
 * @param bus The bus to scan
 * @param callback Callback function
 * @param parameters Scan parameters (optional, leave as NULL if you don't care)
 * @param data Driver-specific data to pass along
 */
int pci_scanBus(uint8_t bus, pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data) {
    for (int slot = 0; slot < 32; slot++) {
        if (pci_scanSlot(bus, slot, callback, parameters, data)) {
            return 1;
        } 
    }

    return 0;
}

/**
 * @brief Scan for PCI devices
 * @param callback Callback function
 * @param parameters Scan parameters (optional, leave as NULL if you don't care)
 * @param data Driver-specific data to pass along
 * @returns 1 on an error, 0 on success
 */
int pci_scanDevice(pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data) {
    // Check if this bus is multi-function
    uint8_t header_type = pci_readConfigOffset(0, 0, 0, PCI_HEADER_TYPE_OFFSET, 1);
    if (header_type & PCI_HEADER_TYPE_MULTIFUNCTION) {
        for (int func = 0; func < 8; func++) {
            uint16_t vid = pci_readConfigOffset(0, 0, func, PCI_VENID_OFFSET, 2);
            if (vid != PCI_NONE) {
                if (pci_scanBus(func, callback, parameters, data)) {
                    return 1;
                }
            }
        }
    } else {
        return pci_scanBus(0, callback, parameters, data);
    }

    return 0;
}

/**
 * @brief Get a device from bus/slot/function
 * @param bus The bus to check
 * @param slot The slot to check
 * @param function The function to check
 * @returns Device object or NULL
 */
pci_device_t *pci_getDevice(uint8_t bus, uint8_t slot, uint8_t function) {
    pci_device_t *dev = PCI_DEVICE(bus, slot, function);
    if (dev->valid) return dev;
    return NULL;
}

/**
 * @brief Get capability of PCI device
 * @param dev The device to search the capability list of
 * @param offset The offset to check (pointer)
 * @returns The first byte at the capability
 */
uint8_t pci_getNextCapability(pci_device_t *dev, uint8_t *offset) {
    uint16_t sts;
    pci_readConfigWord(dev, PCI_STATUS_OFFSET, &sts);
    if (((sts & PCI_STATUS_CAPABILITIES_LIST) == 0)) {
        return PCI_CAP_NONE;
    }

    if (*offset == 0) {
        uint8_t cap_list_off;
        pci_readConfigByte(dev, PCI_GENERAL_CAPABILITIES_OFFSET, &cap_list_off);
        *offset = cap_list_off;
    }

    uint16_t cap;
    pci_readConfigWord(dev, *offset, &cap);

    // next capability
    *offset = (cap >> 8) & 0xfc;
    return *offset ? cap & 0xff : PCI_CAP_NONE;
}

/**
 * @brief PCI probe for MSI/MSI-X
 */
static void pci_probeCaps(pci_device_t *dev) {
    // TODO use api pci_getNextCapability
    uint16_t sts;
    pci_readConfigWord(dev, PCI_STATUS_OFFSET, &sts);
    if (((sts & PCI_STATUS_CAPABILITIES_LIST) == 0)) {
        return;
    }

    uint8_t cap_list_off;
    pci_readConfigByte(dev, PCI_GENERAL_CAPABILITIES_OFFSET, &cap_list_off);

    while (cap_list_off) {
        uint16_t cap;
        pci_readConfigWord(dev, cap_list_off, &cap);

        if ((cap & 0xFF) == PCI_CAP_MSI) {
            LOG(DEBUG, "Probe: Found MSI offset at 0x%x\n", cap_list_off);
            dev->msi_offset = cap_list_off;
        }

        if ((cap & 0xFF) == PCI_CAP_MSI_X) {
            LOG(DEBUG, "Probe: Found MSI-X offset at 0x%x\n", cap_list_off);
            dev->msix_offset = cap_list_off;
        }

        cap_list_off = (cap >> 8) & 0xfc;
    }
}

/**
 * @brief Attempt to allocate MSI interrupt
 */
static int pci_allocateInterruptMSI(pci_device_t *dev, int minimum, int maximum) {
    if (dev->msi_offset == -1) {
        // Search configuration space for this
        // TODO: pre-parse this during probe so we dont have to reparse
        pci_probeCaps(dev);
        if (dev->msi_offset == -1) return -ENODEV;
    }


    // Read the MSI message control register
    uint16_t msi_control;
    pci_readConfigWord(dev, dev->msi_offset + 0x02, &msi_control);

    // Check multi-message capable
    int messages_capable = (msi_control >> 1) & 0x7;
    messages_capable = 1 << messages_capable;

    if (messages_capable < minimum) {
        LOG(WARN, "MSI only supports %d interrupts, need %d\n", messages_capable, minimum);
        return -EINVAL;
    }
    
    // TODO: MME requires the interrupt vectors to be aligned. We dont support that right now
    // For now, if the user wanted a max of 1 interrupt (or a minimum of 1 on a device that doesnt support MSI-X)
    // then we can use MSI.
    if (!(dev->msix_offset == -1 && minimum <= 1) && maximum != 1) {
        // !!!: This prevents the case where the user only wants MSI and doesnt want MSI-X in which case call fails.
        LOG(WARN, "Skipping MSI because user wanted multiple interrupts\n");
        return -1;
    }

    // Enable
    msi_control &= ~(0x07 << 4);
    msi_control |= 1;
    pci_writeConfigWord(dev, dev->msi_offset + 0x02, msi_control);

    // Allocate an MSI IRQ
    pci_irq_t *irqs = kmalloc(sizeof(pci_irq_t) * 1);
    irqs[0].type = PCI_IRQ_MSI;

    int ret = irq_allocate(msi_domain, 0, (void*)dev, &irqs[0].vector);
    if (ret != 0) {
        kfree(irqs);
        return ret;
    }

    // IRQs allocated
    dev->irqs = irqs;
    dev->nirqs = 1;

    // Turn off MSI-X and pin interrupts
    pci_disableMSIX(dev->bus, dev->slot, dev->function);
    pci_disablePinInterrupts(dev->bus, dev->slot, dev->function);


    return 1;
}

/**
 * @brief Attempt to allocate MSI-X interrupts
 */
static int pci_allocateInterruptMSIX(pci_device_t *dev, int min, int max) {
    if (dev->msix_offset == -1) {
        // Search configuration space for this
        // TODO: pre-parse this during probe so we dont have to reparse
        pci_probeCaps(dev);
        if (dev->msix_offset == -1) return -ENODEV;
    }

    // Enable MSI-X
    // See https://wiki.osdev.org/PCI
    uint16_t ctrl;
    pci_readConfigWord(dev, dev->msix_offset + 0x02, &ctrl);
    ctrl |= (1 << 15) | (1 << 14);
    pci_writeConfigWord(dev, dev->msix_offset + 0x02, ctrl);

    // Check the table size
    uint16_t table_size = ctrl & 0x7FF;
    if (table_size+1 < min) {
        LOG(ERR, "Table size is too small (need %d interrupts only have %d).\n", min, table_size);
        return -EINVAL;
    }

    // Find the amount of interrupts we want
    int nirq = (table_size+1 < max) ? table_size+1 : max;
    LOG(DEBUG, "MSI-X allocating %d IRQs\n", nirq);

    // Get the BIR and table offset
    uint32_t dw;
    pci_readConfigDword(dev, dev->msix_offset + 0x04, &dw);
    uint32_t tb_off = (dw & ~7u);
    uint8_t bir = (uint8_t)(dw & 7u);

    // Read the BAR
    pci_bar_t *bar = pci_readBAR(dev->bus, dev->slot, dev->function, bir);
    if (!bar || (bar->type != PCI_BAR_MEMORY32 && bar->type != PCI_BAR_MEMORY64)) {
        LOG(ERR, "MSI-X is missing BAR%d or it is invalid\n", bir);
        return -EINVAL;
    }

    // Map the BAR and begin allocation
    uintptr_t b = mmio_map(bar->address, bar->size);

    dev->irqs = kzalloc(sizeof(pci_irq_t) * nirq);
    int irqs_allocated = 0;

    // TODO: If eventually allocating more interrupts without freeing is supported need to use dev->msix_index
    for (int i = 0; i < nirq; i++) {
        int r = irq_allocate(msix_domain, i, (void*)(b + tb_off), &dev->irqs[i].vector);
        if (r != 0) {
            // TODO: maybe return error code
            LOG(WARN, "Failed to allocate IRQ (error %d)\n", r);
            break;
        }

        dev->irqs[i].type = PCI_IRQ_MSI_X;
        irqs_allocated++;
    }

    // Turn off MSI and pin interrupts if we got any IRQs
    if (irqs_allocated > 0) {
        pci_disableMSI(dev->bus, dev->slot, dev->function);
        pci_disablePinInterrupts(dev->bus, dev->slot, dev->function);
    }
    
    dev->nirqs = irqs_allocated;

    // TODO: cleanup BAR MMIO
    kfree(bar);

    // re-enable MSI-X
    ctrl &= ~(1 << 14);
    pci_writeConfigWord(dev, dev->msix_offset + 0x02, ctrl);

    return irqs_allocated;
}

/**
 * @brief Attempt to allocate pin interrupt
 */
static int pci_allocatePinInterrupt(pci_device_t *dev, int min, int max) {
    // Pin interrupt allocation sucks. We dont support reading the GSI bases from
    // ACPI and we just make some guesses.
    LOG(WARN, "Trying to allocate pin interrupt. This is going to probably go very poorly.\n");
    if (min > 1) return -1;

    // Without a _PRT the only thing we can do is assume all GSIs are 0 for PCI devices.
    // This statement only really holds true for emulators..
    // !!!: This is REALLY bad and will fail eventually.
    uint8_t irq_line;
    pci_readConfigByte(dev, PCI_GENERAL_INTERRUPT_OFFSET, &irq_line);

    LOG(DEBUG, "IRQ%d\n", irq_line);
    
    // Lol you'll just get unexpected interrupt if this doesnt work
    irq_number_t num;
    int r = irq_allocate(global_domain, irq_line, NULL, &num);
    if (r != 0) {
        LOG(ERR, "Failed to register handler for hwirq%d\n", num);
        return r;
    }

    // Allocate a single IRQ
    pci_irq_t *irqs = kmalloc(sizeof(pci_irq_t) * 1);
    irqs[0].vector = num;
    irqs[0].type = PCI_IRQ_PIN_INTERRUPT;
    
    // IRQs
    dev->irqs = irqs;
    dev->nirqs = 1;

    return 1;
}

/**
 * @brief Allocate IRQ vectors for a device
 * @param dev The device to allocate IRQ vectors for
 * @param min The minimum number of IRQ vectors to allocate
 * @param max The maximum number of IRQ vectors to allocate
 * @param allowed Allowed IRQ vector type bitmask
 * @returns Number of interrupts allocated on success, negative for failure
 */
int pci_allocateInterrupts(pci_device_t *dev, int min, int max, unsigned char allowed) {
    assert(dev->irqs == NULL && "allocating more interrupts without freeing existing not supported yet");

    int ret = 0;
    if (allowed & PCI_IRQ_MSI_X) {
        ret = pci_allocateInterruptMSIX(dev, min, max);
        if (ret > 0) goto finish;
        LOG(WARN, "MSI-X failed to allocate (%d)\n", ret);
    }

    if (allowed & PCI_IRQ_MSI) {
        ret = pci_allocateInterruptMSI(dev, min, max);
        if (ret > 0) goto finish;
        LOG(WARN, "MSI failed to allocate (%d)\n", ret);
    }

    if (allowed & PCI_IRQ_PIN_INTERRUPT) {
        ret = pci_allocatePinInterrupt(dev, min, max);
        if (ret > 0) goto finish;
        LOG(WARN, "Pin interrupt failed to allocate (%d)\n", ret);
    }

    assert(0 && "No PCI interrupts available");

finish:
    return ret;
}

/**
 * @brief Get an interrupt vector allocated for a device
 * @param dev The device to get the interrupt vector for
 * @param idx The index of the allocated interrupt vector
 * @returns IRQ object
 */
pci_irq_t *pci_getInterruptVector(pci_device_t *dev, int idx) {
    assert(idx < dev->nirqs);
    return &dev->irqs[idx];
}

/**
 * @brief Free interrupt vectors allocated for device
 * @param dev The device to free the IRQ vectors for
 */
int pci_freeInterrupts(pci_device_t *dev) {
    pci_irq_t *irqs = dev->irqs;
    dev->nirqs = 0;
    dev->irqs = NULL;

    kfree(irqs); // TODO: unregister all IRQs
    return 0;
}
