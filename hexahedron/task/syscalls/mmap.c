/**
 * @file hexahedron/task/syscalls/mmap.c
 * @brief mmap, munmap, and friends.
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
#include <kernel/misc/util.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <errno.h>

long sys_mmap(sys_mmap_context_t *context) {
    void *addr = context->addr;
    size_t len = context->len;
    off_t off = context->off;
    int prot = context->prot;
    int flags = context->flags;
    int filedes = context->filedes;

    if (!((flags & MAP_PRIVATE) || (flags & MAP_SHARED))) {
        return -EINVAL;
    }

    if ((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) {
        return -EINVAL;
    }

    if (len == 0) {
        return -EINVAL;
    }

    if ((uintptr_t)addr >= MMU_USERSPACE_END) {
        return -EINVAL;
    }


    int file = ((flags & MAP_ANONYMOUS) == 0);
    if (file) {
        if (!FD_VALIDATE(filedes)) {
            return -EBADF;
        }
    }

    if (!addr) addr = (void*)PROCESS_MMAP_MINIMUM;
    addr = (void*)PAGE_ALIGN_DOWN((uintptr_t)addr);

    len = PAGE_ALIGN_UP(len);
    off = PAGE_ALIGN_UP(off);

    mmu_flags_t mmu_flags = vmm_toMMU(prot);
    vmm_flags_t vm_flags =  ((flags & MAP_FIXED) ? VM_FLAG_REPLACE : 0) |
                            ((file) ? VM_FLAG_FILE : 0) |
                            ((flags & MAP_SHARED) ? VM_FLAG_SHARED : 0) |
                            VM_FLAG_ALLOC;
    
    void *r;
    if (file) {
        vfs_file_t *fd = FD(filedes);
        
        // TODO: More checks when page cache added
        vmm_file_t f = {
            .node = fd,
            .offset = off,
            .path = NULL
        };

        if (f.node->inode->flags & INODE_FLAG_MMAP_UNSUPPORTED) {
            return -ENOTSUP;
        }

        SYSCALL_LOG(DEBUG, "mmap process %p %d 0x%x 0x%x %s\n", addr, len, vm_flags, mmu_flags, f.path);

        r = vmm_map(addr, len, vm_flags, mmu_flags, &f);
    } else {
        r = vmm_map(addr, len, vm_flags, mmu_flags);
    }


    // SYSCALL_LOG(INFO, "mmap(%p, %d, 0x%x, 0x%x, %d, %d) = %p\n", context->addr, context->len, context->prot, context->flags, context->filedes, context->off, r);
    if (!r) {
        SYSCALL_LOG(ERR, "Out of memory for allocation at address %p length 0x%x\n", addr, len);
        return -ENOMEM;
    }
    
    return (long)r;
}

long sys_munmap(void *addr, size_t len) {
    // TODO: more checks
    if ((uintptr_t)addr > MMU_USERSPACE_END) return -EFAULT;
    if (((uintptr_t)addr & (PAGE_SIZE-1)) || len == 0) return -EINVAL; 
    vmm_unmap(addr, len);
    return 0;
}