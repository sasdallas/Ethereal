/**
 * @file hexahedron/include/kernel/mem/regions.h
 * @brief Regions handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_REGIONS_H
#define KERNEL_MEM_REGIONS_H

/**** INCLUDES ****/
#include <stdint.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the regions system
 * 
 * Call this after your memory system is fully initialized (heap ready)
 */
void mem_regionsInitialize();

#endif