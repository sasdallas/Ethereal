/**
 * @file hexahedron/task/syscalls/mprotect.c
 * @brief mprotect
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
#include <kernel/mm/vmm.h>
#include <sys/mman.h>

long sys_mprotect(void *addr, size_t len, int prot) {
    mmu_flags_t target = vmm_toMMU(prot);
    return vmm_update(addr, len, VM_OP_SET_FLAGS, target);
}
