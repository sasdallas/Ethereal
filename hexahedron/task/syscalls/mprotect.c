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
    if (((uintptr_t)addr & (PAGE_SIZE - 1)) || len == 0) return -EINVAL;
    if ((uintptr_t)addr >= MMU_USERSPACE_END || len > MMU_USERSPACE_END - (uintptr_t)addr) return -ENOMEM;

    mmu_flags_t target = vmm_toMMU(prot);
    return vmm_update(addr, len, VM_OP_SET_FLAGS, target);
}
