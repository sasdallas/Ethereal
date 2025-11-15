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
#include <kernel/mem/pmm.h>
#include <kernel/loader/elf.h>
#include <kernel/misc/util.h>

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
volatile uint64_t task_switches = 0;

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

    // Initialize futexes
    futex_init();

    // Initialize reap queue and reaper process
    reap_queue = list_create("process reap queue");
    // reaper_proc = process_createKernel("reaper", 0, PRIORITY_MED, process_reaper, NULL);
    // scheduler_insertThread(reaper_proc->main_thread);

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
    vmm_switch(current_cpu->current_process->ctx);

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
    arch_load_context(&current_cpu->current_thread->context); // Didn't bother switching context
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
    vmm_switch(current_cpu->current_thread->ctx);

    // On your mark... (load kstack)
    arch_prepare_switch(current_cpu->current_thread);

    // Get set..
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_RUNNING);
    
    // Acquire the lock - arch_yield will release the lock
    spinlock_acquire(current_cpu->sched.lock);
    if (prev && reschedule && !(prev->status & THREAD_STATUS_SLEEPING)) {
        list_append_node(current_cpu->sched.queue, &prev->sched_node);
    }

    task_switches += 1;
    arch_yield(prev, current_cpu->current_thread);
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

    if (parent) {
        process->uid = parent->uid;
        process->gid = parent->gid;
        process->euid = parent->euid;
        process->egid = parent->egid;
        process->pgid = parent->pgid;
        process->sid = parent->sid;
    } else {
        process->gid = process->uid = 0;
    }

    process->pid = process_allocatePID();


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

    // Make directory
    process->vas = NULL;
    if (process->flags & PROCESS_KERNEL) {
        // Reuse kernel directory
        process->ctx = vmm_kernel_context;
    } else if (parent) {
        // Clone parent directory
        process->ctx = vmm_clone(parent->ctx);
    } else {
        // Clone kernel
        process->ctx = vmm_createContext();
    }

    LOG(DEBUG, "process->ctx = %p current->ctx = %p\n", process->ctx, current_cpu->current_context);

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

        memset(process->fd_table->fds, 0, sizeof(fd_t*) * fd_count);

        if (parent) {
            for (size_t i = 0; i < parent->fd_table->total; i++) {
                if (parent->fd_table->fds[i]) {
                    process->fd_table->fds[i] = kmalloc(sizeof(fd_t));
                    LOG(DEBUG, "copy fd %d: %p -> %p\n", i, process->fd_table->fds[i], parent->fd_table->fds[i]);
                    process->fd_table->fds[i]->mode = parent->fd_table->fds[i]->mode;
                    process->fd_table->fds[i]->offset = parent->fd_table->fds[i]->offset;
                    process->fd_table->fds[i]->fd_number = parent->fd_table->fds[i]->fd_number;
                    
                    process->fd_table->fds[i]->node = fs_copy(parent->fd_table->fds[i]->node);
                }
            }
        }
    }

    if (process_list) list_append(process_list, (void*)process);

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
    process_t *proc = process_create(NULL, name, flags | PROCESS_KERNEL, priority);
    proc->main_thread = thread_create(proc, proc->ctx, (uintptr_t)&arch_enter_kthread, THREAD_FLAG_KERNEL);

    THREAD_PUSH_STACK(SP(proc->main_thread->context), void*, data);
    THREAD_PUSH_STACK(SP(proc->main_thread->context), void*, entrypoint);

    return proc;
}

/**
 * @brief Idle process function
 */
static void kernel_idle() {
    arch_pause();

    // For the kidle process, this can serve as total "cycles"
    current_cpu->current_thread->total_ticks++; 
    current_cpu->idle_time++;

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

    LOG(DEBUG, "Destroying process \"%s\" (%p, by request of %p)...\n", proc->name, proc, __builtin_return_address(0));

    process_freePID(proc->pid);
    list_delete(process_list, list_find(process_list, (void*)proc));

    // Destroy mmap mappings
    process_destroyMappings(proc);

    // Destroy the remainder of the context
    if (!(proc->flags & PROCESS_KERNEL)) vmm_destroyContext(proc->ctx);

    if (proc->ptrace.tracees) {
        foreach(tracee, proc->ptrace.tracees) {
            // TODO: PTRACE_O_EXITKILL
            ptrace_untrace((process_t*)tracee->value, proc);
        }

        list_destroy(proc->ptrace.tracees, 0);
    }

    // Destroy everything we can
    if (proc->waitpid_queue) list_destroy(proc->waitpid_queue, false);
    fd_destroyTable(proc);
    
    
    if (proc->thread_list) list_destroy(proc->thread_list, false);
    if (proc->node) {
        tree_remove(process_tree, proc->node);
    }

    kfree(proc->wd_path);
    kfree(proc->name);
    kfree(proc);
    LOG(DEBUG, "On finish: PMM block usage is %d\n", pmm_getUsedBlocks());
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
    init->sid = 1;
    init->pgid = 1;

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
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 * 
 * @todo There's a lot of pointless directory switching for some reason - need to fix
 */
int process_executeDynamic(char *path, fs_node_t *file, int argc, char **argv, char **envp) {
    // Execute dynamic loader
    // First, try to open the interpreter that's in PT_INTERP
    char *interpreter_path = elf_getInterpreter(file);
    fs_node_t *interpreter = NULL;
    if (interpreter_path) {
        // We have an interpreter path
        LOG(INFO, "Trying to execute interpreter: %s\n", interpreter_path);
        interpreter = kopen(interpreter_path, 0);
        kfree(interpreter_path);
    }

    // Strike 2
    if (!interpreter) {
        // Backup path: Run /usr/lib/ld.so
        LOG(INFO, "Trying to load interpreter: /usr/lib/ld.so\n");
        interpreter = kopen("/usr/lib/ld.so", 0);
    }

    // Strike 3
    if (!interpreter) {
        LOG(ERR, "No interpreter available\n");
        return -ENOENT;
    }

    // Setup new name
    // TODO: This should be a *pointer* to argv[0], not a duplicate.
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
    vmm_switch(vmm_kernel_context);

    // Destroy the current thread
    if (current_cpu->current_thread) {
        __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPING);
        // thread_destroy(current_cpu->current_thread);
    }

    // VMM context
    vmm_context_t *oldctx = current_cpu->current_process->ctx;
    current_cpu->current_process->ctx = vmm_createContext();
    vmm_switch(current_cpu->current_process->ctx);
    if (oldctx && oldctx != vmm_kernel_context) vmm_destroyContext(oldctx);

    // Create a new VAS
    current_cpu->current_process->vas = NULL; 

    // Create a new main thread with a blank entrypoint
    current_cpu->current_process->main_thread = thread_create(current_cpu->current_process, current_cpu->current_process->ctx, 0x0, THREAD_FLAG_DEFAULT);

    // Load file into memory
    // TODO: This runs check twice (redundant)
    uintptr_t elf_binary = elf_load(interpreter, ELF_USER);

    // Success?
    if (elf_binary == 0x0) {
        // Something happened...
        LOG(ERR, "ELF binary failed to load properly (but is valid?)\n");
        return -EINVAL;
    }

    // Alright cool we have an interpreter now, load the file
    elf_dynamic_info_t info;
    if (elf_loadDynamicELF(file, &info)) {
        LOG(ERR, "Error loading dynamic ELF file\n");
        return -ENOEXEC;
    }

    // Setup heap location
    current_cpu->current_process->heap_base = elf_getHeapLocation(elf_binary);
    current_cpu->current_process->heap = current_cpu->current_process->heap_base;

    // Populate image
    elf_createImage(elf_binary);

    // Get the entrypoint
    uintptr_t process_entrypoint = elf_getEntrypoint(elf_binary);
    arch_initialize_context(current_cpu->current_process->main_thread, process_entrypoint, current_cpu->current_process->main_thread->stack);

    // We own this process
    current_cpu->current_thread = current_cpu->current_process->main_thread;

    // Done with ELF
    kfree((void*)elf_binary);

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

    // Now we can align the stack
    current_cpu->current_thread->stack = ALIGN_DOWN(current_cpu->current_thread->stack, 16);

    // Realign the stack if we need to. Everything from now on should JUST be a uintptr_t
    // The pending amount of bytes: 9 auxiliary vector variables, argc arguments, envc environment variables, plus argc itself and the two NULLs
    uintptr_t bytes = (9 * sizeof(uintptr_t)) + (argc * sizeof(uintptr_t)) + (envc * sizeof(uintptr_t)) + (3 * sizeof(uintptr_t));
    if (!IS_ALIGNED(current_cpu->current_thread->stack - bytes, 16)) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, 0x0);
    }
    
    // REF: https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf

    // Create SysV auxiliary vector
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_NULL);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, info.at_phdr);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_PHDR);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, info.at_phnum);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_PHNUM);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, info.at_phent);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_PHENT);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, info.at_entry);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_ENTRY);
    

    // Now let's push the envp array
    // We have to do this backwards to make sure the array is constructured properly
    // Push NULL first
    char **user_envp = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int e = envc; e > 0; e--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, envp_pointers[e-1]);
    }

    // Push the argv array
    // Push NULL first
    char **user_argv = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int a = argc; a > 0; a--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, argv_pointers[a-1]);
    }

    
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, argc);

    // Enter
    LOG(DEBUG, "Launching new ELF process (stack = %p)\n", current_cpu->current_thread->stack);
    arch_prepare_switch(current_cpu->current_thread);
    arch_start_execution(process_entrypoint, current_cpu->current_thread->stack);
    return 0;
}

/**
 * @brief Execute a new ELF binary for the current process (execve)
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 * 
 * @todo There's a lot of pointless directory switching for some reason - need to fix
 */
int process_execute(char *path, fs_node_t *file, int argc, char **argv, char **envp) {
    if (!file) return -EINVAL;
    if (!current_cpu->current_process) return -EINVAL; // TODO: Handle this better

    // First, check if the file is a dynamic object
    if (elf_check(file, ELF_DYNAMIC)) {
        // Yes, it is, run the process with ld.so
        // TODO: Get PT_INTERP value
        LOG(INFO, "Running dynamic executable\n");
        return process_executeDynamic(path, file, argc, argv, envp);
    }

    // Check the ELF binary
    if (elf_check(file, ELF_EXEC) == 0) {
        // Not a valid ELF binary
        LOG(ERR, "Invalid ELF binary detected when trying to start execution\n");
        return -EINVAL;
    }



    // Setup new name
    // TODO: This should be a *pointer* to argv[0], not a duplicate.
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
    vmm_switch(vmm_kernel_context);

    // Destroy the current thread
    if (current_cpu->current_thread) {
        __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPING);
        // thread_destroy(current_cpu->current_thread);
    }

    // VMM context
    vmm_context_t *oldctx = current_cpu->current_process->ctx;
    current_cpu->current_process->ctx = vmm_createContext();
    vmm_switch(current_cpu->current_process->ctx);
    vmm_destroyContext(oldctx);

    // Create a new VAS
    current_cpu->current_process->vas = NULL; 

    // Create a new main thread with a blank entrypoint
    current_cpu->current_process->main_thread = thread_create(current_cpu->current_process, current_cpu->current_process->ctx, 0x0, THREAD_FLAG_DEFAULT);

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

    // Populate image
    elf_createImage(elf_binary);

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

    // Now we can align the stack
    current_cpu->current_thread->stack = ALIGN_DOWN(current_cpu->current_thread->stack, 16);

    // Realign the stack if we need to. Everything from now on should JUST be a uintptr_t
    // The pending amount of bytes: 9 auxiliary vector variables, argc arguments, envc environment variables, plus argc itself and the two NULLs
    uintptr_t bytes = (9 * sizeof(uintptr_t)) + (argc * sizeof(uintptr_t)) + (envc * sizeof(uintptr_t)) + (3 * sizeof(uintptr_t));
    if (!IS_ALIGNED(current_cpu->current_thread->stack - bytes, 16)) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, 0x0);
    }
    

    // REF: https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf

    // Create SysV auxiliary vector
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, AT_NULL);
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, 0x0);
    

    // Now let's push the envp array
    // We have to do this backwards to make sure the array is constructured properly
    // Push NULL first
    char **user_envp = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int e = envc; e > 0; e--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, envp_pointers[e-1]);
    }

    // Push the argv array
    // Push NULL first
    char **user_argv = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int a = argc; a > 0; a--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, argv_pointers[a-1]);
    }

    
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
          
                // If the thread is already stopped, then that means it was waited on before and we can destroy it now
                if (thr->status & THREAD_STATUS_STOPPED) {
                    thread_destroy(thr);
                    continue;
                }

                // Else, mark the thread as stopping and let the scheduler catch it.
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
            child->parent = (process_t*)process_tree->root->value;
        }
    }

    // Instead of freeing all the memory now, we add ourselves to the reap queue
    // The reap queue is either destroyed by:
    //      1. The reaper kernel thread
    //      2. The parent process waiting for this process to exit (POSIX - should only happen during waitpid)
    
    // If our parent is waiting, wake them up
    if (process->parent) {
        if ((process->parent->flags & PROCESS_RUNNING)) signal_send(process->parent, SIGCHLD);
        if (process->parent && process->parent->waitpid_queue && process->parent->waitpid_queue->length) {
            // TODO: Locking?
            foreach(thr_node, process->parent->waitpid_queue) {
                thread_t *thr = (thread_t*)thr_node->value;
                sleep_wakeup(thr);
            }

            // !!!: KNOWN BUG: If a process that is forked off by a shell is not waited on, then it will not exit properly.
            if (process == current_cpu->current_process) process_switchNextThread(); // !!!: Hopefully that works and they free us..
        } 

    } 

    // // Put ourselves in the wait queue
    // spinlock_acquire(&reap_queue_lock);
    // LOG(WARN, "Assuming this process is orphaned\n");
    // list_append(reap_queue, (void*)process);

    // // Wakeup the reaper thread
    // sleep_wakeup(reaper_proc->main_thread);
    // spinlock_release(&reap_queue_lock);

    // To the next process we go
    if (process == current_cpu->current_process) {
        __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_STOPPING);
        scheduler_insertThread(current_cpu->current_thread);
        process_switchNextThread();
    }
}

/**
 * @brief Fork the current process
 * @returns Depends on what you are. Only call this from system call context.
 */
pid_t process_fork() {
    LOG(DEBUG, "On fork: PMM block usage is %d\n", pmm_getUsedBlocks());

    // First we create our child pprocess
    process_t *parent = current_cpu->current_process;   
    process_t *child = process_create(parent, parent->name, parent->flags, parent->priority);

    // Create a new child process thread
    child->main_thread = thread_create(child, child->ctx, (uintptr_t)NULL, THREAD_FLAG_CHILD);
    
    // Configure context of child thread
    IP(child->main_thread->context) = (uintptr_t)&arch_restore_context;
    SP(child->main_thread->context) = child->main_thread->kstack;
    BP(child->main_thread->context) = SP(child->main_thread->context);
    TLSBASE(child->main_thread->context) = TLSBASE(current_cpu->current_thread->context);
    
    // Push the registers onto the stack
    struct _registers r;
    memcpy(&r, current_cpu->current_thread->regs, sizeof(struct _registers));
    
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
    if (!current_cpu->current_process->node) {
        // lol
        return -ECHILD;
    }

    // Put ourselves in our wait queue
    if (!current_cpu->current_process->waitpid_queue) current_cpu->current_process->waitpid_queue = list_create("waitpid queue");

    for (;;) {
        node_t *n = list_find(current_cpu->current_process->waitpid_queue, current_cpu->current_thread);
        if (n) list_delete(current_cpu->current_process->waitpid_queue, n);
            
        // We need this to stop interferance from other threads also trying to waitpid
        spinlock_acquire(&reap_queue_lock);

        // Let's look through the list of children to see if we find anything
        if (!current_cpu->current_process->node->children || !current_cpu->current_process->node->children->length) {
            // There are no children available
            spinlock_release(&reap_queue_lock);
            return -ECHILD;
        }
   
        foreach(child_node, current_cpu->current_process->node->children) {
            process_t *child = (process_t*)((tree_node_t*)(child_node->value))->value;
            if (!child) continue;

            // Validate PID matches
            if (pid < -1) {
                // Wait for any child process whose group ID is the absolute value of PID
                if (child->gid != (gid_t)(pid * -1)) continue;
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
                if (wstatus) {
                    if (child->exit_reason == PROCESS_EXIT_NORMAL) {
                        *wstatus = (child->exit_status << 8);
                    } else {
                        *wstatus = (child->exit_status & 0x7F);
                    }
                }

                if (!PROCESS_IN_USE(child)) {
                    process_destroy(child);
                }

                // Take us out and return
                // list_delete(current_cpu->current_process->waitpid_queue, list_find(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread));
                spinlock_release(&reap_queue_lock);

                return ret_pid;
            }

            // Is the process stopped?
            if (child->flags & PROCESS_SUSPENDED) {
                // TODO: Check for WUNTRACED properly? i.e. make sure the signal was generated by a source outside of ptrace
                if (options & WSTOPPED || (child->ptrace.tracer == NULL && options & WUNTRACED)) {
                    pid_t ret_pid = child->pid;

                    // Update wstatus
                    if (wstatus) {
                        *wstatus = (child->exit_status << 8) | 0x7F;
                    }

                    spinlock_release(&reap_queue_lock);
                    return ret_pid;
                }
            }

            // TODO: Look for continued, interrupted, etc
        }

        // There were children available but they didn't seem important
        if (options & WNOHANG) {
            // Return immediately, we didn't get anything.
            // list_delete(current_cpu->current_process->waitpid_queue, list_find(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread));
            
            spinlock_release(&reap_queue_lock);
            return 0;
        } else {
            // Sleep until we get woken up
            spinlock_release(&reap_queue_lock);
            sleep_prepare();    
            list_append(current_cpu->current_process->waitpid_queue, (void*)current_cpu->current_thread);
            if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;
        }
    }
}

/**
 * @brief Add a new thread to the currnet process (sort of similar to clone())
 * @param stack The stack of the thread to add
 * @param tls (Optional) Thread local stack
 * @param entry The thread entrypoint
 * @param arg An argument to the thread
 * @returns ID of the new thread
 * 
 * If @c tls is 0x0, then the TLS of the current thread will be used.
 */
pid_t process_createThread(uintptr_t stack, uintptr_t tls, void *entry, void *arg) {
    if (!current_cpu->current_process->thread_list) current_cpu->current_process->thread_list = list_create("process thread list");
    thread_t *thr = thread_create(current_cpu->current_process, current_cpu->current_process->ctx, (uintptr_t)entry, THREAD_FLAG_CHILD);
    thr->stack = stack; // Fix stack
    list_append(current_cpu->current_process->thread_list, thr);

    // Boom, we have a thread. That was easy, right?
    // Well, unfortunately we have to steal the hack from fork() and use arch_restore_context

    // Configure context of child thread
    IP(thr->context) = (uintptr_t)&arch_restore_context;
    SP(thr->context) = thr->kstack;
    BP(thr->context) = thr->kstack;
    TLSBASE(thr->context) = (tls == 0x0) ? TLSBASE(current_cpu->current_thread->context) : tls;

    // Make some registers
    struct _registers r;
    memcpy(&r, current_cpu->current_thread->regs, sizeof(struct _registers));

#ifdef __ARCH_I386__
    THREAD_PUSH_STACK(thr->stack, void*, arg);
#elif defined(__ARCH_X86_64__)
    r.rdi = (uint64_t)arg;
#elif defined(__ARCH_AARCH64__)
    r.x0 = (uint64_t)arg;
#else
    #error "Please handle this"
#endif

    // Update registers
    REGS_IP((&r)) = (uintptr_t)entry;
    REGS_SP((&r)) = thr->stack;
    REGS_BP((&r)) = thr->stack;

    THREAD_PUSH_STACK(SP(thr->context), struct _registers, r);

    scheduler_insertThread(thr);

    return thr->tid;
}