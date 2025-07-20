/**
 * @file drivers/usb/xhci/xhci.c
 * @brief xHCI controller driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "xhci.h"
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>

/**
 * @brief PCI scan method for xHCI
 */
int xhci_scan(pci_device_t *dev, void *data) {
    if (pci_readConfigOffset(dev->bus, dev->slot, dev->function, PCI_PROGIF_OFFSET, 1) == 0x30) {
        // xHCI controller found
        xhci_initController(dev);
    }

    return 0; // return 0 to find more
}

/**
 * @brief Driver init method
 */
int driver_init(int argc, char **argv) {
    // Do a scan
    pci_scan_parameters_t params = {
        .class_code = 0x0C,
        .subclass_code = 0x03,
        .id_list = NULL
    };

    return pci_scanDevice(xhci_scan, &params, NULL);
}

/**
 * @brief Driver deinit method
 */
int driver_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "xHCI Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};