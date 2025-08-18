/**
 * @file userspace/lib/include/ethereal/reboot.h
 * @brief Reboot 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _SYS_ETHEREAL_REBOOT_H
#define _SYS_ETHEREAL_REBOOT_H

/**** DEFINITIONS ****/

#define REBOOT_TYPE_DEFAULT                 0
#define REBOOT_TYPE_POWEROFF                1
#define REBOOT_TYPE_HIBERNATE               2

/**** FUNCTIONS ****/

int ethereal_reboot(int operation);

#endif