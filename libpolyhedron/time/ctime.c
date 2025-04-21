/**
 * @file libpolyhedron/time/ctime.c
 * @brief ctime
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <time.h>

char *ctime(const time_t *ptr) {
    return asctime(localtime(ptr));
}