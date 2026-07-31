/**
 * @file hexahedron/task/syscall_ethereal.c
 * @brief Ethereal specific system calls
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/syscall.h>
#include <kernel/task/process.h>
#include <kernel/task/fd.h>
#include <kernel/fs/shared.h>
#include <kernel/arch/arch.h>
#include <kernel/debug.h>
#include <kernel/loader/driver.h>
#include <ethereal/driver.h>
#include <kernel/kernel.h>
#include <ethereal/reboot.h>
#include <kernel/hal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL:ETHEREAL", __VA_ARGS__)

/**** SHARED MEMORY API ****/

long sys_ethereal_shared_new(size_t size, int flags) {
    int f = sharedfs_new(current_cpu->current_process, size, flags);
    if (f >= 0) {
        LOG(INFO, "New shared memory object created (fd %d): %p\n", f, FD(f)->priv);
    }
    return f;
}

key_t sys_ethereal_shared_key(int fd) {
    if (!FD_VALIDATE(fd)) return -EBADF;
    return sharedfs_key(FD(fd));
}

long sys_ethereal_shared_open(key_t key) {
    return sharedfs_openFromKey(current_cpu->current_process, key);
}

/**** PTHREAD API ****/

long sys_create_thread(uintptr_t stack, uintptr_t tls, void *entry, void *arg) {
    return process_createUserThread(stack, tls, entry, arg);   
}

long sys_exit_thread(void *retval) {
    // Mark this thread as stopped
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPING);

    // Bye thread
    process_yield(0);
    return -1;
}

pid_t sys_gettid() {
    return current_cpu->current_thread->tid;
}

int sys_settls(uintptr_t tls) {
    SYSCALL_VALIDATE_PTR(tls);

    // Context won't reflect until next save/load cycle
    TLSBASE(current_cpu->current_thread->context) = tls;
    arch_set_tlsbase(tls);
    return 0;
}


long sys_join_thread(pid_t tid, void **retval) {
    assert(0);
}

long sys_kill_thread(pid_t tid, int sig) {
    LOG(ERR, "sys_kill_thread: UNIMPL\n");
    return 0;
}

/**** DRIVER API ****/

long sys_load_driver(char *filename, int priority, char **argv) {
    SYSCALL_VALIDATE_PTR(filename);
    SYSCALL_VALIDATE_PTR(argv);
    char **p = argv;
    int argc = 0;
    while (*p) {
        SYSCALL_VALIDATE_PTR(*p);
        p++;
        argc++;
    }

    if (priority > DRIVER_IGNORE) return -EINVAL;
    if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;

    // Open
    vfs_file_t *f;
    int r = vfs_open(filename, O_RDONLY, &f);
    if (r) return r;
    

    // Load driver
    r = driver_load(f, priority, filename, argc, argv);
    return r;
}

long sys_unload_driver(pid_t id) {
    // TODO
    LOG(ERR, "sys_unload_driver is unimplemented\n");
    return -ENOSYS;
}

long sys_get_driver(pid_t id, ethereal_driver_t *driver) {
    SYSCALL_VALIDATE_PTR(driver);
    if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;

    loaded_driver_t *d = driver_findByID(id);
    if (!d) return -ENOENT;

    strncpy(driver->filename, d->filename, 256);
    driver->base = d->load_address;
    driver->size = d->size;
    driver->id = d->id;

    if (d->metadata->author) strncpy(driver->metadata.author, d->metadata->author, 256);
    if (d->metadata->name) strncpy(driver->metadata.name, d->metadata->name, 256);
    
    return 0;
} 

/**** REBOOT API ****/

long sys_reboot(int operation) {
    if (operation < 0 || operation > REBOOT_TYPE_HIBERNATE) return -EINVAL;
    if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;

    // Disable interrupts
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    int state = 0;
    if (operation == REBOOT_TYPE_DEFAULT) {
        state = HAL_POWER_REBOOT;
    } else if (operation == REBOOT_TYPE_POWEROFF) {
        state = HAL_POWER_SHUTDOWN;
    } else if (operation == REBOOT_TYPE_HIBERNATE) {
        state = HAL_POWER_HIBERNATE;
    }

    kernel_prepareForPowerState(state);
    int r = hal_setPowerState(state);

    // Reboot failure
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
    return r;
}