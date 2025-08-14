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
#include <kernel/arch/x86_64/smp.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/arch/arch.h>
#include <kernel/processor_data.h>
#include <stdio.h>

/* External parameters */
extern generic_parameters_t *parameters;

/* Helper */
#define COPY_USER_REG(uregs, thread, reg) thread->regs->reg = user_regs->reg
#define COPY_THREAD_REG(uregs, thread, reg) user_regs->reg = (uint64_t)thread->regs->reg

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
 * @brief Pause execution on the current CPU for one cycle
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
    thread->context.rip = entry;
    thread->context.rsp = stack;
    thread->context.rbp = stack;
}

/**
 * @brief /kernel/cpus/XXX in the kernelfs
 */
static int arch_cpu_kernelfs(struct kernelfs_entry *entry, void *data) {
	processor_t *cpu = (processor_t*)data;

	kernelfs_writeData(entry,
		"CpuId:%d\n"
		"LapicId:%d\n"
		"Model:%s\n"
		"Manufacturer:%s\n"
		"Family:0x%x\n"
		"ModelNumber:0x%x\n"
		"CurrentDirectory:%p\n"
		"CurrentProcess:%s\n",
			cpu->cpu_id,
			cpu->lapic_id,
			cpu->cpu_model,
			cpu->cpu_manufacturer,
			cpu->cpu_family,
			cpu->cpu_model_number,
			cpu->current_dir,
			cpu->current_process ? cpu->current_process->name : "N/A");

	return 0;
}

/**
 * @brief Mount KernelFS nodes
 */
void arch_mount_kernelfs() {
	kernelfs_dir_t *dir = kernelfs_createDirectory(NULL, "cpus", 1);
	for (int i = 0; i < MAX_CPUS; i++) {
		if (processor_data[i].cpu_id || !i) {
			char name[128];
			snprintf(name, 128, "cpu%d", i);
			kernelfs_createEntry(dir, name, arch_cpu_kernelfs, &processor_data[i]);
		}
	}
}

/**
 * @brief Set the usermode TLS base
 * @param tls The TLS base to set
 * 
 * This should also reflect in the context when saved/restored
 */
void arch_set_tlsbase(uintptr_t tls) {
	asm volatile ("wrmsr" : : "c"(0xc0000100), "d"((uint32_t)(tls >> 32)), "a"((uint32_t)(tls & 0xFFFFFFFF)));
}

/**
 * @brief Convert a thread's saved registers into a user context structure
 * @param user_regs The user registers structure to fill
 * @param thread The thread to use the registers from to fill
 */
void arch_to_user_regs(struct user_regs_struct *user_regs, struct thread *thread) {
	COPY_THREAD_REG(user_regs, thread, rax);
	COPY_THREAD_REG(user_regs, thread, rbx);
	COPY_THREAD_REG(user_regs, thread, rcx);
	COPY_THREAD_REG(user_regs, thread, rdx);
	COPY_THREAD_REG(user_regs, thread, rsi);
	COPY_THREAD_REG(user_regs, thread, rdi);
	COPY_THREAD_REG(user_regs, thread, rbp);
	COPY_THREAD_REG(user_regs, thread, r8);
	COPY_THREAD_REG(user_regs, thread, r9);
	COPY_THREAD_REG(user_regs, thread, r10);
	COPY_THREAD_REG(user_regs, thread, r11);
	COPY_THREAD_REG(user_regs, thread, r12);
	COPY_THREAD_REG(user_regs, thread, r13);
	COPY_THREAD_REG(user_regs, thread, r14);
	COPY_THREAD_REG(user_regs, thread, r15);
	COPY_THREAD_REG(user_regs, thread, rflags);
	COPY_THREAD_REG(user_regs, thread, rip);
	COPY_THREAD_REG(user_regs, thread, rsp);
	COPY_THREAD_REG(user_regs, thread, cs);
	COPY_THREAD_REG(user_regs, thread, ds);
	COPY_THREAD_REG(user_regs, thread, ss);
	COPY_THREAD_REG(user_regs, thread, int_no);
	COPY_THREAD_REG(user_regs, thread, err_code);
}

/**
 * @brief Convert user regs into a thread's saved registers
 * @param user_regs The user registers structure to use to fill
 * @param thread The thread to fill
 */
void arch_from_user_regs(struct user_regs_struct *user_regs, struct thread *thread) {
	COPY_USER_REG(user_regs, thread, rax);
	COPY_USER_REG(user_regs, thread, rbx);
	COPY_USER_REG(user_regs, thread, rcx);
	COPY_USER_REG(user_regs, thread, rdx);
	COPY_USER_REG(user_regs, thread, rsi);
	COPY_USER_REG(user_regs, thread, rdi);
	COPY_USER_REG(user_regs, thread, rbp);
	COPY_USER_REG(user_regs, thread, r8);
	COPY_USER_REG(user_regs, thread, r9);
	COPY_USER_REG(user_regs, thread, r10);
	COPY_USER_REG(user_regs, thread, r11);
	COPY_USER_REG(user_regs, thread, r12);
	COPY_USER_REG(user_regs, thread, r13);
	COPY_USER_REG(user_regs, thread, r14);
	COPY_USER_REG(user_regs, thread, r15);
	COPY_USER_REG(user_regs, thread, rflags);
	COPY_USER_REG(user_regs, thread, rip);
	COPY_USER_REG(user_regs, thread, rsp);
	COPY_USER_REG(user_regs, thread, cs);
	COPY_USER_REG(user_regs, thread, ds);
	COPY_USER_REG(user_regs, thread, ss);
	COPY_USER_REG(user_regs, thread, int_no);
	COPY_USER_REG(user_regs, thread, err_code);

	if (thread->syscall) {
		// TODO: Will this cause issues?
        thread->syscall->syscall_number = thread->regs->rax;
        thread->syscall->parameters[0] = thread->regs->rdi;
        thread->syscall->parameters[1] = thread->regs->rsi;
        thread->syscall->parameters[2] = thread->regs->rdx;
        thread->syscall->parameters[3] = thread->regs->r10;
        thread->syscall->parameters[4] = thread->regs->r8;
        thread->syscall->parameters[5] = thread->regs->r9;
		thread->syscall->return_value = thread->regs->rax;
	}
}

/**
 * @brief Set the single step state of a thread
 * @param thread The thread to set the single step state of
 * @param state On or off
 */
void arch_single_step(struct thread *thread, int state) {
	// Set TF in RFLAGS
	if (state) {
		thread->regs->rflags |= (1 << 8);
	} else {
		thread->regs->rflags &= ~(1 << 8);
	}
}

/**
 * @brief Read the current system tick count
 */
uint64_t arch_tick_count() {
	return clock_readTSC();
}