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
#include <kernel/mm/vmm.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <sys/wait.h>
#include <sys/mman.h>
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

    // Exit the thread if its trying to stop
    if (current_cpu->current_thread->status & THREAD_STATUS_STOPPING) {
        return thread_exit();
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

    process->thread_list = NULL;
    process->nthreads = 0;
    SPINLOCK_INIT(&process->thread_lock);

    EVENT_INIT(&process->wait_event);

    process->pid = process_allocatePID();

    // Create working directory
    if (parent && parent->wd_path) {
        process->wd_path = strdup(parent->wd_path);
        process->wd_node = parent->wd_node;
        inode_hold(process->wd_node);
    } else {
        // No parent, just use "/" I guess
        process->wd_path = strdup("/");
    
        // !!! violation of VFS things
    extern vfs_inode_t *vfs_root_inode;
        process->wd_node = vfs_root_inode;
        if (vfs_root_inode) inode_hold(process->wd_node);
    }

    // Create tree node
    if (parent && parent->node) {
        process->node = tree_insert_child(process_tree, parent->node, (void*)process);
    }

    // Make directory
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

    // Create the file descriptor table
    fd_copyTable(parent, process);

    process->proc_list_node.value = (void*)process;
    if (process_list) list_append_node(process_list, &process->proc_list_node);
extern systemfs_node_t *systemfs_proc_create(process_t *proc);
    process->proc_sysfs = systemfs_proc_create(process);
    return process;
}

/**
 * @brief Create and add a thread to a process
 * @param proc The process to add the thread to
 * @param entry The entrypoint of the thread
 * @param flags The flags of the thread
 * @returns The new thread object
 */
thread_t *process_createThread(process_t *proc, uintptr_t entry, unsigned int flags) {
    thread_t *t = thread_create(proc, proc->ctx, entry, flags);
    spinlock_acquire(&proc->thread_lock);
    t->next = proc->thread_list;
    proc->thread_list = t;
    proc->nthreads++;
    spinlock_release(&proc->thread_lock);
    return t;
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
    proc->main_thread = process_createThread(proc, (uintptr_t)&arch_enter_kthread, THREAD_FLAG_KERNEL);

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
    idle->main_thread = process_createThread(idle, (uintptr_t)&kernel_idle, THREAD_FLAG_KERNEL);

    return idle;
}

/**
 * @brief Totally destroy a process
 * @param proc The process to destroy
 */
void process_destroy(process_t *proc) {
    if (!proc || !(proc->flags & PROCESS_STOPPED)) return;


    // Wait for all threads in process to die
    // They should have already been marked as STOPPING by process_exit or the like
    spinlock_acquire(&proc->thread_lock);
    thread_t *l = proc->thread_list;
    proc->thread_list = NULL;
    spinlock_release(&proc->thread_lock);

    while (l) {
        while ((l->status & THREAD_STATUS_STOPPED) == 0) {
            LOG(DEBUG, "Waiting for thread %p/%d to die before destroying process\n", l, l->tid);
            process_yield(1);
        }

        thread_t *next = l->next;
        thread_destroy(l);
        l = next;
    }

    assert(proc->nthreads == 0);
    LOG(DEBUG, "Destroying process \"%s\" (%p)...\n", proc->name, proc);

    process_freePID(proc->pid);
    list_delete(process_list, list_find(process_list, (void*)proc));
    if (proc->exe_image) vfs_close(proc->exe_image);
    inode_release(proc->wd_node);

    // Destroy table of fds
    fd_destroyTable(proc);

    // Destroy the remainder of the context
    if (!(proc->flags & PROCESS_KERNEL)) vmm_destroyContext(proc->ctx);

    if (proc->ptrace.tracees) {        
        foreach(tracee, proc->ptrace.tracees)  {
            // TODO: PTRACE_O_EXITKILL
            ptrace_untrace((process_t*)tracee->value, proc);
        }

        list_destroy(proc->ptrace.tracees, 0);
    }

    // !!! HOTFIX TO FIX STRACE
    if (proc->ptrace.tracer) {
        process_ptrace_t *trace = &proc->ptrace.tracer->ptrace;
        spinlock_acquire(&trace->lock);
        // TODO send PTRACE_EVENT_EXIT
        list_delete(trace->tracees, list_find(trace->tracees, proc));
        spinlock_release(&trace->lock);
        proc->ptrace.tracer = NULL;
    }

    // Destroy everything we can
    if (proc->node) {
        tree_remove(process_tree, proc->node);
    }

extern void systemfs_proc_destroy(process_t *proc);
    systemfs_proc_destroy(proc);

    kfree(proc->wd_path);
    kfree(proc->name);
    kfree(proc);
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
 * @brief Common process execute
 */
int process_executeCommon(vfs_file_t *file, uintptr_t *entry) {
    // Destroy previous threads
    process_t *proc = current_cpu->current_process;

    // Stop all previous threads
    spinlock_acquire(&proc->thread_lock);
    thread_t *t = proc->thread_list;
    while (t) {
        if (t != current_cpu->current_thread) {
            __sync_or_and_fetch(&t->status, THREAD_STATUS_STOPPING);
            sleep_wakeup(t);

            while ((t->status & THREAD_STATUS_STOPPED) == 0) {
                LOG(DEBUG, "process_executeCommon waiting for thread %p to die\n", t);
                process_yield(1);
            }

            thread_t *nxt = t->next;
            thread_destroy(t);
            t = nxt;
            continue;
        }
        t = t->next;
    }

    proc->thread_list = NULL;
    proc->nthreads = 0;
    spinlock_release(&proc->thread_lock);

    // Switch away from old directory
    vmm_switch(vmm_kernel_context);

    // VMM context
    vmm_context_t *oldctx = proc->ctx;
    proc->ctx = vmm_createContext();
    vmm_switch(proc->ctx);
    if (oldctx && oldctx != vmm_kernel_context) vmm_destroyContext(oldctx);

    // Create a new main thread with a blank entrypoint
    proc->main_thread = process_createThread(proc, 0x0, THREAD_FLAG_DEFAULT);
    if (current_cpu->current_thread) {
        // Reuse kernel-stack
        kfree((void*)(proc->main_thread->kstack - PROCESS_KSTACK_SIZE));
        proc->main_thread->kstack = current_cpu->current_thread->kstack;
    }

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
    proc->heap_base = elf_getHeapLocation(elf_binary);
    proc->heap = proc->heap_base;

    // Populate image
    elf_createImage(elf_binary);

    // Get the entrypoint
    uintptr_t process_entrypoint = elf_getEntrypoint(elf_binary);
    arch_initialize_context(proc->main_thread, process_entrypoint, proc->main_thread->stack);

    // We own this process
    thread_t *old = current_cpu->current_thread;
    current_cpu->current_thread = proc->main_thread;
    if (old) old->kstack = 0;

    *entry = process_entrypoint;

    // Done with ELF
    kfree((void*)elf_binary);
    return 0; // Enough loaded
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
int process_executeDynamic(char *path, vfs_file_t *file, int argc, char **argv, char **envp) {
    // Execute dynamic loader
    // First, try to open the interpreter that's in PT_INTERP
    char *interpreter_path = elf_getInterpreter(file);
    vfs_file_t *interpreter;
    int r = -1;
    if (interpreter_path) {
        // We have an interpreter path
        LOG(INFO, "Trying to execute interpreter: %s\n", interpreter_path);
        r = vfs_open(interpreter_path, O_RDONLY, &interpreter);
        kfree(interpreter_path);
    }

    // Strike 2
    if (r) {
        // Backup path: Run /usr/lib/ld.so
        LOG(INFO, "Trying to load interpreter: /usr/lib/ld.so\n");
        r = vfs_open("/usr/lib/ld.so", O_RDONLY, &interpreter);
    }

    // Strike 3
    if (r) {
        LOG(ERR, "No interpreter available\n");
        return -ENOENT;
    }

    // Setup new name
    // TODO: This should be a *pointer* to argv[0], not a duplicate.
    kfree(current_cpu->current_process->name);
    current_cpu->current_process->name = strdup(argv[0]); // ??? will this work?

    // Do common execution
    uintptr_t entry;
    assert(!process_executeCommon(interpreter, &entry));

    vfs_close(interpreter);

    // Load the dynamic file
    elf_dynamic_info_t info;
    if (elf_loadDynamicELF(file, &info)) {
        LOG(ERR, "Error loading dynamic ELF file\n");
        return -ENOEXEC;
    }

    current_cpu->current_process->exe_image = file;

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
    arch_start_execution(entry, current_cpu->current_thread->stack);
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
int process_execute(char *path, vfs_file_t *file, int argc, char **argv, char **envp) {
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

    // Do the inner execution
    uintptr_t process_entrypoint;
    assert(!process_executeCommon(file, &process_entrypoint));
    current_cpu->current_process->exe_image = file;

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

    // Now free the remainders
    for (int i = 0; i < envc; i++) {
        kfree(envp[i]);
    }

    kfree(envp);

    for (int i = 0; i < argc; i++) {
        kfree(argv[i]);
    }

    kfree(argv);

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

    // Now we need to mark all threads of this process as stopping. This will ensure that memory is fully separate
    spinlock_acquire(&process->thread_lock);
    
    thread_t *t = process->thread_list;
    while (t) {
        __sync_or_and_fetch(&t->status, THREAD_STATUS_STOPPING);
        if ((t->status & THREAD_STATUS_STOPPED) == 0 && t != current_cpu->current_thread) {
            sleep_wakeup(t);

            while ((t->status & THREAD_STATUS_STOPPED) == 0) {
                // LOG(DEBUG, "process_exit waiting for thread %d to stop\n", t->tid);
                arch_pause_single();
            }
        }

        t = t->next;
    }

    spinlock_release(&process->thread_lock);

    process->flags |= PROCESS_STOPPED;

    // Now we can mark it as stopped

    // TODO: Ugly
    process->exit_status = status_code;

    // Deparent the children
    if (process->node && process->node->children) {
        foreach (cnode, process->node->children) {
            tree_node_t *tnode = (tree_node_t*)cnode->value;
            process_t *child = (process_t*)tnode->value;
            child->parent = (process_t*)process_tree->root->value;
        }

        // Signal init and let it cleanup
        EVENT_SIGNAL(&((process_t*)process_tree->root->value)->wait_event);
    }

    // The parent process will destroy us
    // If our parent is waiting, wake them up
    if (process->parent) {
        if ((process->parent->flags & PROCESS_RUNNING)) signal_send(process->parent, SIGCHLD);
        EVENT_SIGNAL(&process->parent->wait_event);
    } 

    // To the next process we go. Once the scheduler is unlocked we can be caught and destroyed.
    // !!!: This approach seems very finnicky.
    if (process == current_cpu->current_process) {
        thread_exit();
    }
}

/**
 * @brief Fork the current process
 * @returns Depends on what you are. Only call this from system call context.
 */
pid_t process_fork() {
    // First we create our child pprocess
    process_t *parent = current_cpu->current_process;   
    process_t *child = process_create(parent, parent->name, parent->flags, parent->priority);
    child->main_thread = process_createThread(child, (uintptr_t)NULL, THREAD_FLAG_CHILD);

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
    process_t *proc = current_cpu->current_process;

    for (;;) {
        // todo lock
        if (!proc->node->children || !proc->node->children->length) {
            return -ECHILD;
        }
        

        foreach(cnode, proc->node->children) {
            process_t *child = ((tree_node_t*)cnode->value)->value;
            
            if (pid < -1 && child->gid != (gid_t)(-pid)) continue;
            else if (pid == 0 && child->gid != proc->gid) continue;
            else if (pid > 0 && child->pid != pid) continue;
            
            if (child->flags & PROCESS_STOPPED) {
                // dead
                pid_t r = child->pid;

                if (wstatus){
                    if (child->exit_reason == PROCESS_EXIT_NORMAL) {
                        *wstatus = (child->exit_status << 8);
                    } else {
                        *wstatus = (child->exit_status & 0x7f);
                    }
                }

                process_destroy(child);
                return r;
            }

            if (child->flags & PROCESS_SUSPENDED) {
                if (options & WSTOPPED || (child->ptrace.tracer == NULL && options & WUNTRACED)) {
                    pid_t r = child->pid;
                    if (wstatus) *wstatus = (child->exit_status << 8) | 0x7f;
                    return r;
                }
            }

            // todo the rest of the garbage
        }

        if (options & WNOHANG) {
            return 0;
        } else {
            // TODO make this process less racey
            event_listener_t l;
            EVENT_INIT_LISTENER(&l);
            EVENT_ATTACH(&l, &proc->wait_event);
            int r = EVENT_WAIT(&l, -1);
            if (r < 0) return r;
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
pid_t process_createUserThread(uintptr_t stack, uintptr_t tls, void *entry, void *arg) {
    thread_t *thr = process_createThread(current_cpu->current_process, (uintptr_t)entry, THREAD_FLAG_CHILD);
    thr->stack = stack; // Fix stack
    LOG(DEBUG, "Thread %p (TID=%d) created for process.\n", thr, thr->tid);

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


/**
 * @brief Create a new kernel-mode thread
 * @param process The kernel process to append
 * @param flags Thread flags
 * @param entry The thread entrypoint
 * @param arg An argument for the thread
 * @returns The new thread
 */
thread_t *process_createKernelThread(process_t *process, unsigned int flags, void *entry, void *arg) {
    thread_t *thr = process_createThread(process, (uintptr_t)&arch_enter_kthread, flags | THREAD_FLAG_KERNEL);

    THREAD_PUSH_STACK(SP(thr->context), void*, arg);
    THREAD_PUSH_STACK(SP(thr->context), void*, entry);

    return thr;
}
