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
    // mlibc
    return 0;

    mmu_flags_t target = ((prot & PROT_READ) ? MMU_FLAG_PRESENT : 0) |
                         ((prot & PROT_WRITE) ? MMU_FLAG_WRITE : 0) |
                         ((prot & PROT_EXEC) ? 0 : MMU_FLAG_NOEXEC) |
                         MMU_FLAG_USER; 


    SYSCALL_LOG(DEBUG, "Updating VM flags for region %p - %p to %x\n", addr, addr+len, prot);

    return vmm_update(addr, len, VM_OP_SET_FLAGS, target);
}