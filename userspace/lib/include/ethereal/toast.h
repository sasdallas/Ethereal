/**
 * @file userspace/lib/include/ethereal/toast.h
 * @brief Ethereal toast daemon
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _TOAST_H
#define _TOAST_H

/**** DEFINITIONS ****/

#define TOAST_FLAG_NO_ICON              0x1

/**** TYPES ****/

typedef struct toast {
    unsigned char flags;
    char icon[64];
    char title[128];
    char description[128];
} toast_t;


#endif