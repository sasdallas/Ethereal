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
#include <kernel/loader/elfv2.h>
#include <kernel/misc/util.h>

#include <structs/tree.h>
#include <structs/list.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Process tree */
tree_t *process_tree = NULL;

/* Global process list */
list_t *process_list = NULL;
spinlock_t process_list_lock = SPINLOCK_INITIALIZER;

/* Last used PID */
static int process_last_pid = 1;

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

    // Idle exit
    if (current_cpu->current_process == current_cpu->idle_process) {
        timemonitor_updateIdleExit();
    }

    // Update CPU variables
    current_cpu->current_thread = next_thread;
    current_cpu->current_process = next_thread->parent;
    
    // Idle enter
    if (current_cpu->current_process == current_cpu->idle_process) {
        timemonitor_updateIdleEntry();
    }

    // Setup page directory
    vmm_switch(current_cpu->current_thread->ctx);

    // On your mark...
    arch_prepare_switch(current_cpu->current_thread);

    // Get set..
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_RUNNING);

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
    // Calculate times on context switch
    timemonitor_updateContextSwitch();

    // If the idle process is being rescheduled handle that
    if (current_cpu->current_process == current_cpu->idle_process) {
        current_cpu->current_thread->flags &= ~(THREAD_FLAG_NEEDS_RESCHED);
        return process_switchNextThread();
    }

    // Do we even have a thread?
    if (current_cpu->current_thread == NULL) {
        // Just switch to next thread
        return process_switchNextThread();
    }

    // Exit the thread if its trying to stop
    if (current_cpu->current_thread->status & THREAD_STATUS_STOPPING) {
        return thread_exit();
    }

    // Thread no longer has any time to execute. Save FPU registers
    // TODO: DESPERATELY move this to context structure.
    // TODO: Lazy FPU
#if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
    asm volatile ("fxsave64 (%0)" :: "r"(current_cpu->current_thread->fp_regs));
#endif

    // Equivalent to a setjmp, use arch_save_context() to save our context
    if (arch_save_context(&current_cpu->current_thread->context) == 1) {
        // We are back, and were chosen to be executed. Return

    #if defined(__ARCH_I386__) || defined(__ARCH_X86_64__)
        asm volatile ("fxrstor64 (%0)" :: "r"(current_cpu->current_thread->fp_regs));
    #endif

        return;
    }
    
    // Get current thread
    thread_t *prev = current_cpu->current_thread;

    // Get next thread in queue
    thread_t *next_thread = scheduler_get();
    if (!next_thread) {
        kernel_panic_extended(SCHEDULER_ERROR, "scheduler", "*** No thread was found in the scheduler (or something has been corrupted). Got thread %p.\n", next_thread);
    }

    // Clear resched flag so we dont get preempted in here
    next_thread->flags &= ~(THREAD_FLAG_NEEDS_RESCHED);

    // Update CPU variables
    current_cpu->current_thread = next_thread;
    current_cpu->current_process = next_thread->parent;

    // Entering idle process
    if (current_cpu->current_process == current_cpu->idle_process) {
        timemonitor_updateIdleEntry();
    }

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

    arch_yield(prev, current_cpu->current_thread);
    __builtin_unreachable();
}

/**
 * @brief Allocate a new PID from the PID bitmap
 */
pid_t process_allocatePID() {
    return __atomic_fetch_add(&process_last_pid, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Free a PID after process destruction
 * @param pid The PID to free
 */
void process_freePID(pid_t pid) {
    // TODO: PID recycling properly
}

/**
 * @brief Get a process from a PID
 * @param pid The pid to check for
 * @returns The process if found, otherwise NULL
 */
process_t *process_getFromPID(pid_t pid) {
    // TODO: Gotta be a better way to do this..
    if (current_cpu->current_process->pid == pid) {
        return current_cpu->current_process;
    }

    spinlock_acquire(&process_list_lock);
    foreach(proc_node, process_list) {
        process_t *proc = (process_t*)proc_node->value;
        if (proc) {
            if (proc->pid == pid) {
                spinlock_release(&process_list_lock);
                return proc;
            }
        }
    }

    spinlock_release(&process_list_lock);
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
    process->state = PROCESS_RUNNING;

    if (parent && parent->flags & PROCESS_TRACE_SYS) {
        process->flags |= PROCESS_TRACE_SYS;
    }

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

    if (parent) {
        process->umask = parent->umask;
    } else {
        process->umask = 0022;
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
    if (process_list) {
        spinlock_acquire(&process_list_lock);
        list_append_node(process_list, &process->proc_list_node);
        spinlock_release(&process_list_lock);
    }

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
    for (;;) arch_pause();
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
    process_t *idle = process_createStructure(NULL, "idle", PROCESS_KERNEL, PRIORITY_LOW);
    
    // !!!: Hack
    process_freePID(idle->pid);
    idle->pid = -1; // Not actually a process
    
    // !!!: remove
    // if (process_list) list_delete(process_list, list_find(process_list, (void*)idle));

    // Create a new thread
    idle->main_thread = process_createThread(idle, (uintptr_t)&kernel_idle, THREAD_FLAG_KERNEL);

    timemonitor_updateProcessStart(idle->main_thread);
    return idle;
}

/**
 * @brief Destroy a zombie process
 */
void process_destroyZombie(process_t *proc) {
    while (proc->state != PROCESS_ZOMBIE) {
        LOG(DEBUG, "process_destroyZombie awaiting process threads to exit\n");
        process_yield(1);
    }

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

        // accumulate l time into current process time
        current_cpu->current_process->proc_times.cutime += l->times.utime;
        current_cpu->current_process->proc_times.cstime += l->times.stime;

        thread_t *next = l->next;
        thread_destroy(l);
        l = next;
    }


    if (proc->node) {
        tree_remove(process_tree, proc->node);
    }

    spinlock_acquire(&process_list_lock);
    list_delete(process_list, &proc->proc_list_node);
    spinlock_release(&process_list_lock);

    process_freePID(proc->pid);
    kfree(proc->name);
    kfree(proc);
}

/**
 * @brief Totally destroy a process
 * @param proc The process to destroy
 * 
 * Doesn't free the process but turns it into a zombie
 */
void process_destroy(process_t *proc) {
    assert(proc->nthreads == 0);
    LOG(DEBUG, "Destroying process \"%s\" (%p)...\n", proc->name, proc);

    if (proc->exe_image) vfs_close(proc->exe_image);
    inode_release(proc->wd_node);

    // Destroy table of fds
    fd_destroyTable(proc);

    // Destroy the remainder of the context
    if (proc->ctx && proc->ctx != vmm_kernel_context) {
        vmm_destroyContext(proc->ctx);
    }

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
        proc->ptrace.tracer = NULL;
        spinlock_release(&trace->lock);
    }

extern void systemfs_proc_destroy(process_t *proc);
    systemfs_proc_destroy(proc);

    kfree(proc->cmdline);
    kfree(proc->wd_path);

    // Now we are a zombie
    proc->state = PROCESS_ZOMBIE;

    // If our parent is waiting, wake them up
    if (proc->parent) {
        signal_send(proc->parent, SIGCHLD);
        EVENT_SIGNAL(&proc->parent->wait_event);
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
    process_t *init = process_createStructure(NULL, "init", 0, PRIORITY_HIGH);
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
int process_executeCommon(elf_image_t *img) {
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
    if (oldctx && oldctx != vmm_kernel_context) {
        vmm_destroyContext(oldctx);
    }
    
    // Create a new main thread with a blank entrypoint
    proc->main_thread = process_createThread(proc, 0x0, THREAD_FLAG_DEFAULT);
    if (current_cpu->current_thread) {
        // Reuse kernel-stack
        kfree((void*)(proc->main_thread->kstack - PROCESS_KSTACK_SIZE));
        proc->main_thread->kstack = current_cpu->current_thread->kstack;
    }

    // Load the ELF image
    int ret = elf_loadImage(img);
    if (ret != 0) {
        return ret;
    }

    // Close any file descriptors marked "close-on-exec"
    fd_table_t *tbl = current_cpu->current_process->fd_table;
    for (int fd = 0; fd < tbl->table_size; fd++) {
        if (fd_isCloseExecute(fd)) {
            LOG(DEBUG, "Closing file descriptor %d, it is marked as O_CLOEXEC (0x%x)\n", fd, tbl->fds[fd]->flags);
            fd_remove(fd);
        }
    }

    // Initialize the context
    arch_initialize_context(proc->main_thread, img->entrypoint, proc->main_thread->stack);

    // We own this process
    thread_t *old = current_cpu->current_thread;
    current_cpu->current_thread = proc->main_thread;
    if (old) {
        // !!! SEVERE HACK. BY SETTING OLD->KSTACK = 0 THEN IT WONT BE FREED IN THREAD_DESTROY
        old->kstack = 0;
        // thread_destroy(old);
    }

    // Done with ELF
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
    elf_image_t file_img;
    int r = elf_openImage(file, &file_img);
    if (r != 0) {
        return r;
    }
    assert(file_img.interp_path);

    // Next, search for the interpreter
    char *interpreter_path = file_img.interp_path;
    vfs_file_t *interpreter;
    r = -1;
    if (interpreter_path) {
        // We have an interpreter path
        LOG(INFO, "Trying to execute interpreter: %s\n", interpreter_path);
        r = vfs_open(interpreter_path, O_RDONLY, &interpreter);
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

    // Initialize an interpreter image
    elf_image_t interp_img;
    r = elf_openImage(interpreter, &interp_img);
    
    // Interpreter is useless now
    vfs_close(interpreter);
    
    if (r != 0) {
        return r;
    }

    // Verify the interpreter is executable
    if (elf_checkImage(&interp_img, ELF_EXECUTABLE) == 0) {
        LOG(WARN, "Interpreter is not executable\n");
        elf_destroyImage(&file_img);
        elf_destroyImage(&interp_img);
        vfs_close(file);
        return -ENOEXEC;
    }


    // Setup new name
    // TODO: This should be a *pointer* to argv[0], not a duplicate.
    kfree(current_cpu->current_process->name);
    current_cpu->current_process->name = strdup(argv[0]);
    current_cpu->current_process->exe_image = file; // no need to hold an extra reference to file as it will be destroyed on process_destroy
    current_cpu->current_process->cmdline = argv;

    // Do common execution
    assert(process_executeCommon(&interp_img) == 0);

    // Load the file's image
    r = elf_loadImage(&file_img);
    assert(r == 0); // TODO: error handling

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

    // Build an auxiliary vector
    elf_auxv_t auxv;
    assert(elf_buildAuxv(&file_img, &auxv) == 0); // TODO: Error handling

    // Realign the stack if we need to. Everything from now on should JUST be a uintptr_t
    // The pending amount of bytes: Auxiliary vector variables, argc arguments, envc environment variables, plus argc itself and the two NULLs
    uintptr_t bytes = (auxv.entry_count * sizeof(elf_auxv_entry_t)) + (argc * sizeof(uintptr_t)) + (envc * sizeof(uintptr_t)) + (3 * sizeof(uintptr_t));
    if (!IS_ALIGNED(current_cpu->current_thread->stack - bytes, 16)) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, 0x0);
    }
    
    // Push the auxv
    for (int i = auxv.entry_count-1; i >= 0; i--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, auxv.entries[i].value);
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, auxv.entries[i].type);
    }
    

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

    // Cleanup memory before beginning execution
    uintptr_t interp_entry = interp_img.entrypoint;
    for (int i = 0; i < envc; i++) kfree(envp[i]);
    kfree(envp);
    elf_destroyImage(&interp_img);
    elf_destroyImage(&file_img);

    // Before entering the process we need to get its current time
    timemonitor_updateProcessStart(current_cpu->current_thread);

    // Enter
    arch_prepare_switch(current_cpu->current_thread);
    arch_start_execution(interp_entry, current_cpu->current_thread->stack);
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

    // Load the file image
    elf_image_t img;
    int r = elf_openImage(file, &img);
    if (r != 0) {
        return r;
    }

    // First, check if the file requires an interpreter
    if (img.interp_path) {
        // Yes, it is, run the process with ld.so
        LOG(INFO, "Running dynamic executable\n");
        elf_destroyImage(&img); // !!!
        return process_executeDynamic(path, file, argc, argv, envp);
    }

    // Check the ELF binary
    if (elf_checkImage(&img, ELF_EXECUTABLE) == 0) {
        // Not a valid ELF binary
        LOG(ERR, "Invalid ELF binary detected when trying to start execution\n");
        elf_destroyImage(&img);
        return -EINVAL;
    }

    // Setup new name
    // TODO: This should be a *pointer* to argv[0], not a duplicate.
    kfree(current_cpu->current_process->name);
    current_cpu->current_process->name = strdup(argv[0]); // ??? will this work?

    // Do inner load
    uintptr_t entry;
    assert(!process_executeCommon(&img));
    current_cpu->current_process->exe_image = file;

    // Build the auxiliary vector
    elf_auxv_t auxv;
    assert(!elf_buildAuxv(&img, &auxv));
    uintptr_t saved_entry = img.entrypoint;

    // Push auxiliary vector
    for (int i = auxv.entry_count-1; i >= 0; i--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, auxv.entries[i].value);
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, auxv.entries[i].type);
    }

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
    // The pending amount of bytes: Auxiliary vector variables, argc arguments, envc environment variables, plus argc itself and the two NULLs
    uintptr_t bytes = (auxv.entry_count * sizeof(elf_auxv_entry_t)) + (argc * sizeof(uintptr_t)) + (envc * sizeof(uintptr_t)) + (3 * sizeof(uintptr_t));
    if (!IS_ALIGNED(current_cpu->current_thread->stack - bytes, 16)) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, 0x0);
    }

    // Push envp/argv pointers on stack
    char **user_envp = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int e = envc; e > 0; e--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, envp_pointers[e-1]);
    }

    char **user_argv = NULL;
    THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, NULL);
    for (int a = argc; a > 0; a--) {
        THREAD_PUSH_STACK(current_cpu->current_thread->stack, char*, argv_pointers[a-1]);
    }

    THREAD_PUSH_STACK(current_cpu->current_thread->stack, uintptr_t, argc);

    // Cleanup memory
    for (int i = 0; i < envc; i++) kfree(envp[i]);
    kfree(envp);
    for (int i = 0; i < argc; i++) kfree(argv[i]);
    kfree(argv);
    elf_destroyImage(&img);

    // Before entering the process we need to get its current time
    timemonitor_updateProcessStart(current_cpu->current_thread);

    // Go!
    arch_prepare_switch(current_cpu->current_thread);
    arch_start_execution(img.entrypoint, current_cpu->current_thread->stack);
    
    // should not reach
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

    // Try to exit
    if (__atomic_exchange_n(&process->exiting, true, __ATOMIC_SEQ_CST) == true) {
        // Process is already exiting
        return thread_exit();
    }

    // Now we need to mark all threads of this process as stopping. This will ensure that memory is fully separate
    spinlock_acquire(&process->thread_lock);
    thread_t *t = process->thread_list;
    spinlock_release(&process->thread_lock);

    while (t) {
        __sync_or_and_fetch(&t->status, THREAD_STATUS_STOPPING);
        if ((t->status & THREAD_STATUS_STOPPED) == 0 && t != current_cpu->current_thread) {
            sleep_wakeup(t);

            while ((t->status & THREAD_STATUS_STOPPED) == 0) {
                // LOG(DEBUG, "process_exit waiting for thread %d to stop\n", t->tid);
                arch_pause();
            }
        }

        t = t->next;
    }


    // Now we can mark it as stopped.
    process->state = PROCESS_STOPPED;
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

    // To the next process we go. Once the scheduler is unlocked we can be caught and destroyed.
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
    timemonitor_updateProcessStart(child->main_thread);
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


        event_listener_t l = NULL;

        if ((options & WNOHANG) == 0) {
            EVENT_INIT_LISTENER(&l);
            EVENT_ATTACH(&l, &proc->wait_event);
        }

        foreach(cnode, proc->node->children) {
            process_t *child = ((tree_node_t*)cnode->value)->value;
            
            if (pid < -1 && child->gid != (gid_t)(-pid)) continue;
            else if (pid == 0 && child->gid != proc->gid) continue;
            else if (pid > 0 && child->pid != pid) continue;
            
            if (child->state == PROCESS_ZOMBIE) {
                // dead 
                pid_t r = child->pid;

                if (wstatus){
                    if (child->exit_reason == PROCESS_EXIT_NORMAL) {
                        *wstatus = (child->exit_status << 8);
                    } else {
                        *wstatus = (child->exit_status & 0x7f);
                    }
                }


                if ((options & WNOHANG) == 0) {
                    EVENT_DETACH(&l);
                    EVENT_DESTROY_LISTENER(&l);
                }

                process_destroyZombie(child);
                return r;
            }

            if (child->state == PROCESS_SUSPENDED) {
                if (options & WSTOPPED || (child->ptrace.tracer == NULL && options & WUNTRACED)) {
                    pid_t r = child->pid;
                    if (wstatus) *wstatus = (child->exit_status << 8) | 0x7f;
                    

                    if ((options & WNOHANG) == 0) {
                        EVENT_DETACH(&l);
                        EVENT_DESTROY_LISTENER(&l);
                    }

                    return r;
                }
            }

            // todo the rest of the garbage
        }

        if (options & WNOHANG) {
            return 0;
        } else {
            int r = EVENT_WAIT(&l, -1);
            EVENT_DETACH(&l);
            EVENT_DESTROY_LISTENER(&l);
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

    timemonitor_updateProcessStart(thr);
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
