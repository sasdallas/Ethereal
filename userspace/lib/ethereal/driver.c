/**
 * @file userspace/lib/ethereal/driver.c
 * @brief Ethereal driver system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/driver.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define SYSCALL3(name, num, p1_type, p2_type, p3_type) \
    __syscall_prefix __syscall_##name(p1_type p1, p2_type p2, p3_type p3) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "D"((long)(p1)), "S"((long)(p2)), "d"((long)(p3)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

SYSCALL3(load_driver, SYS_LOAD_DRIVER, char*, int, char**);
DEFINE_SYSCALL1(unload_driver, SYS_UNLOAD_DRIVER, pid_t);
DEFINE_SYSCALL2(get_driver, SYS_GET_DRIVER, pid_t, ethereal_driver_t*);

pid_t ethereal_loadDriver(char *filename, int priority, char **argv) {
    __sets_errno(__syscall_load_driver(filename, priority, argv));
}

int ethereal_unloadDriver(pid_t id) {
    __sets_errno(__syscall_unload_driver(id));
}

ethereal_driver_t *ethereal_getDriver(pid_t id) {
    ethereal_driver_t *driver = malloc(sizeof(ethereal_driver_t));
    memset(driver, 0, sizeof(ethereal_driver_t));

    long r = __syscall_get_driver(id, driver);

    if (r < 0) {
        errno = -r;
        return NULL;
    }

    return driver;
}