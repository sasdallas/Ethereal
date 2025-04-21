/**
 * @file hexahedron/include/kernel/drivers/grubvid.h
 * @brief GRUB video driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_GRUBVID_H
#define DRIVERS_GRUBVID_H

/**** INCLUDES ****/
#include <kernel/drivers/video.h>
#include <kernel/generic_mboot.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the GRUB video driver
 * @param parameters Generic parameters containing the LFB driver.
 * @returns NULL on failure to initialize, else a video driver structure
 */
video_driver_t *grubvid_initialize(generic_parameters_t *parameters);



#endif