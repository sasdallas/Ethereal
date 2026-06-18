/**
 * @file hexahedron/task/syscalls/hostname.c
 * @brief gethostname and sethostname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>

extern char *__hostname;
extern size_t __hostnamelen;

long sys_gethostname(char *name, size_t size) {
    SYSCALL_VALIDATE_PTR_SIZE(name, size);

    memcpy(name, __hostname, (size > __hostnamelen) ? __hostnamelen : size);

    if (size < __hostnamelen) return -ENAMETOOLONG;
    return 0;
}

long sys_sethostname(const char *name, size_t size) {
    if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;
    SYSCALL_VALIDATE_PTR_SIZE(name, size);
    if (size > 256) return -EINVAL;

    // Set the hostname
    memcpy(__hostname, name, size);
    __hostnamelen = size;
    
    return 0;
}
