/**
 * @file hexahedron/task/process.c
 * @brief Main process logic
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/arch/arch.h>
#include <kernel/loader/elf_loader.h>
#include <kernel/drivers/clock.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <structs/tree.h>
#include <structs/list.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Process tree */
tree_t *process_tree = NULL;

/* Global process list */
/* TODO: not */
list_t *process_list = NULL;

/* PID bitmap */
uint32_t *pid_bitmap = NULL;

/* Task switches */
volatile int task_switches = 0;

/* Reap queue */
list_t *reap_queue = NULL;
spinlock_t reap_queue_lock = { 0 };

/* Reaper thread */
process_t *reaper_proc = NULL;

/* Reaper function */
void process_reaper(void *ctx);

/* Helper macro to check if a process is in use */
/* !!!: Can fail */
#define PROCESS_IN_USE(proc)    ({ int in_use = 0; for (int i = 0; i < processor_count; i++) { if (processor_data[i].current_process == proc) { in_use = 1; break; } }; in_use; })

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:PROCESS", __VA_ARGS__)

/* Prototype */
pid_t process_allocatePID();

/**
 * @brief Initialize the process system, starting the idle process
 * 
 * This will NOT switch to the next task! Instead it will prepare the system
 * by creating the necessary structures and allocating an idle task for the
 * BSP.
 */
void process_init() {
    // Mark PID 0 as in use
    process_allocatePID(); // !!!

    // Initialize tree
    process_tree = tree_create("process tree");
    process_list = list_create("process list");

    // Initialize scheduler
    scheduler_init();

    // Initialize reap queue and reaper process
    reap_queue = list_create("process reap queue");
    reaper_proc = process_createKernel("reaper", 0, PRIORITY_MED, process_reaper, NULL);
    scheduler_insertThread(reaper_proc->main_thread);

    LOG(INFO, "Process system initialized\n");
}

/**
 * @brief Switch to the next thread in the queue
 * 
 * For CPU cores: This is jumped to immediately after AP initialization, specifically 
 * after the idle task has been created (through process_spawnIdleTask). It will automatically
 * enter the scheduling loop, and when a new process spawns the core can get it.
 * 
 * @warning Don't call this unless you know what you're doing. Use @c process_yield
 */
void __attribute__((noreturn)) process_switchNextThread() {
    // Get next thread in queue
    thread_t *next_thread = scheduler_get();
    if (!next_thread) {
        kernel_panic_extended(SCHEDULER_ERROR, "scheduler", "*** No thread was found in the scheduler (or something has been corrupted). Got thread %p.\n", next_thread);
    }

    // Update CPU variables
    current_cpu->current_thread = next_thread;
    current_cpu->current_process = next_thread->parent;

    // Setup page directory
    mem_switchDirectory(current_cpu->current_thread->dir);

    // On your mark...
    arch_prepare_switch(current_cpu->current_thread);

    // Get set..
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_RUNNING);
    
    // Go!
    // #ifdef __ARCH_I386__
    // dprintf(DEBUG, "Thread %p (%s), dump context: IP %p SP %p BP %p\n", current_cpu->current_thread, next_thread->parent->name, current_cpu->current_thread->context.eip, current_cpu->current_thread->context.esp, current_cpu->current_thread->context.ebp);
    // #else
    // dprintf(DEBUG, "Thread %p (%s), dump context: IP %p SP %p BP %p\n", current_cpu->current_thread, current_cpu->current_thread->parent->name, current_cpu->current_thread->context.rip, current_cpu->current_thread->context.rsp, current_cpu->current_thread->context.rbp);
    // #endif

    task_switches += 1;
    arch_load_context(&current_cpu->current_thread->context);
    __builtin_unreachable();
}

/**
 * @brief Yield to the next task in the queue
 * 
 * This will yield current execution to the next available task, but will return when
 * this process is loaded by @c process_switchNextThread
 * 
 * @param reschedule Whether to readd the process back to the queue, meaning it can return whenever and isn't waiting on something
 */
void process_yield(uint8_t reschedule) {
    // Do we even have a thread?
    if (current_cpu->current_thread == NULL) {
        // Just switch to next thread
        process_switchNextThread();
    }

    // Thread no longer has any time to execute. Save FPU registers
    // TODO: DESPERATELY move this to context structure.
#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
    asm volatile ("fxsave (%0)" :: "r"(current_cpu->current_thread->fp_regs));
#endif

    // Equivalent to a setjmp, use arch_save_context() to save our context
    if (arch_save_context(&current_cpu->current_thread->context) == 1) {
        // We are back, and were chosen to be executed. Return

    #if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
        asm volatile ("fxrstor (%0)" :: "r"(current_cpu->current_thread->fp_regs));
    #endif

        return;
    }
    
    // NOTE: Normally we would call process_switchNextThread but that will cause a critical error. See reschedule part of this function

    // Get current thread
    thread_t *prev = current_cpu->current_thread;

    // Get next thread in queue
    thread_t *next_thread = scheduler_get();
    if (!next_thread) {
        // next_thread = current_cpu->idle_process->main_thread;
        kernel_panic_extended(SCHEDULER_ERROR, "scheduler", "*** No thread was found in the scheduler (or something has been corrupted). Got thread %p.\n", next_thread);
    }

    // Update CPU variables
    current_cpu->current_thread = next_thread;
    current_cpu->current_process = next_thread->parent;

    // Setup page directory
    // TODO: Test this. Is it possible mem_getCurrentDirectory != mem_getKernelDirectory?
    if (next_thread->dir || mem_getCurrentDirectory() != mem_getKernelDirectory()) mem_switchDirectory(current_cpu->current_thread->dir);

    // On your mark... (load kstack)
    arch_prepare_switch(current_cpu->current_thread);

    // Get set..
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_RUNNING);
    
    // Go!
    // #ifdef __ARCH_I386__
    // dprintf(DEBUG, "Thread %p (%s), dump context: IP %p SP %p BP %p\n", current_cpu->current_thread, next_thread->parent->name, current_cpu->current_thread->context.eip, current_cpu->current_thread->context.esp, current_cpu->current_thread->context.ebp);
    // #else
    // dprintf(DEBUG, "Thread %p (%s), dump context: IP %p SP %p BP %p\n", current_cpu->current_thread, current_cpu->current_thread->parent->name, current_cpu->current_thread->context.rip, current_cpu->current_thread->context.rsp, current_cpu->current_thread->context.rbp);
    // #endif

    // Reschedule thread now. This should leave a VERY slim time window for another CPU to pick up the thread
    // NOTE: You're supposed to not reschedule after putting a thread to sleep but just in case they're trying to disallow it.
    if (prev && reschedule && !(prev->status & THREAD_STATUS_SLEEPING)) {
        // !!!: It is possible for a race condition to occur here. It is very unlikely, but possible.
        // !!!: If another CPU picks this thread up and somehow manages to switch to it faster than we can, it will corrupt the stack and cause hell.
        // !!!: This is why we can't call process_switchNextThread. Sorry, not sorry.
        scheduler_insertThread(prev);
    }

    task_switches += 1;
    arch_load_context(&current_cpu->current_thread->context);
    __builtin_unreachable();
}

/**
 * @brief Allocate a new PID from the PID bitmap
 */
pid_t process_allocatePID() {
    if (pid_bitmap == NULL) {
        // Create bitmap
        pid_bitmap = kmalloc(PROCESS_PID_BITMAP_SIZE);
        memset(pid_bitmap, 0, PROCESS_PID_BITMAP_SIZE);
    }

    for (uint32_t i = 0; i < PROCESS_PID_BITMAP_SIZE; i++) {
        for (uint32_t j = 0; j < sizeof(uint32_t) * 8; j++) {
            // Check each bit in the PID bitmap
            if (!(pid_bitmap[i] & (1 << j))) {
                // This is a free PID, allocate and return it
                pid_bitmap[i] |= (1 << j);
                return (i * (sizeof(uint32_t) * 8)) + j;
            }
        }
    }

    // Out of PIDs
    kernel_panic_extended(SCHEDULER_ERROR, "process", "*** Out of process PIDs.\n");
    return 0;
}

/**
 * @brief Free a PID after process destruction
 * @param pid The PID to free
 */
void process_freePID(pid_t pid) {
    if (!pid_bitmap) return; // ???

    // To get the index in the bitmap, round down PID
    int bitmap_idx = (pid / 32) * 32;
    pid_bitmap[bitmap_idx] &= ~(1 << (pid - bitmap_idx));
}

/**
 * @brief Get a process from a PID
 * @param pid The pid to check for
 * @returns The process if found, otherwise NULL
 */
process_t *process_getFromPID(pid_t pid) {
    // TODO: Gotta be a better way to do this..
    if (current_cpu->current_process->pid == pid) return current_cpu->current_process;

    foreach(proc_node, process_list) {
        process_t *proc = (process_t*)proc_node->value;
        if (proc) {
            if (proc->pid == pid) return proc;
        }
    }

    return NULL;
}

/**
 * @brief Internal method to create a new process
 * @param parent The parent of the process
 * @param name The name of the process (will be strdup'd)
 * @param flags The flags to use for the process
 * @param priority The priority of the process
 */
static process_t *process_createStructure(process_t *parent, char *name, unsigned int flags, unsigned int priority) {
    process_t *process = kmalloc(sizeof(process_t));
    memset(process, 0, sizeof(process_t));

    // Setup some variables
    process->parent = parent;
    process->name = strdup(name);
    process->flags = flags;
    process->priority = priority;
    process->gid = process->uid = 0;
    process->pid = process_allocatePID();
    if (!parent) process->vas = vas_create("process vas", MEM_USERSPACE_REGION_START + PROCESS_MMAP_MINIMUM, MEM_USERSPACE_REGION_END - PROCESS_MMAP_MINIMUM, VAS_USERMODE | VAS_COW | VAS_FAKE | VAS_NOT_GLOBAL);

    // Create working directory
    if (parent && parent->wd_path) {
        process->wd_path = strdup(parent->wd_path);
    } else {
        // No parent, just use "/" I guess
        process->wd_path = strdup("/");
    }

    // Create tree node
    if (parent && parent->node) {
        process->node = tree_insert_child(process_tree, parent->node, (void*)process);
    }

    // Create process' kernel stack
    process->kstack = mem_allocate(0, PROCESS_KSTACK_SIZE, MEM_ALLOC_HEAP, MEM_PAGE_KERNEL) + PROCESS_KSTACK_SIZE;
    // memset((void*)process->kstack - PROCESS_KSTACK_SIZE, 0, PROCESS_KSTACK_SIZE*2);
    dprintf(DEBUG, "Process '%s' has had its kstack %p allocated in page directory %p\n", name, process->kstack, current_cpu->current_dir);
    
    // Make directory
    if (process->flags & PROCESS_KERNEL) {
        // Reuse kernel directory
        process->dir = NULL;
    } else if (parent) {
        // Clone parent directory
        process->dir = mem_clone(parent->dir);
    } else {
        // Clone kernel
        process->dir = mem_clone(NULL);
    }

    // Do we need to copy mappings?
    if (parent) {
        // vas_allocation_t *alloc = parent->vas->head;
        // vas_allocation_t *alloc_past = NULL;

        // while (alloc) {
        //     // Create and copy this allocation
        //     vas_allocation_t *new_alloc = kmalloc(sizeof(vas_allocation_t));
        //     memcpy(new_alloc, alloc, sizeof(vas_allocation_t));
        //     if (!alloc->prev) process->vas->head = new_alloc;
        //     new_alloc->prev = alloc_past;
        //     if (alloc_past) alloc_past->next = new_alloc;
        //     alloc_past = new_alloc;
        //     alloc = alloc->next;
        // }

        // process->vas->allocations = parent->vas->allocations;
    extern vas_t *vas_clone(vas_t *parent);
        process->vas = vas_clone(parent->vas);
    }

    process->vas->dir = process->dir;

    // Create file descriptor table
    if (parent && 0) {
        // Reference parent table
        // TODO: Maybe use some sort of process flag that forces recreation of a new fd table?
        process->fd_table = parent->fd_table;
        process->fd_table->references++;
    } else {
        size_t fd_count = (parent) ? parent->fd_table->total : PROCESS_FD_BASE_AMOUNT;
        process->fd_table = kmalloc(sizeof(fd_table_t));
        memset(process->fd_table, 0, sizeof(fd_table_t));
        process->fd_table->total = fd_count;
        process->fd_table->amount = (parent) ? parent->fd_table->amount : 0;
        
        process->fd_table->references = 1;
        process->fd_table->fds = kmalloc(sizeof(fd_t*) * fd_count);
        if (parent) {
            for (size_t i = 0; i < parent->fd_table->total; i++) {
                if (parent->fd_table->fds[i]) {
                    process->fd_table->fds[i] = kmalloc(sizeof(fd_t));
                    process->fd_table->fds[i]->mode = parent->fd_table->fds[i]->mode;
                    process->fd_table->fds[i]->offset = parent->fd_table->fds[i]->offset;
                    process->fd_table->fds[i]->fd_number = parent->fd_table->fds[i]->fd_number;
                    
                    process->fd_table->fds[i]->node = fs_copy(parent->fd_table->fds[i]->node);
                } else {
                    process->fd_table->fds[i] = NULL;
                }
            }
        } else {
            memset(process->fd_table->fds, 0, sizeof(fd_t*) * fd_count);
        }
    }

    if (process_list) list_append(process_list, (void*)process);

#ifdef __ARCH_I386__
    // !!!: very dirty hack
    // !!!: resets pages in process->kstack to be global, meaning they won't be invalidated when the TLB flushes (mem_switchDirectory)
    // !!!: this is bad. kernel allocations should be global in all directories. they are in i386, but stacks cant be handled by its current system.
    for (uintptr_t i = (process->kstack - PROCESS_KSTACK_SIZE); i < process->kstack; i += PAGE_SIZE) {
        page_t *pg = mem_getPage(NULL, i, MEM_CREATE);
        if (pg) pg->bits.global = 1;
    }
#endif

    return process;
}

/**
 * @brief Create a kernel process with a single thread
 * @param name The name of the kernel process
 * @param flags The flags of the kernel process
 * @param priority Process priority
 * @param entrypoint The entrypoint of the kernel process
 * @param data User-specified data
 * @returns Process structure
 */
process_t *process_createKernel(char *name, unsigned int flags, unsigned int priority, kthread_t entrypoint, void *data){
    process_t *proc = process_create(NULL, name, flags, priority);
    proc->main_thread = thread_create(proc, proc->dir, (uintptr_t)&arch_enter_kthread, THREAD_FLAG_KERNEL);

    THREAD_PUSH_STACK(SP(proc->main_thread->context), void*, data);
    THREAD_PUSH_STACK(SP(proc->main_thread->context), void*, entrypoint);

    return proc;
}

/**
 * @brief Idle process function
 */
static void kernel_idle() {
    // Pause execution
    arch_pause();

    // For the kidle process, this can serve as total "cycles"
    current_cpu->current_thread->total_ticks++; 

    // Switch to next thread
    process_switchNextThread();
}

/**
 * @brief Create a new idle process
 * 
 * Creates and returns an idle process. All this process does is repeatedly
 * call @c arch_pause and try to switch to the next thread.
 * 
 * @note    This process should not be added to the process tree. Instead it is
 *          kept in the main process data structure, and will be switched to
 *          when there are no other processes to go to.
 */
process_t *process_spawnIdleTask() {
    // Create new process
    process_t *idle = process_createStructure(NULL, "idle", PROCESS_KERNEL | PROCESS_STARTED | PROCESS_RUNNING, PRIORITY_LOW);
    
    // !!!: Hack
    process_freePID(idle->pid);
    idle->pid = -1; // Not actually a process
    
    // !!!: remove
    // if (process_list) list_delete(process_list, list_find(process_list, (void*)idle));

    // Create a new thread
    idle->main_thread = thread_create(idle, NULL, (uintptr_t)&kernel_idle, THREAD_FLAG_KERNEL);

    return idle;
}

/**
 * @brief Totally destroy a process
 * @param proc The process to destroy
 * @warning ONLY USE THIS IF THE PROCESS IS NOT IN USE. OTHERWISE THIS WILL CAUSE CHAOS
 */
void process_destroy(process_t *proc) {
    if (!proc || !(proc->flags & PROCESS_STOPPED)) return;

    LOG(DEBUG, "Destroying process \"%s\"...\n", proc->name);

    process_freePID(proc->pid);
    list_delete(process_list, list_find(process_list, (void*)proc));

    if (proc->mmap) {
        process_mapping_t *prev = NULL; // to ensure that we can just keep iterating through the list

        foreach(mmap_node, proc->mmap) {
            if (prev) {
                LOG(DEBUG, "Dropping mapping %p: %p - %p\n", prev, prev->addr, prev->size);
                process_removeMapping(current_cpu->current_process, prev);
                prev = NULL;
            }

            if (mmap_node && mmap_node->value) {
                process_mapping_t *map = (process_mapping_t*)mmap_node->value;
                prev = map;
            }
        }

        // if (prev) process_removeMapping(proc, prev);
        list_destroy(proc->mmap, false);
    }

    // Drop main thread in process (?)
    thread_destroy(proc->main_thread);

    // Destroy everything we can
    if (proc->waitpid_queue) list_destroy(proc->waitpid_queue, false);
    fd_destroyTable(proc);
    if (proc->dir) mem_destroyVAS(proc->dir);
    mem_free(proc->kstack - PROCESS_KSTACK_SIZE, PROCESS_KSTACK_SIZE, MEM_DEFAULT);
    
    if (proc->thread_list) list_destroy(proc->thread_list, false);
    if (proc->node) {
        tree_remove(process_tree, proc->node);
        kfree(proc->node);
    }

    kfree(proc->wd_path);
    // kfree(proc->name);
    kfree(proc);
}

/**
 * @brief The grim reaper
 * 
 * This is the kernel thread which runs in the background when processes exit. It frees their resources
 * immediately and then blocks until new processes are available.
 */
void process_reaper(void *ctx) {
    for (;;) {
        sleep_untilNever(current_cpu->current_thread);
        if (sleep_enter() == WAKEUP_SIGNAL) {
            LOG(WARN, "You can't kill the grim reaper.\n");
            continue;
        }

        // Anything available?
        if (!reap_queue->length) continue;

        // Content is available, let's free it
        spinlock_acquire(&reap_queue_lock);

        for (size_t i = 0; i < reap_queue->length; i++) {
            node_t *procnode = list_popleft(reap_queue);
            if (!procnode) break;

            process_t *proc = (process_t*)procnode->value;

            if (proc && (proc->flags & PROCESS_STOPPED)) {
                // Stopped process, ready for the taking.
                
                // Although, we first need to make sure that no CPUs currently own this process
                if (PROCESS_IN_USE(proc)) {
                    // dang.
                    list_append_node(reap_queue, procnode); // We're only executing reap_queue->length times, so this won't matter
                    continue;                    
                }

                // Yoink!
                kfree(procnode);
                process_destroy(proc);
            }
        }

        spinlock_release(&reap_queue_lock);
    }
}

/**
 * @brief Spawn a new init process
 * 
 * Creates and returns an init process. This process has no context, and is basically
 * an empty shell. Rather, when @c process_execute is called, it replaces the current process' (init)
 * threads and sections with the process to execute for init.
 */
process_t *process_spawnInit() {
    // Create a new process
    process_t *init = process_createStructure(NULL, "init", PROCESS_STARTED | PROCESS_RUNNING, PRIORITY_HIGH);

    // !!!: hack
    process_freePID(init->pid);
    init->pid = 0;

    // Set as parent node (all processes stem from this one)
    tree_set_parent(process_tree, (void*)init);
    init->node = process_tree->root;

    // Done
    return init;
}

/**
 * @brief Create a new process
 * @param parent Parent process, or NULL if not needed
 * @param name The name of the process
 * @param flags The flags of the process
 * @param priority The priority of the process 
 */
process_t *process_create(process_t *parent, char *name, int flags, int priority) {
    return process_createStructure(parent, name, flags, priority);
}

/**
 * @brief Execute a new ELF binary for the current process (execve)
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 * 
 * @todo There's a lot of pointless directory switching for some reason - need to fix
 */
int process_execute(fs_node_t *file, int argc, char **argv, char **envp) {
    if (!file) return -EINVAL;
    if (!current_cpu->current_process) return -EINVAL; // TODO: Handle this better

    // Check the ELF binary
    if (elf_check(file, ELF_EXEC) == 0) {
        // Not a valid ELF binary
        LOG(ERR, "Invalid ELF binary detected when trying to start execution\n");
        return -EINVAL;
    }

    // Setup new name
    kfree(current_cpu->current_process->name);
    current_cpu->current_process->name = strdup(argv[0]); // ??? will this work?

    // Destroy previous threads
    if (current_cpu->current_process->main_thread) __sync_or_and_fetch(&current_cpu->current_process->main_thread->status, THREAD_STATUS_STOPPING);
    if (current_cpu->current_process->thread_list) {
        foreach(thread_node, current_cpu->current_process->thread_list) {
            thread_t *thr = (thread_t*)thread_node->value;
            if (thr && thr != current_cpu->current_thread) __sync_or_and_fetch(&thr->status, THREAD_STATUS_STOPPING);
        }
    }

    // Switch away from old directory
    mem_switchDirectory(NULL);

    // Destroy the current thread
    if (current_cpu->current_thread) {
        __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPING);
        thread_destroy(current_cpu->current_thread);
    }

    // Clone new directory and destroy the old one
    LOG(DEBUG, "Process \"%s\" (PID: %d) - destroy VAS %p\n", current_cpu->current_process->name, current_cpu->current_process->pid, current_cpu->current_process->dir);
    page_t *last_dir = current_cpu->current_process->dir;
    current_cpu->current_process->dir = mem_clone(NULL);
    vas_destroy(current_cpu->current_process->vas);
    mem_destroyVAS(last_dir);

    // Create a new VAS
    current_cpu->current_process->vas = vas_create("process vas", MEM_USERSPACE_REGION_START + PROCESS_MMAP_MINIMUM, MEM_USERSPACE_REGION_END - PROCESS_MMAP_MINIMUM, VAS_FAKE | VAS_NOT_GLOBAL | VAS_USERMODE | VAS_COW);

    // Switch to directory
    mem_switchDirectory(current_cpu->current_process->dir);

    // Create a new main thread with a blank entrypoint
    current_cpu->current_process->main_thread = thread_create(current_cpu->current_process, current_cpu->current_process->dir, 0x0, THREAD_FLAG_DEFAULT);

    // Load file into memory
    // TODO: This runs check twice (redundant)
    uintptr_t elf_binary = elf_load(file, ELF_USER);

    // Success?
    if (elf_binary == 0x0) {
        // Something happened...
        LOG(ERR, "ELF binary failed to load properly (but is valid?)\n");
        return -EINVAL;
    }

    // Setup heap location
    current_cpu->current_process->heap_base = elf_getHeapLocation(elf_binary);
    current_cpu->current_process->heap = current_cpu->current_process->heap_base;

    // Get the entrypoint
    uintptr_t process_entrypoint = elf_getEntrypoint(elf_binary);
    arch_initialize_context(current_cpu->current_process->main_thread, process_entrypoint, current_cpu->current_process->main_thread->stack);

    // We own this process
    current_cpu->current_thread = current_cpu->current_process->main_thread;

    // Now we need to start pushing argc, argv, and envp onto the thread stack

    // Calculate envc
    // TODO: Maybe accept envc/force accept envc so this dangerous/slow code can be calculated elsewhere
    int envc = 0;
    char **p = envp;
    while (*p++) envc++;

    // Push contents of envc onto the stack
    char *envp_pointers[envc]; // The array we pass to libc is a list of pointers, so we push the strings and then the pointers
    for (int e = 0; e < envc; e++) {
        THREAD_PUSH_STACK_STRING(current_cpu->current_thread->stack, strlen(envp[e]), envp[e]);
        envp_pointers[e] = (char*)current_cpu->current_thread->stack;
    }

    // Push contents of argv onto the stack
    char *argv_pointers[argc];
    for (int a = 0; a < argc; a++) {
        THREAD_PUSH_STACK_STRING(current_cpu->current_thread->stack, strlen(argv[a]), argv[a]);
        argv_pointers[a] = (char*)current_cpu->current_thread->stack;
    }

    // Now let's push the envp array
    // We have to do this backwards to make sure the array is constructured properly
    // Push NULL first
    char **user_envp = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int e = envc; e > 0; e--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, envp_pointers[e-1]);
    }

    user_envp = (char**)current_cpu->current_thread->stack;

    // Push the argv array
    // Push NULL first
    char **user_argv = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int a = argc; a > 0; a--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, argv_pointers[a-1]);
    }

    user_argv = (char**)current_cpu->current_thread->stack;

    // Now we can the pointers they need
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, user_envp);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, user_argv);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, argc);

    // Enter
    LOG(DEBUG, "Launching new ELF process\n");
    arch_prepare_switch(current_cpu->current_thread);
    arch_start_execution(process_entrypoint, current_cpu->current_thread->stack);

    return 0;
} 

/**
 * @brief Exiting from a process
 * @param process The process to exit from, or NULL for current process
 * @param status_code The status code
 */
void process_exit(process_t *process, int status_code) {
    if (!process) process = current_cpu->current_process;
    if (!process) kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "process", "*** Cannot exit from non-existant process\n");
    
    int is_current_process = (process == current_cpu->current_process);

    // The scheduler itself ignores the process state, so mark it as stopped
    // __sync_or_and_fetch(&process->flags, PROCESS_STOPPED);
    process->flags |= PROCESS_STOPPED;

    // Now we need to mark all threads of this process as stopping. This will ensure that memory is fully separate
    if (process->main_thread) __sync_or_and_fetch(&process->main_thread->status, THREAD_STATUS_STOPPING);
    
    if (process->thread_list && process->thread_list->length) {
        foreach(thread_node, process->thread_list) {
            if (thread_node->value) {
                thread_t *thr = (thread_t*)thread_node->value;
                __sync_or_and_fetch(&thr->status, THREAD_STATUS_STOPPING);
            }
        }
    }

    // TODO: Ugly
    current_cpu->current_process->exit_status = status_code;


    // Deparent the children
    if (process->node && process->node->children) {
        foreach (cnode, process->node->children) {
            tree_node_t *tnode = (tree_node_t*)cnode->value;
            process_t *child = (process_t*)tnode->value;
            child->parent = NULL;
        }
    }

    // Instead of freeing all the memory now, we add ourselves to the reap queue
    // The reap queue is either destroyed by:
    //      1. The reaper kernel thread
    //      2. The parent process waiting for this process to exit (POSIX - should only happen during waitpid)
    
    // If our parent is waiting, wake them up
    if (process->parent && process->parent->waitpid_queue && process->parent->waitpid_queue->length) {
        // TODO: Locking?
        foreach(thr_node, process->parent->waitpid_queue) {
            thread_t *thr = (thread_t*)thr_node->value;
            sleep_wakeup(thr);
        }

        // !!!: KNOWN BUG: If a process that is forked off by a shell is not waited on, then it will not exit properly.
        process_switchNextThread(); // !!!: Hopefully that works and they free us..
    } 

    // Put ourselves in the wait queue
    spinlock_acquire(&reap_queue_lock);
    list_append(reap_queue, (void*)process);  
    spinlock_release(&reap_queue_lock);

    // Wakeup the reaper thread
    sleep_wakeup(reaper_proc->main_thread);

    // To the next process we go
    process_switchNextThread();
}

/**
 * @brief Fork the current process
 * @returns Depends on what you are. Only call this from system call context.
 */
pid_t process_fork() {
    // First we create our child process
    process_t *parent = current_cpu->current_process;   
    process_t *child = process_create(parent, parent->name, parent->flags, parent->priority);

    // Create a new child process thread
    child->main_thread = thread_create(child, child->dir, (uintptr_t)NULL, THREAD_FLAG_CHILD);

    // HACK:    This is one of the grossest hacks in my opinion. This trick is yet another one stolen from ToaruOS.
    //          I would love to call arch_save_context() for the child thread and just return but unfortunately since we're still in
    //          execution context for the current thread it would overwrite the stack and cause problems.
    //          There's definitely a good way to fix this but I'm really tired right now so I'm gonna use this
    
    // Configure context of child thread
    IP(child->main_thread->context) = (uintptr_t)&arch_restore_context;
    SP(child->main_thread->context) = child->kstack;
    BP(child->main_thread->context) = SP(child->main_thread->context);
    
    // Push the registers onto the stack
    struct _registers r;
    memcpy(&r, current_cpu->current_process->regs, sizeof(struct _registers));
    
    // Configure the system call return value
#ifdef __ARCH_I386__
    r.eax = 0;
#elif defined(__ARCH_X86_64__)
    r.rax = 0;
#elif defined(__ARCH_AARCH64__)
    r.x0 = 0;
#else
    #error "Please handle this hacky garbage."
#endif

    THREAD_PUSH_STACK(SP(child->main_thread->context), struct _registers, r);

    // Insert new thread
    scheduler_insertThread(child->main_thread);

    return child->pid;
}

/**
 * @brief waitpid equivalent
 */
long process_waitpid(pid_t pid, int *wstatus, int options) {
    // Let's go. wstatus pointer was validated by syscall, so that's not a need to check.
    for (;;) {
        if (!current_cpu->current_process->node) {
            // lol
            return -ECHILD;
        }

        // Put ourselves in our wait queue
        if (!current_cpu->current_process->waitpid_queue) current_cpu->current_process->waitpid_queue = list_create("waitpid queue");
        list_append(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread);

        // We need this to stop interferance from other threads also trying to waitpid
        spinlock_acquire(&reap_queue_lock);

        // Let's look through the list of children to see if we find anything
        if (!current_cpu->current_process->node->children || !current_cpu->current_process->node->children->length) {
            // There are no children available
            spinlock_release(&reap_queue_lock);
            list_delete(current_cpu->current_process->waitpid_queue, list_find(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread));
            return -ECHILD;
        }
   
        foreach(child_node, current_cpu->current_process->node->children) {
            process_t *child = (process_t*)((tree_node_t*)(child_node->value))->value;
            if (!child) continue;

            // Validate PID matches
            if (pid < -1) {
                // Wait for any child process whose group ID is the absolute value of PID
                if (child->gid != (pid * -1)) continue;
            } else if (pid == 0) {
                // Wait for any child process whose group ID is equal to the calling process
                if (current_cpu->current_process->gid != child->gid) {
                    continue;
                }
            } else if (pid > 0) {
                // Wait for any child process whose PID is the same as PID
                if (child->pid != pid) continue;
            }

            // Look for processes that have exited.
            // TODO: waitid? we can modify this syscall for that
            if (child->flags & PROCESS_STOPPED) {
                // Dead process, nice. This will work.
                // Make sure child process isn't in use
                pid_t ret_pid = child->pid;

                // Update wstatus
                if (wstatus) *wstatus = WSTATUS_EXITED | (child->exit_status << WSTATUS_EXITCODE);

                if (!PROCESS_IN_USE(child)) {
                    process_destroy(child);
                }

                spinlock_release(&reap_queue_lock);

                // Take us out and return
                list_delete(current_cpu->current_process->waitpid_queue, list_find(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread));
                return child->pid;
            }

            // TODO: Look for continued, interrupted, etc
        }

        spinlock_release(&reap_queue_lock);

        // There were children available but they didn't seem important
        if (options & WNOHANG) {
            // Return immediately, we didn't get anything.
            list_delete(current_cpu->current_process->waitpid_queue, list_find(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread));
            return 0;
        } else {
            // Sleep until we get woken up
            sleep_untilNever(current_cpu->current_thread);
            if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;
        }
    }
}