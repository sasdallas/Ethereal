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
    if (!current_cpu->current_process || !current_cpu->current_process->vas) {
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

    // Get VAS
    vas_t *vas = current_cpu->current_process->vas;

    // If protection flags were provided - we don't care.
    if (!(prot & PROT_WRITE)) {
        LOG(WARN, "Protection flags are not implemented\n");
    }

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

    // Do they need a fixed allocation?
    if (flags & MAP_FIXED) {
        if (!IN_RANGE((uintptr_t)addr, PROCESS_MMAP_MINIMUM, (vas->base + vas->size))) {
            // Out of range, die.
            return (void*)-EINVAL;
        }
        
        // See if the VAS already has an allocation
        // TODO: MAP_FIXED_NOREPLACE (?)
        if (vas_get(vas, (uintptr_t)addr)) {
            // TODO: Clobber this allocation
            LOG(ERR, "mmap allocation for %p - %p failed - region already present in process VAS\n", addr, addr + len);
            kfree(map);
            return (void*)-EINVAL;
        }

        // Reserve memory in the VAS
        vas_allocation_t *alloc = vas_reserve(vas, (uintptr_t)addr, len, (flags & MAP_SHARED) ? VAS_ALLOC_MMAP_SHARE : VAS_ALLOC_MMAP);
        if (alloc) {
            list_append(current_cpu->current_process->mmap, (void*)map);
            return (void*)alloc->base;
        }

        return (void*)-ENOMEM;
    }

    // addr was specified, but MAP_FIXED was not specified - this means they
    // want us to interpret it as a hint
    // TODO: care
    if (addr) {
        LOG(WARN, "Blatantly ignoring address hint: %p\n", (uintptr_t)addr);
    }

    // Now let's get an allocation in the directory.
    vas_allocation_t *alloc = vas_allocate(vas, len);
    if (!alloc) {
        // No memory?
        kfree(map);
        return (void*)-ENOMEM;
    }

    // TODO: Protect allocation
    alloc->type = (flags & MAP_SHARED) ? VAS_ALLOC_MMAP_SHARE : VAS_ALLOC_MMAP;
    map->addr = (void*)alloc->base;

    // Did the user request a MAP_ANONYMOUS and/or specify a file descriptor of -1? If so we're done
    if (filedes == -1 || flags & MAP_ANONYMOUS) {
        list_append(current_cpu->current_process->mmap, (void*)map);
        return (void*)alloc->base; // All done
    }

    // No - call fs_mmap()
    int mmap_result = fs_mmap(FD(current_cpu->current_process, filedes)->node, (void*)alloc->base, len, off);
    if (mmap_result < 0) {
        vas_free(vas, vas_get(vas, alloc->base), 0);
        kfree(map);
        return (void*)(uintptr_t)mmap_result;
    }

    // Success!
    list_append(current_cpu->current_process->mmap, (void*)map);
    return (void*)alloc->base;
}

/**
 * @brief Remove a mapping from a process (faster munmap())
 * @param proc The process to remove the mapping for
 * @param map The mapping to remove from the process
 */
int process_removeMapping(process_t *proc, process_mapping_t *map) {
    if (!map) return 0;

    // If there was a file descriptor, close it
    int munmapped = 0;
    if (!(map->flags & MAP_ANONYMOUS) && map->filedes) {
        // Sometimes a proc will close a file descriptor before it exited
        if (FD_VALIDATE(proc, map->filedes)) {
            munmapped = !fs_munmap(FD(proc, map->filedes)->node, map->addr, map->size, map->off);
        }
    }

    // Free the memory in the VAS
    vas_free(proc->vas, vas_get(proc->vas, (uintptr_t)map->addr), munmapped);

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
    if (!current_cpu->current_process->mmap) return -EINVAL;

    // Find a corresponding mapping
    foreach(map_node, current_cpu->current_process->mmap) {
        process_mapping_t *map = (process_mapping_t*)(map_node->value);
        if (RANGE_IN_RANGE((uintptr_t)addr, (uintptr_t)addr+len, (uintptr_t)map->addr, (uintptr_t)map->addr + map->size)) {
            // TODO: "Close enough" system? 

            if ((uintptr_t)map->addr != (uintptr_t)addr || map->size != len) {
                LOG(ERR, "Partial munmap (%p - %p) of mapping %p - %p\n", addr, (uintptr_t)addr + len, map->addr, (uintptr_t)map->addr + map->size);
                return -ENOSYS;
            }


            return process_removeMapping(current_cpu->current_process, map);
        }
    }

    return -EINVAL;
}