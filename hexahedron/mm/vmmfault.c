/**
 * @file hexahedron/mm/vmmfault.c
 * @brief VMM fault resolution code
 *
 *
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 *
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/vmm.h>
#include <kernel/processor_data.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>

#define LOG(status, ...) dprintf_module(status, "MM:VMM:FAULT", __VA_ARGS__)
#pragma GCC diagnostic ignored "-Wtype-limits" // SHUT THE FUCK UP

/* start with 16 page backing */
#define VMM_FAULT_BASELINE          16
#define VMM_FAULT_CAP               1024

extern uintptr_t vmm_zero_page;

/**
 * @brief File memory faultahead callback
 */
static ssize_t vmm_backFileFault(uintptr_t addr, size_t npages, vmm_memory_range_t *r, void *context) {
    vmm_space_t *sp = context;
    uint64_t offset = (uint64_t)r->file.offset + (addr - r->start);
    vfs_file_t *file = r->file.node;
    mmu_flags_t mmu_flags = r->mmu_flags;

    if (r->file.offset < 0 || offset < (uint64_t)r->file.offset) {
        return -EFAULT;
    }

    // fix npages to EOF, but allow devices with their own mmap() to override that
    if (!file->ops->mmap) {
        if (offset >= (uint64_t)file->inode->attr.size) return -EFAULT;
        uint64_t bytes = (uint64_t)file->inode->attr.size - offset;
        npages = min(npages, (size_t)(bytes / PAGE_SIZE + !!(bytes % PAGE_SIZE)));
    }

    // Send anything with a private mapping directly to the CoW code
    if ((r->vmm_flags & VM_FLAG_SHARED) == 0) {
        mmu_flags &= ~(MMU_FLAG_WRITE);
    } else if (!file->ops->mmap && VFS_CACHEABLE(file->inode)) {
        mmu_flags &= ~(MMU_FLAG_WRITE);
    }

    if (file->ops->mmap) {
        // The file provides an mmap method
        int res = file_mmap(file, (void*)addr, npages*PAGE_SIZE, offset, mmu_flags);
        if (res) {
            LOG(ERR, "Mapping pages in failed (error code %d)\n", res);
            return res;
        }
    } else if (VFS_CACHEABLE(file->inode)) {
        // Pin the file while the space lock is dropped, then capture a space mapping generation
        // The mapping generation allows us to lose the space lock while still being sure nothing has changed.
        uint64_t generation = sp->mapping_generation;
        file_hold(file);
        mutex_release(sp->mut);

        CACHE_DEFINE_PAGE_RANGE(page_range, 32);
        cache_ensure(file->inode);
        int res = cache_getRange(file->inode, offset, npages, page_range, 0);

        mutex_acquire(sp->mut);
        file_release(file);

        // The race with changed is not very likely even in bigger multi-threaded programs like WebKitGTK
        // If it happens the fault is retried
        bool changed = generation != sp->mapping_generation;
        if (!changed) {
            for (size_t i = 0; i < npages; i++) {
                if (arch_mmu_physical(NULL, addr + i * PAGE_SIZE)) {
                    changed = true;
                    break;
                }
            }
        }

        if (res || changed) {
            // cache_getRange can retain pages even when it reports an error
            return changed ? -EAGAIN : res;
        }

        for (size_t i = 0; i < npages; i++) {
            arch_mmu_map(NULL, addr + i * PAGE_SIZE, page_range->pages[i], mmu_flags);
        }
    } else {
        LOG(ERR, "vmm_backFileFault is F.U.B.A.R\n");
        return -ENOTSUP;
    }

    return npages;
}

/**
 * @brief Helper to back anon memory
 */
static ssize_t vmm_backAnonFault(uintptr_t addr, size_t npages, vmm_memory_range_t *r, void *context) {
    bool use_zero_page = *(bool*)context;
    for (size_t i = 0; i < npages; i++) {
        uintptr_t page;
        mmu_flags_t flags;        
        if (use_zero_page) {
            page = vmm_zero_page;
            pmm_retain(page);

            flags = r->mmu_flags & ~(MMU_FLAG_WRITE);
        } else {
            page = pmm_allocatePageFlags(ZONE_DEFAULT, PMM_ALLOC_ZEROED);
            flags = r->mmu_flags;
        }

        arch_mmu_map(NULL, addr + i * PAGE_SIZE, page, flags);
    }

    return (ssize_t)npages;
}

/**
 * @brief Helper to faultahead
 */
static ssize_t vmm_backFault(vmm_fault_information_t *info, vmm_space_t *sp, vmm_memory_range_t *r, ssize_t (*callback)(uintptr_t,size_t,vmm_memory_range_t*,void*), void *context) {
    uintptr_t addr = PAGE_ALIGN_DOWN(info->address);

    // Check the space fault information
    size_t to_back = VMM_FAULT_BASELINE;
    if (r->fault_info.sequential_hits != 0) {
        // Check if this is sequential
        size_t filled_last = r->fault_info.last_filled * PAGE_SIZE;
        if (addr >= filled_last && r->fault_info.last_fault == addr - filled_last) {
            // Forwards
            r->fault_info.direction = 1;
            to_back = r->fault_info.cluster_size;
        } else if ((addr <= UINTPTR_MAX - filled_last && r->fault_info.last_fault == addr + filled_last) ||
                   (addr <= UINTPTR_MAX - PAGE_SIZE && r->fault_info.last_fault == addr + PAGE_SIZE)) {
            // Backwards
            r->fault_info.direction = 0;
            to_back = r->fault_info.cluster_size;
        } else {
            // Reset
            r->fault_info.direction = 1;
            r->fault_info.sequential_hits = 0;
            r->fault_info.cluster_size = VMM_FAULT_BASELINE;
        }
    } else {
        r->fault_info.direction = 1;
        r->fault_info.cluster_size = VMM_FAULT_BASELINE;
    }


    // Go ahead now
    to_back = min(to_back, (size_t)32);
    if ((r->vmm_flags & VM_FLAG_FILE) && r->file.node->ops->mmap) to_back = 1;

    if (r->fault_info.direction == 1) {
        // Forwards
        to_back = min(to_back, (r->end - addr) / PAGE_SIZE);

        // Back up to the next present page
        for (size_t i = 1; i < to_back; i++) {
            if (arch_mmu_read_flags(NULL, addr + i * PAGE_SIZE) & MMU_FLAG_PRESENT) {
                to_back = i;
                break;
            }
        }

        ssize_t ret = callback(addr, to_back, r, context);
        if (ret <= 0) {
            LOG(ERR, "Backing pages failed with error %d\n", ret);
            return ret;
        }

        to_back = (size_t)ret;
    } else {
        to_back = min(to_back, ((addr - r->start) / PAGE_SIZE)+1);

        // Back up to the next present page
        for (size_t i = 1; i < to_back; i++) {
            if (arch_mmu_read_flags(NULL, addr - i * PAGE_SIZE) & MMU_FLAG_PRESENT) {
                to_back = i;
                break;
            }
        }

        ssize_t ret = callback(addr - ((to_back-1) * PAGE_SIZE), to_back, r, context);
        if (ret < 0) {
            LOG(ERR, "Backing pages failed with error %d\n", ret);
            return ret;
        }

        // A partial backward fill may not have reached the faulting page
        if (!(arch_mmu_read_flags(NULL, addr) & MMU_FLAG_PRESENT)) return -EIO;
        to_back = (size_t)ret;
    }

    if (!(arch_mmu_read_flags(NULL, addr) & MMU_FLAG_PRESENT)) return -EIO;

    r->fault_info.sequential_hits = 1;
    r->fault_info.last_filled = to_back;
    r->fault_info.last_fault = addr;
    r->fault_info.cluster_size = min(to_back * 2, VMM_FAULT_CAP);
    return (ssize_t)to_back;
}

/**
 * @brief Try to handle VMM fault
 * @param info Fault information
 * @returns VMM_FAULT_RESOLVED on success and VMM_FAULT_UNRESOLVED on failure
 */
int vmm_fault(vmm_fault_information_t *info) {
    // The reasons why we don't support lazy kernel allocations is mainly philosophical:
    // I believe that the kernel should keep itself optimized enough to use all of the pages
    // which it allocates, and that page faults will simply slow down the system's running performance.
    // Just philosophical. Might add a config option for it.
    if (info->address < MMU_USERSPACE_START || info->address >= MMU_USERSPACE_END) return VMM_FAULT_UNRESOLVED;

    info->address = PAGE_ALIGN_DOWN(info->address);
    if (info->from == VMM_FAULT_FROM_KERNEL && info->address < PAGE_SIZE) return VMM_FAULT_UNRESOLVED;

    // Okay, the address is within the userspace. Get the space..
    vmm_space_t *sp = vmm_getSpaceForAddress((void*)info->address);
    if (!sp) return VMM_FAULT_UNRESOLVED;
    mutex_acquire(sp->mut);

retry_fault:
    // Get the range for it.
    vmm_memory_range_t *r = vmm_getRange(sp, info->address, 1);
    if (!r) {
        // Perhaps this is part of the thread stack?
        if (info->address >= MMU_USERMODE_STACK_REGION && info->address < MMU_USERMODE_STACK_REGION + MMU_USERMODE_STACK_SIZE) {
            // Yes it is, expand the stack
            mutex_release(sp->mut);
            assert(vmm_map((void*)PAGE_ALIGN_DOWN(info->address), PAGE_SIZE, VM_FLAG_FIXED | VM_FLAG_ALLOC, MMU_FLAG_USER | MMU_FLAG_WRITE | MMU_FLAG_PRESENT) == (void*)PAGE_ALIGN_DOWN(info->address));
            sp->metrics.stack += PAGE_SIZE;
            LOG(DEBUG, "Expanded thread stack!\n");
            return VMM_FAULT_RESOLVED;
        }

        LOG(WARN, "No range contains %p - fault resolution FAILED\n", info->address);
        mutex_release(sp->mut);
        return VMM_FAULT_UNRESOLVED;
    }

    // Determine the action they were trying to accomplish
    int actions = 0;
    if (r->mmu_flags & MMU_FLAG_PRESENT) actions |= VMM_FAULT_NONPRESENT;
    if (r->mmu_flags & MMU_FLAG_WRITE) actions |= VMM_FAULT_WRITE;
    if (!(r->mmu_flags & MMU_FLAG_NOEXEC)) actions |= VMM_FAULT_EXECUTE;

    if (info->exception_type & ~(actions)) {
        LOG(WARN, "Cannot perform access on range - fault resolution failed\n");
        LOG(WARN, "Allowed access: %x Attempted: %x\n", actions, info->exception_type);
        LOG(WARN, "Range %p-%p VMM flags %x MMU flags %x, PTE flags %x\n",
            r->start, r->end, r->vmm_flags, r->mmu_flags,
            arch_mmu_read_flags(NULL, info->address));
        mutex_release(sp->mut);
        return VMM_FAULT_UNRESOLVED;
    }

    // Map in the page
    uint32_t fl = arch_mmu_read_flags(NULL, info->address);
    if (!(fl & MMU_FLAG_PRESENT)) {
        // Non-present - check the range
        if (r->vmm_flags & VM_FLAG_FILE) {
            vfs_file_t *file = r->file.node;
            if (!file->ops->mmap && VFS_CACHEABLE(file->inode)) {
                uint64_t offset = (uint64_t)r->file.offset + (info->address - r->start);
                if (r->file.offset < 0 || offset < (uint64_t)r->file.offset || offset >= (uint64_t)file->inode->attr.size) {
                    mutex_release(sp->mut);
                    return VMM_FAULT_BUS;
                }
            }

            ssize_t ret = vmm_backFault(info, sp, r, vmm_backFileFault, sp);
            if (ret == -EAGAIN) {
                goto retry_fault;
            }

            if (ret < 0 && !file->ops->mmap && VFS_CACHEABLE(file->inode)) {
                // It's theoretically possible that a neighboring page failed to load
                // but this page is fine
                // In that case as a hail mary try loading it again, else just die off.
                ret = vmm_backFileFault(info->address, 1, r, sp);
                if (ret == -EAGAIN) goto retry_fault;
            }

            if (ret < 0) {
                LOG(ERR, "Error handling page fault on file memory (%d)\n", ret);
                mutex_release(sp->mut);
                return ret == -ENXIO ? VMM_FAULT_BUS : VMM_FAULT_UNRESOLVED;
            }

            sp->metrics.file_resident += (ret * PAGE_SIZE);
        } else {
            // Typically when backing anonymous memory you will write to it, since
            // it should be zeroed when accessing it. If not, maybe you are just allocating some weird
            // pages? In whatever case we give them a shared zero page first, which can later be faulted off of
            // and cloned.
            bool use_zero_page = (r->vmm_flags & VM_FLAG_ALLOC) &&
                                 !(r->vmm_flags & (VM_FLAG_SHARED | VM_FLAG_DEVICE)) &&
                                 !(info->exception_type & VMM_FAULT_WRITE);

            ssize_t ret = vmm_backFault(info, sp, r, vmm_backAnonFault, &use_zero_page);
            if (ret < 0) {
                mutex_release(sp->mut);
                return VMM_FAULT_UNRESOLVED;
            }

            if (!use_zero_page) {
                sp->metrics.anon_resident += ret * PAGE_SIZE;
            }
        }
    } else if (!(fl & MMU_FLAG_WRITE)) {
        // Readable page but not writable
        // Is the range shared?
        uintptr_t phys = arch_mmu_physical(NULL, info->address);

        if (phys == vmm_zero_page) {
            // This is the zero page, meaning we need to duplicate it
            uintptr_t new_phys = pmm_allocatePageFlags(ZONE_DEFAULT, PMM_ALLOC_ZEROED);
            arch_mmu_map(NULL, info->address, new_phys, r->mmu_flags);
            arch_mmu_invalidate_range(info->address, info->address + PAGE_SIZE);
            pmm_release(phys);

            // The zero page doesn't count as anon_resident memory
            sp->metrics.anon_resident += PAGE_SIZE;
        } else if (r->vmm_flags & VM_FLAG_SHARED || r->vmm_flags & VM_FLAG_DEVICE || PMM_REFCOUNT(phys) == 1) {
            // This page is either shared/device memory or the second page in a CoW operation
            // In whatever case, we own it and we should have it.
            if ((r->vmm_flags & (VM_FLAG_FILE | VM_FLAG_SHARED)) == (VM_FLAG_FILE | VM_FLAG_SHARED) && !r->file.node->ops->mmap && VFS_CACHEABLE(r->file.node->inode)) {
                cache_markDirty(pmm_page(phys));
            }

            // Remap the page as writable
            arch_mmu_map(NULL, info->address, phys, r->mmu_flags);
            arch_mmu_invalidate_range(info->address, info->address + PAGE_SIZE);
        } else {
            // Create a copy of the page and release the old one
            uintptr_t new_phys = pmm_allocatePage(ZONE_DEFAULT);

            uintptr_t rm1 = arch_mmu_remap_physical(new_phys, PAGE_SIZE, REMAP_TEMPORARY);
            uintptr_t rm2 = arch_mmu_remap_physical(phys, PAGE_SIZE, REMAP_TEMPORARY);

            memcpy((void*)rm1, (void*)rm2, PAGE_SIZE);
            arch_mmu_unmap_physical(rm1, PAGE_SIZE);
            arch_mmu_unmap_physical(rm2, PAGE_SIZE);
            arch_mmu_map(NULL, info->address, new_phys, r->mmu_flags);
            arch_mmu_invalidate_range(info->address, info->address + PAGE_SIZE);

            pmm_release(phys);
        }

    } else {
        // assume that the fault was already resolved
        LOG(DEBUG, "Fault was already resolved\n");
    }

    mutex_release(sp->mut);
    return VMM_FAULT_RESOLVED;
}
