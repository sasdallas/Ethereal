/**
 * @file hexahedron/include/kernel/kernel.h
 * @brief Main kernel header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H


/**** VARIABLES ****/

/* Set when the kernel is beginning to shutdown */
extern int kernel_shutdown;

/**** FUNCTIONS ****/

/**
 * @brief Main kernel function - start of generic routines
 */
void kmain();

/**
 * @brief Kernel prepare for new power state
 */
void kernel_prepareForPowerState(int state);

#endif