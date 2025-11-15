/**
 * @file hexahedron/include/kernel/task/process.h
 * @brief Main process structure
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_PROCESS_H
#define KERNEL_TASK_PROCESS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <structs/list.h>
#include <time.h>

#include <kernel/task/thread.h>
#include <kernel/task/scheduler.h>
#include <kernel/task/sleep.h>
#include <kernel/task/fd.h>
#include <kernel/task/mem.h>
#include <kernel/task/signal.h>
#include <kernel/task/timer.h>
#include <kernel/task/ptrace.h>
#include <kernel/task/syscall.h>
#include <kernel/task/futex.h>

#include <kernel/mem/vas.h>

#include <kernel/processor_data.h>
#include <kernel/fs/vfs.h>
#include <structs/tree.h>

#include <kernel/arch/arch.h>


/**** DEFINITIONS ****/

#define PROCESS_MAX_PIDS            32768                                       // Maximum amount of PIDs. The kernel uses a bitmap to keep track of these
#define PROCESS_PID_BITMAP_SIZE     PROCESS_MAX_PIDS / (sizeof(uint32_t) * 8)   // Bitmap size

#define PROCESS_KSTACK_SIZE         PAGE_SIZE*4    // Kernel stack size

#define PROCESS_EXIT_NORMAL                 0           // The process chose to exit on its own will
#define PROCESS_EXIT_SIGNAL                 1           // The process was exited by a signal, and exit_status contains the signal number

/**** TYPES ****/

/**
 * @brief Kernel thread function
 * @param data User-specified data
 */
typedef void (*kthread_t)(void *data);

/**
 * @brief Image structure for a process
 */
typedef struct process_image {
    uintptr_t entry;                    // Process entrypoint
    uintptr_t tls;                      // TLS location
    size_t tls_size;                    // TLS size
} process_image_t;

/**
 * @brief The main process type
 */
typedef struct process {
    // GENERAL INFORMATION
    struct process *parent;             // Parent process
    char *name;                         // Name of the process
    int exit_status;                    // Exit statuscode
    int exit_reason;                    // Reason for exit

    // IDs
    pid_t pid;                          // Process ID
    pid_t pgid;                         // Process group/job
    pid_t sid;                          // Session ID

    uid_t uid;                          // Real user ID of the process
    uid_t euid;                         // Effective user ID of the process
    gid_t gid;                          // Real group ID of the process
    gid_t egid;                         // Effective group ID of the process

    gid_t *group_list;                  // Group list

    // SCHEDULER INFORMATION
    unsigned int flags;                 // Scheduler flags (running/stopped/started) - these can also be used by other parts of code
    unsigned int priority;              // Scheduler priority, see scheduler.h
    
    // QUEUE INFORMATION
    tree_node_t *node;                  // Node in the process tree
    list_t *waitpid_queue;              // Wait queue for the process

    // THREADS
    thread_t *main_thread;              // Main thread in the process  - whatever the ELF entrypoint was
    list_t  *thread_list;               // List of threads for the process

    // FILE INFORMATION
    char *wd_path;                      // Working directory path
    fd_table_t *fd_table;               // File descriptor table

    // MEMORY REGIONS
    uintptr_t heap;                     // Heap of the process. Positioned after the ELF binary
    uintptr_t heap_base;                // Base location of the heap
    vas_t *vas;                         // Process virtual address space
    list_t *mmap;                       // mmap() mappings

    // SIGNALS
    vas_allocation_t *userspace;        // Userspace allocation (only for sigtramp right now)

    // ELF
    process_image_t image;              // Image data for the process

    // TIMER
    process_timer_t itimers[3];         // setitimer timers
    
    // DEBUG
    process_ptrace_t ptrace;            // ptrace structure

    // OTHER
    uintptr_t kstack;                   // Kernel stack (see PROCESS_KSTACK_SIZE)
    vmm_context_t *ctx;                 // VMM context
    arch_context_t sigctx;              // Signal handler context
    pid_t tid_next;                     // Next TID to use
} process_t;

/**** MACROS ****/

#define PROC_IS_ROOT(proc) ((proc)->euid == 0)
#define PROC_IS_LEADER(proc) ((proc)->pgid == (proc)->pid)

/**** FUNCTIONS ****/

/**
 * @brief Initialize the process system, starting the idle process
 * 
 * This will NOT switch to the next task! Instead it will prepare the system
 * by creating the necessary structures and allocating an idle task for the
 * BSP.
 */
void process_init();

/**
 * @brief Switch to the next thread in the queue
 * 
 * For CPU cores: This is jumped to immediately after AP initialization, specifically 
 * after the idle task has been created (through process_spawnIdleTask). It will automatically
 * enter the scheduling loop, and when a new process spawns the core can get it.
 */
void __attribute__((noreturn)) process_switchNextThread();

/**
 * @brief Yield to the next task in the queue
 * 
 * This will yield current execution to the next available task, but will return when
 * this process is loaded by @c process_switchNextThread
 * 
 * @param reschedule Whether to readd the process back to the queue, meaning it can return whenever and isn't waiting on something
 */
void process_yield(uint8_t reschedule);

/**
 * @brief Create a new idle process
 * 
 * Creates and returns an idle process. All this process does is repeatedly
 * call @c arch_pause and try to switch to the next thread.
 * 
 * @note    This process should not be added to the process tree. Instead it is
 *          kept in the main process data structure, and will be switched to when there are no other processes
 *          to go to.
 */
process_t *process_spawnIdleTask();

/**
 * @brief Spawn a new init process
 * 
 * Creates and returns an init process. This process has no context, and is basically
 * an empty shell. Rather, when @c process_execute is called, it replaces the current process' (init)
 * threads and sections with the process to execute for init.
 */
process_t *process_spawnInit();

/**
 * @brief Create a new process
 * @param parent Parent process, or NULL if not needed
 * @param name The name of the process
 * @param flags The flags of the process
 * @param priority The priority of the process 
 */
process_t *process_create(process_t *parent, char *name, int flags, int priority);

/**
 * @brief Create a kernel process with a single thread
 * @param name The name of the kernel process
 * @param flags The flags of the kernel process
 * @param priority Process priority
 * @param entrypoint The entrypoint of the kernel process
 * @param data User-specified data
 * @returns Process structure
 */
process_t *process_createKernel(char *name, unsigned int flags, unsigned int priority, kthread_t entrypoint, void *data);

/**
 * @brief Execute a new ELF binary for the current process (execve)
 * @param path The path of the file being executed
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 */
int process_execute(char *path, fs_node_t *file, int argc, char **argv, char **envp);

/**
 * @brief Exiting from a process
 * @param process The process to exit from, or NULL for current process
 * @param status_code The status code
 */
void process_exit(process_t *process, int status_code);

/**
 * @brief Fork the current process
 * @returns Depends on what you are. Only call this from system call context.
 */
pid_t process_fork();

/**
 * @brief waitpid equivalent
 */
long process_waitpid(pid_t pid, int *wstatus, int options);

/**
 * @brief Get a process from a PID
 * @param pid The pid to check for
 * @returns The process if found, otherwise NULL
 */
process_t *process_getFromPID(pid_t pid);

/**
 * @brief Add a new thread to the current process (sort of similar to clone())
 * @param stack The stack of the thread to add
 * @param tls (Optional) Thread local stack
 * @param entry The thread entrypoint
 * @param arg An argument to the thread
 * @returns ID of the new thread
 * 
 * If @c tls is 0x0, then the TLS of the current thread will be used.
 */
pid_t process_createThread(uintptr_t stack, uintptr_t tls, void *entry, void *arg);

#endif