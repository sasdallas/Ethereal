/**
 * @file hexahedron/task/syscalls/uname.c
 * @brief uname
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

long sys_uname(struct utsname *buf) {
    SYSCALL_VALIDATE_PTR(buf);
    
    snprintf(buf->sysname, 128, "Hexahedron");
    snprintf(buf->nodename, 128, __hostname);
    snprintf(buf->release, 128, "%d.%d.%d-%s", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower,
                    __kernel_build_configuration);

    snprintf(buf->version, 128, "%s %s %s",
        __kernel_version_codename,
        __kernel_build_date,
        __kernel_build_time);

    snprintf(buf->machine, 128, "%s", __kernel_architecture);

    return 0;
}
