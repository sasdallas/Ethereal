/**
 * @file hexahedron/task/syscalls/kill.c
 * @brief kill
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

long sys_kill(pid_t pid, int sig) {
    // Check signal
    if (sig < 0 || sig >= NSIG) return -EINVAL;

    if (pid > 0 || pid < -1) {
        if (pid < -1) pid *= -1;

        // !!! racey
        process_t *proc = process_getFromPID(pid);
        if (!proc) return -ESRCH;
        signal_send(proc, sig);
        return 0;
    } else if (!pid) {
        SYSCALL_LOG(ERR, "Unimplemented: Send to every process group\n");
        return -ENOTSUP;
    } else if (pid == -1) {
        // TODO
        SYSCALL_LOG(ERR, "Unimplemented: Send to every process possible\n");
        return -ENOTSUP;
    }

    // Unreachable
    return -EINVAL;
}
