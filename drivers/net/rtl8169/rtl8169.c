/**
 * @file drivers/net/rtl8169/rtl8169.c
 * @brief RTL1869 network card driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "rtl8169.h"
#include <kernel/drivers/pci.h>
#include <kernel/loader/driver.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:RTL8169", __VA_ARGS__)

/**
 * @brief Initialize a RTL8169 NIC
 */
int rtl8169_init(uint32_t device) {
    LOG(DEBUG, "Initializing a RTL8169 NIC (bus %d slot %d func %d)\n", PCI_BUS(device), PCI_SLOT(device), PCI_FUNCTION(device));
    return 0;
}

/**
 * @brief PCI scan method
 */
int rtl8169_scan(uint8_t bus, uint8_t slot, uint8_t function, uint16_t vendor_id, uint16_t device_id, void *data) {
    if (vendor_id == 0x10ec && (device_id == 0x8161 || device_id == 0x8168 || device_id == 0x8169)) {
        // RTL8169
        return rtl8169_init(PCI_ADDR(bus, slot, function, 0));
    }

    if ((vendor_id == 0x1259 && device_id == 0xc107) || (vendor_id == 0x1737 && device_id == 0x1032) || (vendor_id == 0x16ec && device_id == 0x0116)) {
        return rtl8169_init(PCI_ADDR(bus, slot, function, 0));
    }

    return 0;
}

/**
 * @brief Init method
 */
int driver_init(int argc, char *argv[]) {
    return pci_scan(rtl8169_scan, NULL, -1);
}

/**
 * @brief Deinit method
 */
int driver_deinit() {
    return 0;
}


struct driver_metadata driver_metadata = {
    .name = "Realtek RTL8169 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};