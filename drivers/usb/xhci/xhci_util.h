/**
 * @file drivers/usb/xhci/xhci_util.h
 * @brief Utility macros
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _XHCI_UTIL_H
#define _XHCI_UTIL_H

/**** INCLUDES ****/
#include <stddef.h>

/**** MACROS ****/

/* ISO C forbids blah blah blah */
#pragma GCC diagnostic ignored "-Wpedantic"

#define XHCI_TIMEOUT(condition, timeout) ({uint32_t t = timeout; while (!(condition) && t) t--; t;})

#endif