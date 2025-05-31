/**
 * @file hexahedron/arch/x86_64/util.c
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

#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/arch/arch.h>
#include <kernel/processor_data.h>

/* External parameters */
extern generic_parameters_t *parameters;

/**
 * @brief Returns the current CPU active in the system
 */
int arch_current_cpu() {
    return current_cpu->cpu_id;
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
    asm volatile (  "sti\n"
                    "hlt\n"
                    "cli\n");
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
    hal_loadKernelStack(thread->parent->kstack);
}

/**
 * @brief Initialize the thread context
 * @param thread The thread to initialize the context for
 * @param entry The requested entrypoint for the thread
 * @param stack The stack to use for the thread
 */
void arch_initialize_context(struct thread *thread, uintptr_t entry, uintptr_t stack) {
    thread->context.rip = entry;
    thread->context.rsp = stack;
    thread->context.rbp = stack;
}

/**
 * @brief /kernel/pat entry in the kernelfs
 */
static int arch_pat_kernelfs(struct kernelfs_entry *entry, void *data) {
	// https://github.com/klange/toaruos/blob/1a551db5cf47aa8ae895983b2819d5ca19be53a3/kernel/vfs/procfs.c#L367
    uint32_t pat_value_low, pat_value_high;
	asm volatile ( "rdmsr" : "=a" (pat_value_low), "=d" (pat_value_high): "c" (0x277) );
	uint64_t pat_values = ((uint64_t)pat_value_high << 32) | (pat_value_low);

	const char * pat_names[] = {
		"uncacheable (UC)",
		"write combining (WC)",
		"Reserved",
		"Reserved",
		"write through (WT)",
		"write protected (WP)",
		"write back (WB)",
		"uncached (UC-)"
	};

	int pa_0 = (pat_values >>  0) & 0x7;
	int pa_1 = (pat_values >>  8) & 0x7;
	int pa_2 = (pat_values >> 16) & 0x7;
	int pa_3 = (pat_values >> 24) & 0x7;
	int pa_4 = (pat_values >> 32) & 0x7;
	int pa_5 = (pat_values >> 40) & 0x7;
	int pa_6 = (pat_values >> 48) & 0x7;
	int pa_7 = (pat_values >> 56) & 0x7;

	kernelfs_writeData(entry,
			"PA0: %d %s\n"
			"PA1: %d %s\n"
			"PA2: %d %s\n"
			"PA3: %d %s\n"
			"PA4: %d %s\n"
			"PA5: %d %s\n"
			"PA6: %d %s\n"
			"PA7: %d %s\n",
			pa_0, pat_names[pa_0],
			pa_1, pat_names[pa_1],
			pa_2, pat_names[pa_2],
			pa_3, pat_names[pa_3],
			pa_4, pat_names[pa_4],
			pa_5, pat_names[pa_5],
			pa_6, pat_names[pa_6],
			pa_7, pat_names[pa_7]);
    return 0;
}

/**
 * @brief Mount KernelFS nodes
 */
void arch_mount_kernelfs() {
    kernelfs_createEntry(NULL, "pat", arch_pat_kernelfs, NULL);
}