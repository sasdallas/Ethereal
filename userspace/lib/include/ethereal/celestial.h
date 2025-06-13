/**
 * @file userspace/lib/include/ethereal/celestial.h
 * @brief Celestial header file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_H
#define _CELESTIAL_H

/**** INCLUDES ****/
#include <ethereal/celestial/request.h>

/**** FUNCTIONS ****/

/**
 * @brief Create a new window in Ethereal (undecorated)
 * @param flags The flags to use when creating the window
 * @returns A window ID or -1
 */
wid_t celestial_createWindowUndecorated(int flags);

#endif