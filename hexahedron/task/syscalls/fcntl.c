/**
 * @file hexahedron/task/syscalls/fcntl.c
 * @brief fcntl
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <fcntl.h>

long sys_fcntl(int fd, int cmd, int extra) {
    vfs_file_t *file = GET_FD_OR_ERROR(fd);

    long ret = 0;
    switch (cmd) {
        case F_DUPFD: {
            int output;
            int e = fd_duplicate(fd, extra, &output);
        
            if (e != 0) ret = e;
            else ret = output;
            break;
        }

        case F_GETFD:
            ret = file->flags & O_CLOEXEC;
            break;;
       
        case F_SETFD:
            file->flags |= ((extra & FD_CLOEXEC) ? O_CLOEXEC : 0);
            ret = 0;
            break;

        case F_SETFL: {
            extra &= (O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT | O_NOATIME);
            long flags_last = file->flags;
            flags_last &= ~(O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT | O_NOATIME);
            flags_last |= extra;

            // TODO: Protect this
            file->flags = flags_last;
            file_check_flags(file);

            ret = 0;
            break;
        }

        case F_GETFL:
            return file->flags;

        case F_SETLK:
            ret = 0;
            break;

        default:
            SYSCALL_LOG(WARN, "Unimplemented fcntl() command: 0x%x\n", cmd);
            ret = -ENOSYS;
            break;
    }

    FD_FINISH(file);
    return ret;
}