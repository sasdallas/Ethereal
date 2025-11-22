/**
 * @file hexahedron/task/mem.c
 * @brief Handles shared memory and mmap()
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <kernel/panic.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:MEM", __VA_ARGS__)
#pragma GCC diagnostic ignored "-Wtype-limits"

/**
 * @brief Map a file into a process' memory space (mmap() equivalent)
 * @returns Negative error code
 * 
 * @todo This isn't a "fully compliant" mmap() for multiple reasons
 */
void *process_mmap(void *addr, size_t len, int prot, int flags, int filedes, off_t off) {
    if (!current_cpu->current_process) {
        return (void*)-EINVAL;
    }

    if (!((flags & MAP_PRIVATE) || (flags & MAP_SHARED))) {
        return (void*)-EINVAL;
    }

    // If needed, validate the file descriptor
    if (filedes && !(flags & MAP_ANONYMOUS)) {
        if (!FD_VALIDATE(current_cpu->current_process, filedes)) return (void*)-EBADF;
    }

    if (flags & MAP_SHARED) {
        LOG(WARN, "MAP_SHARED may unstable (more testing required)\n");
    }

    // If protection flags were provided - we don't care.
    if (!(prot & PROT_WRITE)) {
        LOG(WARN, "Protection flags are not implemented\n");
    }

    int anon = (filedes == -1 || flags & MAP_ANONYMOUS);

    // Make a new mapping structure
    process_mapping_t *map = kmalloc(sizeof(process_mapping_t));
    map->addr = addr;
    map->size = len;
    map->flags = flags;
    map->prot = prot;
    map->filedes = filedes;
    map->off = off;

    // If needed make the process' mmap list
    if (!current_cpu->current_process->mmap) {
        current_cpu->current_process->mmap = list_create("mmap list");
    } 

    // Destructively align address
    addr = (void*)(MEM_ALIGN_PAGE_DESTRUCTIVE((uintptr_t)addr));
    if (len & 0xFFF) len = PAGE_ALIGN_UP(len);

    // Get space
    vmm_space_t *sp = vmm_getSpaceForAddress(addr);

    // Do they need a fixed allocation?
    if (flags & MAP_FIXED) {
        if (!IN_RANGE((uintptr_t)addr, PROCESS_MMAP_MINIMUM, sp->end)) {
            // Out of range, die.
            return (void*)-EINVAL;
        }
        
        // See if there's already an allocation
        // TODO: MAP_FIXED_NOREPLACE (?)
        if (vmm_getRange(sp, (uintptr_t)addr, len)) {
            // TODO: Clobber this allocation
            LOG(ERR, "mmap allocation for %p - %p failed - region already present in process VAS\n", addr, addr + len);
            kfree(map);
            return (void*)-EINVAL;
        }

        // Use the
        void *b = vmm_map(addr, len, VM_FLAG_FIXED | VM_FLAG_ALLOC | (anon ? 0 : VM_FLAG_FILE), MMU_FLAG_RW | MMU_FLAG_PRESENT | MMU_FLAG_USER);
        if (b) {
            map->addr = b;
            if (filedes == -1 || flags & MAP_ANONYMOUS) {
                LOG(DEBUG, "MAP_FIXED mapping at %p\n", b);
                list_append(current_cpu->current_process->mmap, (void*)map);
                memset(b, 0, len);
                return b;
            }

            STUB();
        }

        LOG(ERR, "Error while reserving a region. Failing mmap of %d bytes with ENOMEM\n", len);

        return (void*)-ENOMEM;
    }

    // addr was specified, but MAP_FIXED was not specified - this means they
    // want us to interpret it as a hint
    // TODO: care
    if (addr) {
        LOG(WARN, "Blatantly ignoring address hint: %p\n", (uintptr_t)addr);
    }

    // Now let's get an allocation in the directory.
    void *alloc = vmm_map((void*)0x1000, len, VM_FLAG_ALLOC | VM_FLAG_DEFAULT | (anon ? 0 : VM_FLAG_FILE), MMU_FLAG_PRESENT | MMU_FLAG_RW | MMU_FLAG_USER);
    if (!alloc) {
        // No memory?
        LOG(ERR, "Error while allocating a region. Failing mmap of %d bytes with ENOMEM\n", len);
    
        kfree(map);
        return (void*)-ENOMEM;
    }

    map->addr = alloc;

    // Did the user request a MAP_ANONYMOUS and/or specify a file descriptor of -1? If so we're done
    if (filedes == -1 || flags & MAP_ANONYMOUS) {
        list_append(current_cpu->current_process->mmap, (void*)map);
        memset(alloc, 0, len);
        return alloc; // All done
    }

    // No - call fs_mmap()
    int mmap_result = fs_mmap(FD(current_cpu->current_process, filedes)->node, alloc, len, off);
    if (mmap_result < 0) {
        
        kfree(map);
        return (void*)(uintptr_t)mmap_result;
    }

    // Success!
    list_append(current_cpu->current_process->mmap, (void*)map);
    return alloc;
}

/**
 * @brief Remove a mapping from a process (faster munmap())
 * @param proc The process to remove the mapping for
 * @param map The mapping to remove from the process
 */
int process_removeMapping(process_t *proc, process_mapping_t *map) {
    if (!map) return 0;

    // If there was a file descriptor, close it
    if (!(map->flags & MAP_ANONYMOUS) && map->filedes != -1) {
        // Sometimes a proc will close a file descriptor before it exited
        if (FD_VALIDATE(proc, map->filedes)) {
            fs_munmap(FD(proc, map->filedes)->node, map->addr, map->size, map->off);
        }
    }

    // Free the memory in the VAS
    vmm_unmap(map->addr, map->size);

    // Cleanup
    if (proc->mmap) {
        list_delete(proc->mmap, list_find(proc->mmap, (void*)map));
    }
    
    kfree(map);
    return 0;
}

/**
 * @brief Unmap a file from a process' memory space (munmap() equivalent)
 * @returns Negative error code
 */
int process_munmap(void *addr, size_t len) {
    if (addr >= (void*)MMU_USERSPACE_END) return -EFAULT;
    LOG(DEBUG, "TRACE: process_munmap %p %d\n", addr, len);
    vmm_unmap(addr, len);
    return 0;
}

/**
 * @brief Destroy mapping list
 */
void process_destroyMappings(struct process *proc) {
}