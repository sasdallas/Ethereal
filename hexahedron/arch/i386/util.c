/**
 * @file hexahedron/arch/i386/util.c
 * @brief Utility functions provided to generic parts of kernel
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/arch.h>
#include <kernel/arch/i386/smp.h>
#include <kernel/arch/arch.h>
#include <kernel/task/process.h>

/* Generic parameters */
extern generic_parameters_t *parameters;


/**
 * @brief Returns the current CPU active in the system
 */
int arch_current_cpu() {
    return smp_getCurrentCPU();
}

/**
 * @brief Get the generic parameters
 */
generic_parameters_t *arch_get_generic_parameters() {
    return parameters;
}

/**
 * @brief Pause execution on the current CPU until preemption
 */
void arch_pause() {
    // Halt here. This will allow for an interrupt to catch us
    asm volatile (  "sti\n"
                    "hlt\n"
                    "cli\n");
}

/**
 * @brief Pause execution on the CPU for one cycle
 */
void arch_pause_single() {
    asm volatile ("pause");
}

/**
 * @brief Determine whether the interrupt fired came from usermode 
 * 
 * Useful to main timer logic to know when to switch tasks.
 */
int arch_from_usermode(registers_t *registers, extended_registers_t *extended) {
    return (registers->cs != 0x08);
}

/**
 * @brief Prepare to switch to a new thread
 * @param thread The thread to prepare to switch to
 */
void arch_prepare_switch(struct thread *thread) {
    // Ask HAL to nicely load the kstack
    hal_loadKernelStack(thread->kstack);
}

/**
 * @brief Initialize the thread context
 * @param thread The thread to initialize the context for
 * @param entry The requested entrypoint for the thread
 * @param stack The stack to use for the thread
 */
void arch_initialize_context(struct thread *thread, uintptr_t entry, uintptr_t stack) {
    thread->context.eip = entry;
    thread->context.esp = stack;
    thread->context.ebp = stack;
}

/**
 * @brief Mount KernelFS nodes
 */
void arch_mount_kernelfs() {
}


/**
 * @brief Set the usermode TLS base
 * @param tls The TLS base to set
 * 
 * This should also reflect in the context when saved/restored
 */
void arch_set_tlsbase(uintptr_t tls) {
    // TODO
}