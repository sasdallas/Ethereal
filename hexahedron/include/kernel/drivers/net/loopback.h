/**
 * @file hexahedron/include/kernel/drivers/net/loopback.h
 * @brief Loopback interface
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#ifndef DRIVERS_NET_LOOPBACK_H
#define DRIVERS_NET_LOOPBACK_H

/**** INCLUDES ****/
#include <kernel/drivers/net/ethernet.h>

/**** FUNCTIONS ****/

/**
 * @brief Install the loopback device
 */
void loopback_install();

#endif