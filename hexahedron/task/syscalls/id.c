/**
 * @file hexahedron/task/syscalls/id.c
 * @brief get/set id system calls
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

pid_t sys_getpid() {
    return current_cpu->current_process->pid;
}


uid_t sys_getuid() {
    return current_cpu->current_process->uid;
}

int sys_setuid(uid_t uid) {
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        current_cpu->current_process->uid = uid;
        current_cpu->current_process->euid = uid;
        return 0;
    } 

    return -EPERM;
}

gid_t sys_getgid() {
    return current_cpu->current_process->gid;
}

int sys_setgid(gid_t gid) {
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        current_cpu->current_process->gid = gid;
        current_cpu->current_process->egid = gid;
        return 0;
    }

    return -EPERM;
}

pid_t sys_getppid() {
    if (current_cpu->current_process->parent) {
        return current_cpu->current_process->parent->pid;
    }

    return 0;
}

pid_t sys_getpgid(pid_t pid) {
    if (pid < 0) return -EINVAL;
    if (!pid) return current_cpu->current_process->pgid;
    
    process_t *p = process_getFromPID(pid);
    if (!p) return -ESRCH;

    return p->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    if (pid < 0) return -EINVAL;
    
    process_t *p = current_cpu->current_process;

    if (pid) {
        p = process_getFromPID(pid);
        if (!p) return -ESRCH;
    }

    if (p->sid != current_cpu->current_process->sid || p->sid == p->pid) {
        return -EPERM; // Attempt to change process in a different session or session leader
    }

    if (!pgid) {
        p->pgid = p->pid;
    } else {
        // Validate PGID
        process_t *valid = process_getFromPID(pgid);
        if (!valid || valid->sid != p->sid) return -EPERM;
        
        p->pgid = pgid;
    }

    return 0;
}

pid_t sys_getsid() {
    return current_cpu->current_process->sid;
}

pid_t sys_setsid() {
    if (current_cpu->current_process->sid == current_cpu->current_process->pid) return -EPERM;
    current_cpu->current_process->sid = current_cpu->current_process->pid;
    return 0;
}

uid_t sys_geteuid() {
    return current_cpu->current_process->euid;
}

int sys_seteuid(uid_t uid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && uid != current_cpu->current_process->uid) {
        // Nope
        return -EPERM;
    }

    current_cpu->current_process->euid = uid;
    return 0;
}

gid_t sys_getegid() {
    return current_cpu->current_process->egid;
}

int sys_setegid(gid_t gid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && gid != current_cpu->current_process->gid) {
        return -EPERM;
    }

    current_cpu->current_process->egid = gid;
    return 0;
}
