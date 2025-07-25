/**
 * @file libpolyhedron/errno/errno.c
 * @brief Defines errno
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <errno.h>
#include <pthread.h>

int *__errno_location() {
    return &__get_tcb()->_errno;
}