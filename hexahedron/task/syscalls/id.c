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
    return current_cpu->current_process->cred.uid;
}

int sys_setuid(uid_t uid) {
    auth_cred_t *cred = &current_cpu->current_process->cred;
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        cred->uid = cred->euid = cred->suid = uid;
        return 0;
    }
    if (uid != cred->uid && uid != cred->suid) return -EPERM;
    cred->euid = uid;
    return 0;
}

int sys_setresuid(uid_t ruid, uid_t euid, uid_t suid) {
    auth_cred_t *cred = &current_cpu->current_process->cred;

    if (!PROC_IS_ROOT(current_cpu->current_process)) {
        // silly checks
        if (ruid != (uid_t)-1 && ruid != cred->uid && ruid != cred->euid && ruid != cred->suid) return -EPERM;
        if (euid != (uid_t)-1 && euid != cred->uid && euid != cred->euid && euid != cred->suid) return -EPERM;
        if (suid != (uid_t)-1 && suid != cred->uid && suid != cred->euid && suid != cred->suid) return -EPERM;
    }

    if (ruid != (uid_t)-1) cred->uid = ruid;
    if (euid != (uid_t)-1) cred->euid = euid;
    if (suid != (uid_t)-1) cred->suid = suid;

    return 0;
}

gid_t sys_getgid() {
    return current_cpu->current_process->cred.gid;
}

int sys_setgid(gid_t gid) {
    auth_cred_t *cred = &current_cpu->current_process->cred;
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        cred->gid = cred->egid = cred->sgid = gid;
        return 0;
    }

    if (gid != cred->gid && gid != cred->sgid) return -EPERM;
    cred->egid = gid;
    return 0;
}

int sys_setresgid(gid_t rgid, gid_t egid, gid_t sgid) {
    auth_cred_t *cred = &current_cpu->current_process->cred;
    if (!PROC_IS_ROOT(current_cpu->current_process)) {
        if (rgid != (gid_t)-1 && rgid != cred->gid && rgid != cred->egid && rgid != cred->sgid) return -EPERM;
        if (egid != (gid_t)-1 && egid != cred->gid && egid != cred->egid && egid != cred->sgid) return -EPERM;
        if (sgid != (gid_t)-1 && sgid != cred->gid && sgid != cred->egid && sgid != cred->sgid) return -EPERM;
    }
    if (rgid != (gid_t)-1) cred->gid = rgid;
    if (egid != (gid_t)-1) cred->egid = egid;
    if (sgid != (gid_t)-1) cred->sgid = sgid;
    return 0;
}

pid_t sys_getppid() {
    if (current_cpu->current_process->parent) {
        return current_cpu->current_process->parent->pid;
    }

    return 0;
}

pid_t sys_getpgid(pid_t pid) {
    if (pid < 0) return -EINVAL;
    process_t *process = pid ? process_getFromPID(pid) : current_cpu->current_process;
    if (!process) return -ESRCH;
    return session_getpgid(process);
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    return session_setpgid(pid, pgid);
}

pid_t sys_getsid() {
    return session_getsid(current_cpu->current_process);
}

pid_t sys_setsid() {
    return session_setsid();
}

uid_t sys_geteuid() {
    return current_cpu->current_process->cred.euid;
}

int sys_seteuid(uid_t uid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && uid != current_cpu->current_process->cred.uid && uid != current_cpu->current_process->cred.suid) {
        // Nope
        return -EPERM;
    }

    current_cpu->current_process->cred.euid = uid;
    return 0;
}

gid_t sys_getegid() {
    return current_cpu->current_process->cred.egid;
}

int sys_setegid(gid_t gid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && gid != current_cpu->current_process->cred.gid && gid != current_cpu->current_process->cred.sgid) {
        return -EPERM;
    }

    current_cpu->current_process->cred.egid = gid;
    return 0;
}
