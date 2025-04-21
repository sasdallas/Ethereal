/**
 * @file drivers/ide/main.c
 * @brief Main driver logic of the IDE driver
 * 
 * Just a forwarder to @c ata.c
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "ata.h"
#include <kernel/loader/driver.h>

int ide_initialize(int argc, char **argv) {
    // Initialize and load the main sections of the IDE ATA driver
    ata_initialize();
    
    LOG(INFO, "IDE driver online and initialized\n");
    return 0;
}

int ide_deinit() {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "IDE ATA/ATAPI Driver",
    .author = "Samuel Stuart",
    .init = ide_initialize,
    .deinit = ide_deinit
};