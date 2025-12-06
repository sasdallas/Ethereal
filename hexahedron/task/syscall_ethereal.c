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
    return sharedfs_new(current_cpu->current_process, size, flags);
}

key_t sys_ethereal_shared_key(int fd) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) return -EBADF;
    return sharedfs_key(FD(current_cpu->current_process, fd)->node);
}

long sys_ethereal_shared_open(key_t key) {
    return sharedfs_openFromKey(current_cpu->current_process, key);
}

/**** PTHREAD API ****/

long sys_create_thread(uintptr_t stack, uintptr_t tls, void *entry, void *arg) {
    return process_createThread(stack, tls, entry, arg);   
}

long sys_exit_thread(void *retval) {
    // Notify joined threads of our exit
    spinlock_acquire(&current_cpu->current_thread->joiner_lck);

    current_cpu->current_thread->retval = retval;

    if (current_cpu->current_thread->joiners) {
        foreach(thr_node, current_cpu->current_thread->joiners) {
            sleep_wakeup(((thread_t*)thr_node->value));
        }
    
        // all entries in the thread list are stack anyways
    }

    // Mark this thread as stopped
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPED);
    spinlock_release(&current_cpu->current_thread->joiner_lck);

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
    // Find the thread by the TID
    if (!current_cpu->current_process->thread_list) return -ESRCH; // Can't join the main thread, buddy.
    if (retval) SYSCALL_VALIDATE_PTR(retval);

    thread_t *target = NULL;
    foreach(t_node, current_cpu->current_process->thread_list) {
        thread_t *t = (thread_t*)t_node->value;
        if (t->tid == tid) {
            target = t;
            break;
        }
    }

    if (!target) return -ESRCH;
    if (target == current_cpu->current_thread) return -EDEADLK; // Nice try wise guy

    spinlock_acquire(&target->joiner_lck);

    // Has the thread exited already?
    if (target->status & THREAD_STATUS_STOPPED) {
        // Yep, set retval and let it go
        spinlock_release(&target->joiner_lck);
        
        if (retval) *retval = target->retval;
        return 0;
    }

    // Nope, we should add ourselves to the queue
    if (!target->joiners) target->joiners = list_create("thread joiners");
    node_t n = { .value = current_cpu->current_thread };
    sleep_prepare(current_cpu->current_thread);
    list_append_node(target->joiners, &n);
    spinlock_release(&target->joiner_lck);

    // Enter sleep
    int w = sleep_enter();

    // TODO: Spec says NEVER to do this...
    if (w == WAKEUP_SIGNAL) return -EINTR;

    // Ok, set retval
    if (retval) *retval = target->retval;
    return 0;
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
    fs_node_t *n = kopen_user(filename, O_RDONLY);
    if (!n) return -ENOENT;
    

    // Load driver
    int r = driver_load(n, priority, filename, argc, argv);
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