/**
 * @file hexahedron/drivers/pci.c
 * @brief PCI driver
 * 
 * @todo Support MSI and MSI-X
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
#include <kernel/fs/kernelfs.h>
#include <kernel/mem/mem.h>
#include <kernel/debug.h>
#include <kernel/arch/arch.h>
#include <assert.h>
#include <string.h>
#include <kernel/misc/args.h>

#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
#include <kernel/drivers/x86/pic.h>
#include <kernel/drivers/x86/local_apic.h>
#endif

/* MSI map array */
/* TODO: This will be rewritten */
uint8_t msi_array[HAL_IRQ_MSI_COUNT / 8] = { 0 };

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
 * @brief Get the interrupt registered to a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the interrupt ID
 */
uint8_t pci_getInterrupt(uint8_t bus, uint8_t slot, uint8_t func) {
    // Disable MSI and MSI-X
    pci_disableMSI(bus, slot, func);
    pci_disableMSIX(bus, slot, func);

    // TODO: Make sure header type is 1?
#if !defined(__ARCH_I386__) && !defined(__ARCH_X86_64__)
    return pci_readConfigOffset(bus, slot, func, PCI_GENERAL_INTERRUPT_OFFSET, 1);
#else

    // !!!: This is a hack to get Bochs working since it doesn't support setting custom IRQ vectors
    uint8_t irq_original = pci_readConfigOffset(bus, slot, func, PCI_GENERAL_INTERRUPT_OFFSET, 1);
    if (irq_original != 0xFF && !hal_interruptHandlerInUse(irq_original)) {
        LOG(DEBUG, "PCI using default IRQ%d as it was not in use\n", irq_original);
        uint16_t cmd = pci_readConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, 2);
        pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, cmd & ~(PCI_COMMAND_INTERRUPT_DISABLE), 2);
        return irq_original;
    }

    // Allocate an IRQ from the PIC
    uint32_t irq = pic_allocate();
    LOG(DEBUG, "PCI allocated IRQ%d\n", irq);
    if (irq == 0xFFFFFFFF) return 0xFF;
    
    // Enable IRQs
    uint16_t cmd = pci_readConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, 2);
    pci_writeConfigOffset(bus, slot, func, PCI_COMMAND_OFFSET, cmd & ~(PCI_COMMAND_INTERRUPT_DISABLE), 2);

    pci_writeConfigOffset(bus, slot, func, PCI_GENERAL_INTERRUPT_OFFSET, irq, 1);
    

    return irq;    
#endif
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



/**
 * @brief Enable MSI-X
 */
static uint8_t pci_enableMSIX(uint8_t bus, uint8_t slot, uint8_t func, uint8_t msi_off) {
    // Find an available interrupt
    uint8_t interrupt = 0xFF;
    for (int i = 0; i < HAL_IRQ_MSI_COUNT; i++) {
        if (!(msi_array[i / 8] & (1 << (i % 8)))) {
            interrupt = i;
            msi_array[i / 8] |= (1 << (i % 8));
            break;
        }
    }

    if (interrupt == 0xFF) {
        LOG(ERR, "Kernel is out of MSI vectors. This is a bug.\n");
        return 0xFF;
    }

    interrupt = HAL_IRQ_MSI_BASE + interrupt;
    LOG(DEBUG, "MSIX: Get interrupt %x\n", interrupt);


    // Set it up
    uint16_t ctrl = pci_readConfigOffset(bus, slot, func, msi_off + 0x02, 2);
    ctrl |= (1 << 15);
    pci_writeConfigOffset(bus, slot, func, msi_off + 0x02, ctrl, 2);

    uint32_t dw = pci_readConfigOffset(bus, slot, func, msi_off + 0x04, 4);
    uint32_t offset = dw & ~7u;
    uint8_t bir = (uint8_t)(dw & 7u);

    LOG(DEBUG, "BIR=%02x OFF=%08x\n", bir, offset);

    uint64_t addr = 0xFEE00000;

    // Allocate the BAR region
    pci_bar_t *bar = pci_readBAR(bus, slot, func, bir);
    if (!bar || bar->type == PCI_BAR_IO_SPACE || bar->type == PCI_BAR_MEMORY16) {
        LOG(WARN, "MSI-X device is missing BAR%d or it is invalid\n", bir);
        return 0xFF;
    }

    // TODO: FIND A WAY TO CLEAN THIS UP (?)
    uintptr_t r = mem_mapMMIO(bar->address, bar->size);

    pci_msix_entry_t *entry = (pci_msix_entry_t*)&(((pci_msix_entry_t*)(r + offset))[PCI_DEVICE(bus, slot, func)->msix_index]);
    entry->msg_addr_low = addr & 0xFFFFFFFF;
    entry->msg_addr_high = addr >> 32;
    entry->msg_data = (interrupt & 0xFF);
    entry->vector_ctrl = entry->vector_ctrl & ~1u;
    
    PCI_DEVICE(bus, slot, func)->msix_index++;

    // Disable MSI + pin
    pci_disableMSI(bus, slot, func);
    pci_disablePinInterrupts(bus, slot, func);

    PCI_DEVICE(bus, slot, func)->msix_offset = msi_off;

    return interrupt - HAL_IRQ_BASE;
} 

/**
 * @brief Get MSI interrupts
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * @returns 0xFF or the interrupt ID
 */
uint8_t pci_enableMSI(uint8_t bus, uint8_t slot, uint8_t func) {
#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)
    if (!lapic_initialized()) {
        LOG(WARN, "MSI enabling failed: Local APIC not initialized. Buggy interrupts may occur\n");
        return 0xFF;
    }
#endif

    // Find the MSI capability
    uint16_t status = pci_readConfigOffset(bus, slot, func, PCI_STATUS_OFFSET, 2);
    if (!(status & PCI_STATUS_CAPABILITIES_LIST)) {
        return 0xFF;
    }

    // Get a pointer to the capability list
    uint8_t cap_list_off = pci_readConfigOffset(bus, slot, func, PCI_GENERAL_CAPABILITIES_OFFSET, 1);
    uint8_t msi_saved = 0x0;

    // Start parsing
    while (cap_list_off) {
        uint16_t cap = pci_readConfigOffset(bus, slot, func, cap_list_off, 2);
        if ((cap & 0xFF) == 0x05) {
            LOG(DEBUG, "MSI offset found at %x\n", cap_list_off);
            msi_saved = cap_list_off;
        }

        if ((cap & 0xFF) == 0x11) {
            // MSI-X
            LOG(DEBUG, "MSI-X offset found at %x\n", cap_list_off);

            uint8_t r = pci_enableMSIX(bus, slot, func, cap_list_off);
            if (r != 0xFF) return r;
        }

        cap_list_off = (cap >> 8) & 0xFC;
    }

    // Did we find it?
    if (!msi_saved) {
        LOG(ERR, "Device does not support MSI or MSI-X\n");
        return 0xFF;
    }

    cap_list_off = msi_saved;

    // Find an available interrupt
    uint8_t interrupt = 0xFF;
    for (int i = 0; i < HAL_IRQ_MSI_COUNT; i++) {
        if (!(msi_array[i / 8] & (1 << (i % 8)))) {
            interrupt = i;
            msi_array[i / 8] |= (1 << (i % 8));
            break;
        }
    }

    if (interrupt == 0xFF) {
        LOG(ERR, "Kernel is out of MSI vectors. This is a bug.\n");
        return 0xFF;
    }

    interrupt = HAL_IRQ_MSI_BASE + interrupt;

    LOG(DEBUG, "MSI: Get interrupt %x\n", interrupt);


    // Start parsing the MSI information
    // TODO: Put this into a header file
    
    // First enable MSI and only use one interrupt
    uint16_t ctrl = pci_readConfigOffset(bus, slot, func, cap_list_off + 0x02, 2);
    ctrl &= ~(0x07 << 4);
    ctrl |= 1;
    pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x02, ctrl, 2);

    LOG(DEBUG, "msg_ctrl = %04x (64-bit: %s)\n", ctrl, (ctrl & (1 << 7)) ? "YES" : "NO");

    // Configure message address and data
    if (ctrl & (1 << 7)) {
        // 64-bit address supported
        uint64_t addr = 0xFEE00000;
        uint32_t data = interrupt & 0xFF;
        pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x04, addr & 0xFFFFFFFF, 4);
        pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x08, (addr >> 32), 4);
        pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x0C, data, 2);
    } else {
        // Only 32-bit
        uint32_t addr = 0xFEE00000;
        uint32_t data = interrupt & 0xFF;

        pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x04, addr & 0xFFFFFFFF, 4);
        pci_writeConfigOffset(bus, slot, func, cap_list_off + 0x08, data, 2);
    }
    
    // Disable pin interrupts too
    pci_disableMSIX(bus, slot, func);
    pci_disablePinInterrupts(bus, slot, func);
    
    PCI_DEVICE(bus, slot, func)->msi_offset = cap_list_off;

    return interrupt - 32;
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
    dev->pid = pci_readConfigOffset(bus, slot, function, PCI_DEVID_OFFSET, 2);
    dev->class_code = pci_readConfigOffset(bus, slot, function, PCI_CLASSCODE_OFFSET, 1);
    dev->subclass_code = pci_readConfigOffset(bus, slot, function, PCI_SUBCLASS_OFFSET, 1);
    dev->driver = NULL;
    dev->msi_offset = 0xFF;
    dev->msix_offset = 0xFF;

    // LOG(DEBUG, "Found device %04x:%04x on bus %02x slot %02x func %02x\n", dev->vid, dev->pid, dev->bus, dev->slot, dev->function);

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
                    // For each PID
                    uint16_t *pid = map->pid;

                    if (*pid == PCI_NONE) {
                        // The first and only PID in the list is PCI_NONE, accept all that have VID
                        break;
                    }

                    int found = 0;
                    while (*pid != PCI_NONE) {
                        if (*pid == dev->pid) { found = 1; break; }
                        pid++;
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
 * @brief PCI KernelFS scan method
 */
static int pci_kernelFSScan(pci_device_t *dev, void *data) {
    kernelfs_entry_t *entry = (kernelfs_entry_t*)data;

    kernelfs_appendData(entry,   "%02x:%02x.%d (%04x, %04x:%04x)\n"
        " IRQ: %d Pin: %d\n"
        " BAR0: 0x%08x BAR1: 0x%08x BAR2: 0x%08x BAR3: 0x%08x BAR4: 0x%08x BAR5: 0x%08x\n", 
            dev->bus, dev->slot, dev->function, pci_readType(dev->bus, dev->slot, dev->function), dev->vid, dev->pid,
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_INTERRUPT_OFFSET, 1), pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_INTERRUPT_PIN_OFFSET, 1),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR0_OFFSET, 4),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR1_OFFSET, 4),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR2_OFFSET, 4),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR3_OFFSET, 4),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR4_OFFSET, 4),
            pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_GENERAL_BAR5_OFFSET, 4));

    return 0;
}

/**
 * @brief PCI KernelFS 
 */
static int pci_fillKernelFS(struct kernelfs_entry *entry, void *data) {
    pci_scanDevice(pci_kernelFSScan, NULL, (void*)entry);
    entry->finished = 1;
    return 0;
}

/**
 * @brief Mount PCI KernelFS node
 */
void pci_mount() {
    kernelfs_dir_t *dir = kernelfs_createDirectory(NULL, "pci", 1);
    kernelfs_createEntry(dir, "devices", pci_fillKernelFS, NULL);
}