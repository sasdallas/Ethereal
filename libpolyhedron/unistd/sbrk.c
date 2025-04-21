/**
 * @file libpolyhedron/unistd/sbrk.c
 * @brief sbrk and brk
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

DEFINE_SYSCALL1(brk, SYS_BRK, void*);

/* Current brk() base */
void *curbrk = NULL;

int brk(void *addr) {
    if (curbrk == NULL) {
        // First, get curbrk. We need it for error handling
        curbrk = (void*)__syscall_brk(0x0);
    }

    // Try to increase heap space
    void *newbrk = (void*)__syscall_brk(addr);

    if (newbrk < addr) {
        errno = ENOMEM;
        return -1;
    }

    // Update curbrk, heap has been expanded
    curbrk = newbrk;
    return 0;
}

void *sbrk(intptr_t increment) {
    if (curbrk == NULL) {
        if (brk(0) < 0) {
            return (void*)-1;
        }
    }

    // Check validity of increment
    if ((increment > 0 && ((uintptr_t)curbrk + (uintptr_t)increment < (uintptr_t)curbrk)) || (increment < 0 && ((uintptr_t)curbrk < (uintptr_t)-increment))) {
        // Trying to cause an overflow or an underflow - nice try
        errno = ENOMEM;
        return (void*)-1;
    }

    // Increase heap size
    void *prevbrk = curbrk;
    if (brk(curbrk + increment) < 0) {
        return (void*)-1;
    }

    // Return previous heap position
    return prevbrk;
}