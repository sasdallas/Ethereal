/**
 * @file hexahedron/include/kernel/drivers/grubvid.h
 * @brief GRUB framebuffer driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_GRUBVID_H
#define DRIVERS_GRUBVID_H

/**** INCLUDES ****/
#include <kernel/drivers/fb.h>
#include <kernel/generic_mboot.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the GRUB framebuffer driver
 * @param parameters Generic parameters
 * @returns 0 on success
 */
int grubvid_initialize(generic_parameters_t *parameters);

#endif
