/**
 * @file hexahedron/task/syscalls/sigaction.c
 * @brief sigaction
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

long sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oact) {
    return signal_action(signum, (struct sigaction *)act, oact);
}
