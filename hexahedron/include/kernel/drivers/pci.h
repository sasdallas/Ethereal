/**
 * @file hexahedron/include/kernel/drivers/x86/pci.h
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

#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

// General stuff
#define PCI_NONE                    0xFFFF  // Whatever you wanted isn't present
#define PCI_MAX_BUS                 256     // 256 buses
#define PCI_MAX_SLOT                32      // 32 slots
#define PCI_MAX_FUNC                8       // 8 functions

#define PCI_MAX_PID                 32

// BAR-specifics
#define PCI_BAR_MEMORY32            0x0     // 32-bit memory space BAR (physical RAM) 
#define PCI_BAR_IO_SPACE            0x1     // I/O space BAR, can reside at any memory address
#define PCI_BAR_MEMORY16            0x2     // 16-bit memory space BAR (reserved nowadays) 
#define PCI_BAR_MEMORY64            0x4     // 64-bit memory space BAR

// I/O Addresses
#define PCI_CONFIG_ADDRESS          0xCF8
#define PCI_CONFIG_DATA             0xCFC 

// Common header fields offsets
#define PCI_VENID_OFFSET            0x00    // Vendor ID
#define PCI_DEVID_OFFSET            0x02    // Device ID
#define PCI_COMMAND_OFFSET          0x04    // Controls a device's PCI bus connection/cycles
#define PCI_STATUS_OFFSET           0x06    // Status of PCI bus events
#define PCI_REVISION_ID_OFFSET      0x08    // Revision ID for the device
#define PCI_PROGIF_OFFSET           0x09    // Programming interface byte (register-level programming interface of the device)
#define PCI_SUBCLASS_OFFSET         0x0A    // Specific function of the device
#define PCI_CLASSCODE_OFFSET        0x0B    // Type of function the device performs 
#define PCI_CACHE_LINE_SIZE_OFFSET  0x0C    // System cache line size in 32-bit units
#define PCI_LATENCY_TIMER_OFFSET    0x0D    // Latency timer in units of PCI bus clocks (?)
#define PCI_HEADER_TYPE_OFFSET      0x0E    // Identifies the layout of the rest of the header
#define PCI_BIST_OFFSET             0x0F    // BIST (built-in self test)

// Command register layout
#define PCI_COMMAND_IO_SPACE                0x01    // Device can respond to I/O space accesses
#define PCI_COMMAND_MEMORY_SPACE            0x02    // Device can respond to memory space accesses
#define PCI_COMMAND_BUS_MASTER              0x04    // Device can behave as a bus master
#define PCI_COMMAND_SPECIAL_CYCLES          0x08    // Device can monitor special cycles
#define PCI_COMMAND_MEM_WRITEINVL           0x10    // Device can generate memory write and invalidate commands
#define PCI_COMMAND_VGA_SNOOP               0x20    // Device will snoop data from the VGA pallete instead of responding to writes
#define PCI_COMMAND_PARITY_RESPONSE         0x40    // Device can take normal action when parity error occurs
#define PCI_COMMAND_SERR                    0x100   // SERR# driver enabled
#define PCI_COMMAND_FAST_BACKBACK           0x200   // Device can generate fast back-to-back transactions 
#define PCI_COMMAND_INTERRUPT_DISABLE       0x400   // Assertions of device interupts are disabled

// Status register layout
#define PCI_STATUS_INTERRUPT_STATUS         0x08    // Device INTx# signal status
#define PCI_STATUS_CAPABILITIES_LIST        0x10    // Device supports capabilities list (offset 0x34)
#define PCI_STATUS_66MHZ_CAPABLE            0x20    // Device supports running at 66 MHz
#define PCI_STATUS_FAST_BACKBACK_CAPABLE    0x80    // Device supports fast back-to-back transactions
#define PCI_STATUS_MASTER_DATA_PARITY_ERR   0x100   // Something that really isn't important - feel free to read it at https://wiki.osdev.org/PCI#Configuration_Space
#define PCI_STATUS_DEVSEL_TIMING            0x600   // Slowest time that a devivce will assert DEVSEL# (0x0 = fast, 0x1 = medium, 0x2 = slow)
#define PCI_STATUS_SIGNALLED_TARGET_ABORT   0x800   // Target device transaction terminated with Target-Abort
#define PCI_STATUS_RECEIVED_TARGET_ABORT    0x1000  // Master device transaction terminated with Target-Abort
#define PCI_STATUS_RECEIVED_MASTER_ABORT    0x2000  // Master device transaction terminated with Master-Abort
#define PCI_STATUS_SIGNALLED_SYSTEM_ERROR   0x4000  // Device asserted SERR#
#define PCI_STATUS_PARITY_ERROR             0x8000  // Device detected parity error

// Header types
#define PCI_HEADER_TYPE                     0x03    // Mask for header types (multifunction is a bit)
#define PCI_HEADER_TYPE_GENERAL             0x00
#define PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE   0x01
#define PCI_HEADER_TYPE_PCI_CARDBUS_BRIDGE  0x02
#define PCI_HEADER_TYPE_MULTIFUNCTION       0x80

// Header type 1 (general device)
#define PCI_GENERAL_BAR0_OFFSET             0x10    // BARx = Base Address Register
#define PCI_GENERAL_BAR1_OFFSET             0x14
#define PCI_GENERAL_BAR2_OFFSET             0x18
#define PCI_GENERAL_BAR3_OFFSET             0x1C
#define PCI_GENERAL_BAR4_OFFSET             0x20
#define PCI_GENERAL_BAR5_OFFSET             0x24
#define PCI_GENERAL_CARDBUS_CIS_OFFSET      0x28    // Points to Card Information Structure (used by devices that share silicon between CardBus and PCI)
#define PCI_GENERAL_SUBSYSTEM_VENID_OFFSET  0x2C
#define PCI_GENERAL_SUBSYSTEM_ID_OFFSET     0x2E
#define PCI_GENERAL_EXPANSION_ROM_OFFSET    0x30
#define PCI_GENERAL_CAPABILITIES_OFFSET     0x34    // Pointer to a linked list of new capabilites implemented by the device (only if PCI_STATUS_CAPABILITIES_LIST is 1)
#define PCI_GENERAL_INTERRUPT_OFFSET        0x3C    // Corresponds to the PIC IRQ #0-15 (0xFF = no connection)
#define PCI_GENERAL_INTERRUPT_PIN_OFFSET    0x3D    // Specifies which interrupt pin the device devices (0x1 = INTA#, 0x2 = INTB#, 0x3 = INTC#, 0x4 = INTD#, and 0x0 = no interrupt pin)
#define PCI_GENERAL_MIN_GRANT_OFFSET        0x3E    // Burst period length in 1/4 microsecond units
#define PCI_GENERAL_MAX_LATENCY_OFFSET      0x3F    // How often the device needs access to the PCI bus (in 1/4 microsecond units)


// PCI types that are required
#define PCI_TYPE_BRIDGE                     0x0604  // PCI-to-PCI bridge

/**** TYPES ****/

/* PCI address */
typedef uint32_t pci_address_t;


/**
 * @brief PCI device object
 * @param bus Device bus
 * @param slot Device slot
 * @param function Device function
 * @param class_code Class code of the device
 * @param subclass_code Subclass code of the device
 * @param vid Vendor ID of the device
 * @param pid Product ID of the device
 */
typedef struct pci_device {
    uint8_t             bus;                // Bus
    uint8_t             slot;               // Slot
    uint8_t             function;           // Function
    uint8_t             class_code;         // Class code
    uint8_t             subclass_code;      // Subclass code
    uint16_t            vid;                // Vendor ID
    uint16_t            pid;                // Product ID
    void                *driver;            // Driver-specific field                       

    uint32_t            msi_offset;         // MSI offset in configuration space (or 0xFF if not found)
    uint32_t            msix_offset;        // MSI-X offset in configuration space (or 0xFF if not found)

    int                 valid;              // !!!: Valid device hack
    uint32_t msix_index;                    // MSIX index
} pci_device_t;

/**
 * @brief PCI base address register
 *
 * @param type Defines the type of the BAR and its address size. Can be @c PCI_BAR_IO_SPACE or @c PCI_BAR_MEMORY32 or @c PCI_BAR_MEMORY64
 * @param size Size of the actual BAR in memory
 * @param prefetchable Whether the BAR is prefetchable (applies to memory BARs only)
 * @param address The address of the BAR
 */
typedef struct pci_bar {
    int type;           // Type of the BAR
    uint64_t size;      // Size of the BAR
    int prefetchable;   // Whether the BAR is prefetchable (it does not read side effects)
    uint64_t address;   // Address of the BAR. Note that it doesn't take up all of this space.s
} pci_bar_t;

/**
 * @brief PCI callback function
 * 
 * @param bus The bus of the currently being checked PCI device
 * @param slot The slot of the currently being checked PCI device
 * @param function The function of the currently being checked PCI device
 * @param vendor_id The vendor ID of the device
 * @param device_id The device ID of the device
 * @param data Any additionally specified data by the caller of @c pci_scan
 * 
 * @returns 1 on successfully found, 0 on failed to find
 */
typedef int (*pci_callback_t)(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data);

/**
 * @brief PCI scan callback function
 * @param device The device currently being checked
 * @param data Driver-specific
 * @returns 1 on an error, 0 on success
 */
typedef int (*pci_scan_callback_t)(pci_device_t *device, void *data);

/**
 * @brief PCI VID:PID(s) mapping
 */
typedef struct pci_id_pid_mapping {
    uint16_t vid;                   // Vendor ID
    uint16_t pid[PCI_MAX_PID+1];    // PCI_NONE-terminated of accepted PIDs. If there is only one and VID matches, it will be accepted
} pci_id_mapping_t;

/**
 * @brief PCI scan parameters
 * @param class_code Class code being scanned for, leave as 0 to ignore
 * @param subclass_code Subclass code being scanned for, leave as 0 to ignore
 * @param id_list List of VID:PID(s) mappings to accept, leave as NULL to ignore. SHOULD END WITH PCI_ID_MAPPING_END
 */
typedef struct pci_scan_parameters {
    uint8_t class_code;
    uint8_t subclass_code;
    pci_id_mapping_t *id_list;
} pci_scan_parameters_t;


/**
 * @brief PCI slot
 */
typedef struct pci_slot {
    pci_device_t functions[PCI_MAX_FUNC];
} pci_slot_t;

/**
 * @brief PCI bus
 */
typedef struct pci_bus {
    pci_slot_t slots[PCI_MAX_SLOT];
} pci_bus_t;

typedef struct pci_msix_entry {
    uint32_t msg_addr_low;              // Low bits of message address
    uint32_t msg_addr_high;             // High bits of message address
    uint32_t msg_data;                  // Message data
    uint32_t vector_ctrl;               // Vector control
} pci_msix_entry_t;

/**** MACROS ****/

// Macro for help translating a bus/slot/function/offset to an address that can be written to PCI_CONFIG_ADDRESS
#define PCI_ADDR(bus, slot, func, offset) (uint32_t)(((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000))

// Macros for extracting values from PCI_ADDR()
#define PCI_FUNCTION(address) (uint8_t)((address >> 8) & 0xFF)
#define PCI_SLOT(address) (uint8_t)((address >> 11) & 0xFF)
#define PCI_BUS(address) (uint8_t)((address >> 16) & 0xFF)

// Ending mapping
#define PCI_ID_MAPPING_END { .pid = { PCI_NONE }, .vid = PCI_NONE }
#define PCI_PID_ACCEPT_ALL { PCI_NONE }

/**** FUNCTIONS ****/

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
uint32_t pci_readConfigOffset(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, int size);

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
int pci_writeConfigOffset(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value, int size);

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
pci_bar_t *pci_readBAR(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar);

/**
 * @brief Read the type of the PCI device (class code + subclass)
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the type
 */
uint16_t pci_readType(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Read the device ID of a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the device ID
 */
uint16_t pci_readDeviceID(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Read the vendor ID of a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the vendor ID
 */
uint16_t pci_readVendorID(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Read the type of the PCI device (class code + subclass)
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the type
 */
uint16_t pci_readType(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Get the interrupt registered to a PCI device
 * 
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * 
 * @returns PCI_NONE or the interrupt ID
 */
uint8_t pci_getInterrupt(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Get MSI interrupts
 * @param bus The bus of the PCI device
 * @param slot The slot of the PCI device
 * @param func The function of the PCI device
 * @returns 0xFF or the interrupt ID
 */
uint8_t pci_enableMSI(uint8_t bus, uint8_t slot, uint8_t func);

/**
 * @brief Initialize and probe for PCI devices
 */
void pci_init();

/**
 * @brief Scan for PCI devices
 * @param callback Callback function
 * @param parameters Scan parameters (optional, leave as NULL if you don't care)
 * @param data Driver-specific data to pass along
 * @returns 1 on an error, 0 on success
 */
int pci_scanDevice(pci_scan_callback_t callback, pci_scan_parameters_t *parameters, void *data); 

/**
 * @brief Get a device from bus/slot/function
 * @param bus The bus to check
 * @param slot The slot to check
 * @param function The function to check
 * @returns Device object or NULL
 */
pci_device_t *pci_getDevice(uint8_t bus, uint8_t slot, uint8_t function);

#endif