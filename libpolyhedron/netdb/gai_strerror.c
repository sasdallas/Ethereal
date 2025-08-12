/**
 * @file libpolyhedron/netdb/gai_strerror.c
 * @brief strerror
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

const char *__gai_errno_strings[] = {
    [0]                 = "Success",
    [EAI_AGAIN]         = "The name could not be resolved at this time",
    [EAI_BADFLAGS]      = "The flags had an invalid value",
    [EAI_FAMILY]        = "A non-recoverable error occurred",
    [EAI_MEMORY]        = "There was a memory allocation failure",
    [EAI_NONAME]        = "The name does not resolve for the supplied parameters",
    [EAI_SERVICE]       = "The service passed was not recognized for the specified socket type",
    [EAI_SOCKTYPE]      = "The intended socket type was not recognized",
    [EAI_SYSTEM]        = "A system error occurred",
    [EAI_OVERFLOW]      = "An argument buffer overflowed"
};

const char *gai_strerror(int ecode) {
    if (ecode < 0 || (size_t)ecode > sizeof(__gai_errno_strings) / sizeof(char*)) return "Unknown error";
    return __gai_errno_strings[ecode];
}