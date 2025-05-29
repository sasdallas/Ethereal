/**
 * @file libpolyhedron/socket/h_error.c
 * @brief h_error
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <netdb.h>
#include <stdio.h>

/* h_errno */
int h_errno = 0;

/* h_errno messages */
const char *__h_errno_strs[] = {
    [HOST_NOT_FOUND]        = "The specified host is unknown.",
    [NO_DATA]               = "The requested name is valid but does not have an IP address.",
    [NO_RECOVERY]           = "A nonrecoverable name server error occurred.",
    [TRY_AGAIN]             = "A temporary error occurred on an authoritative name server. Try again later."  
};

static char __h_errno_str[512];

const char *hstrerror(int err) {
    if ((unsigned long)err >= sizeof(__h_errno_strs) / sizeof(char*)) {
        snprintf(__h_errno_str, 512, "Unknown error (%d)", err);
    } else {
        snprintf(__h_errno_str, 512, "%s", __h_errno_strs[err]);
    }

    return __h_errno_str;
}

void herror(const char *s) {
    // !!!: I dont actually know the purpose of s
    fprintf(stderr, "%s: %s\n", s, hstrerror(h_errno));
}