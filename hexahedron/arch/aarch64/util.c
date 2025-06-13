/**
 * @file hexahedron/arch/aarch64/util.c
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

#include <kernel/arch/aarch64/hal.h>
#include <kernel/arch/aarch64/arch.h>
#include <kernel/arch/arch.h>

/* Generic parameters */
extern generic_parameters_t *parameters;


/**
 * @brief Returns the current CPU active in the system
 */
int arch_current_cpu() {
    return 0;
}

/**
 * @brief Get the generic parameters
 */
generic_parameters_t *arch_get_generic_parameters() {
    return parameters;
}

/**
 * @brief Pause execution on the current CPU for one cycle
 */
void arch_pause() {
    // Halt here. This will allow for an interrupt to catch us

}

/**
 * @brief Determine whether the interrupt fired came from usermode 
 * 
 * Useful to main timer logic to know when to switch tasks.
 */
int arch_from_usermode(registers_t *registers, extended_registers_t *extended) {
    return 0;
}

/**
 * @brief Prepare to switch to a new thread
 * @param thread The thread to prepare to switch to
 */
void arch_prepare_switch(struct thread *thread) {
    // Ask HAL to nicely load the kstack
    return;
}

/**
 * @brief Initialize the thread context
 * @param thread The thread to initialize the context for
 * @param entry The requested entrypoint for the thread
 * @param stack The stack to use for the thread
 */
void arch_initialize_context(struct thread *thread, uintptr_t entry, uintptr_t stack) {
}

/**
 * @brief Mount KernelFS nodes
 */
void arch_mount_kernelfs() {
}