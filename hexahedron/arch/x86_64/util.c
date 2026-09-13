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

#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <signal.h>
#include <kernel/arch/x86_64/arch.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/drivers/x86/clock.h>
#include <kernel/fs/systemfs.h>
#include <kernel/arch/arch.h>
#include <kernel/task/process.h>
#include <kernel/debug.h>
#include <ethereal/user.h>
#include <kernel/init.h>
#include <string.h>
#include <stdio.h>

/* External parameters */
extern generic_parameters_t *parameters;

/* Helper */
#define COPY_USER_REG(uregs, thread, reg) thread->regs->reg = user_regs->reg
#define COPY_THREAD_REG(uregs, thread, reg) user_regs->reg = (uint64_t)thread->regs->reg

/**
 * @brief Returns the current CPU active in the system
 */
inline int arch_current_cpu() {
    return smp_getCurrentCPU();
}

/**
 * @brief Get the generic parameters
 */
generic_parameters_t *arch_get_generic_parameters() {
    return parameters;
}

/**
 * @brief Wait for next interrupt
 */
void arch_pause() {
	unsigned long flags;
	asm volatile(
        "pushf\n"
        "pop %0\n"
        "sti\n"
        "hlt\n"
        "push %0\n"
        "popf\n"
        : "=r"(flags) :: "memory", "cc");
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

	memset(thread->fp_regs, 0, sizeof(thread->fp_regs));
	*(uint16_t*)thread->fp_regs = 0x37F;
	*(uint32_t*)(&thread->fp_regs[24]) = 0x1f80;
}

/**
 * @brief /system/cpus/XXX
 */
static ssize_t arch_cpu_systemfs(systemfs_node_t *n) {
	processor_t *cpu = (processor_t*)n->priv;

	return systemfs_printf(n,
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
			cpu->info.model,
			cpu->info.vendor,
			cpu->info.family,
			cpu->info.model_number,
			cpu->current_context->dir,
			cpu->current_process ? cpu->current_process->name : "N/A");
}

/**
 * @brief Mount SystemFS nodes
 */
int arch_mount_systemfs() {
	systemfs_node_t *dir = systemfs_createDirectory(systemfs_root, "cpus");
	for (int i = 0; i < MAX_CPUS; i++) {
		if (processor_data[i].cpu_id || !i) {
			char name[128];
			snprintf(name, 128, "cpu%d", i);
			systemfs_registerSimple(dir, name, arch_cpu_systemfs, NULL,  &processor_data[i]);
		}
	}
	return 0;
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
 * @brief Set the usermode GS base
 * @param gs The GS base to set
 */
void arch_set_user_gsbase(uintptr_t gs) {
	asm volatile ("wrmsr" : : "c"(0xc0000102), "d"((uint32_t)(gs >> 32)), "a"((uint32_t)(gs & 0xFFFFFFFF)));
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
 * @brief Prepare signal frame
 */
int arch_prepare_signal_frame(thread_t *thread, int signal, registers_t *regs, uintptr_t handler, void *restorer, int flags, sigset_t blocked) {
	// Figure out what stack to place this on
	// Occasionally a process can take the fault from the altstack itself
	uintptr_t stack;
	bool on_alt_stack = signal_onAltStack(thread, regs->rsp);
	if (!on_alt_stack && (flags & SA_ONSTACK) && thread->signal.altstack.ss_size) {
		stack = (uintptr_t)(thread->signal.altstack.ss_sp) + thread->signal.altstack.ss_size;
	} else {
		stack = regs->rsp - 128; // red-zone alignment
	}

	stack -= sizeof(siginfo_t);
	stack &= ~0xFUL;
	siginfo_t *si = (siginfo_t *)stack;

	stack -= sizeof(ucontext_t);
	stack &= ~0xFUL;
	ucontext_t *ucontext = (ucontext_t*)stack;

	// restorer must be here, such that on a return from a signal handler function
	// it jumps to it rather than just crash. mlibc sets this by default to be a
	// handler that calls sigreturn
	THREAD_PUSH_STACK(stack, uintptr_t, restorer);

	// like the only usage of vmm_validate ever
	if (!vmm_validate(stack, (uintptr_t)(si + 1) - stack, VMM_PTR_USER)) {
		dprintf(ERR, "arch_prepare_signal_frame detected invalid stack %p\n", stack);
		return -EFAULT;
	}

	// Zero them
	memset(ucontext, 0, sizeof(ucontext_t));
    memset(si, 0, sizeof(siginfo_t));

	// Prepare ucontext
	ucontext->uc_sigmask = blocked;
    ucontext->uc_stack = thread->signal.altstack;
	if (thread->signal.altstack.ss_size == 0) {
		ucontext->uc_stack.ss_flags = SS_DISABLE;
	}
	ucontext->uc_flags = 0;
    // uc_link is unused for signals

    unsigned long *gregs = ucontext->uc_mcontext.gregs;
    gregs[MCONTEXT_REG_R8] = regs->r8;
    gregs[MCONTEXT_REG_R9] = regs->r9;
    gregs[MCONTEXT_REG_R10] = regs->r10;
    gregs[MCONTEXT_REG_R11] = regs->r11;
    gregs[MCONTEXT_REG_R12] = regs->r12;
    gregs[MCONTEXT_REG_R13] = regs->r13;
    gregs[MCONTEXT_REG_R14] = regs->r14;
    gregs[MCONTEXT_REG_R15] = regs->r15;
    gregs[MCONTEXT_REG_RDI] = regs->rdi;
    gregs[MCONTEXT_REG_RSI] = regs->rsi;
    gregs[MCONTEXT_REG_RBP] = regs->rbp;
    gregs[MCONTEXT_REG_RBX] = regs->rbx;
    gregs[MCONTEXT_REG_RDX] = regs->rdx;
    gregs[MCONTEXT_REG_RAX] = regs->rax;
    gregs[MCONTEXT_REG_RCX] = regs->rcx;
    gregs[MCONTEXT_REG_RSP] = regs->rsp;
    gregs[MCONTEXT_REG_RIP] = regs->rip;
    gregs[MCONTEXT_REG_EFL] = regs->rflags;
    gregs[MCONTEXT_REG_ERR] = regs->err_code;
    gregs[MCONTEXT_REG_TRAPNO] = regs->int_no;
    gregs[MCONTEXT_REG_OLDMASK] = blocked;
    gregs[MCONTEXT_REG_CSGSFS] = 0x1B; // todo

	// !!! hack but due to the way SIGSEGV is sent, CR2 should still have the fault address...
	// !!! this is very hit or miss, and CR2 should probably be saved in trap frame
    asm volatile ("movq %%cr2, %0" : "=a"(gregs[MCONTEXT_REG_CR2]));
	
	// save FPU registers
    uint8_t fxbuf[512] __attribute__((aligned(16)));
    memset(fxbuf, 0, sizeof(fxbuf));
    asm volatile ("fxsave64 (%0)" :: "r"(fxbuf) : "memory");
    memcpy(&ucontext->__fpregs_mem, fxbuf, sizeof(ucontext->__fpregs_mem));
    ucontext->uc_mcontext.fpregs = &ucontext->__fpregs_mem;

	// get siginfo ready
	memcpy(si, &thread->signal.info[signal], sizeof(siginfo_t));
	si->si_signo = signal;

    regs->rdi = signal;
    regs->rsi = (uintptr_t)si;
    regs->rdx = (uintptr_t)ucontext;
    regs->rax = 0;
    regs->rsp = stack;
    regs->rip = handler;

	return 0;
}


/**
 * @brief Restore a thread from a signal frame
 * @param thr The thread to restore
 * @param regs The register frame to restore into
 * @param uctx User context
 */
int arch_restore_signal_frame(thread_t *thr, registers_t *regs, void *_uctx) {
    ucontext_t *uctx = (ucontext_t *)_uctx;

    if (!vmm_validate((uintptr_t)uctx, sizeof(ucontext_t), VMM_PTR_USER)) {
        return -EFAULT;
    }

    unsigned long *gregs = uctx->uc_mcontext.gregs;
    regs->r8  = gregs[MCONTEXT_REG_R8];
    regs->r9  = gregs[MCONTEXT_REG_R9];
    regs->r10 = gregs[MCONTEXT_REG_R10];
    regs->r11 = gregs[MCONTEXT_REG_R11];
    regs->r12 = gregs[MCONTEXT_REG_R12];
    regs->r13 = gregs[MCONTEXT_REG_R13];
    regs->r14 = gregs[MCONTEXT_REG_R14];
    regs->r15 = gregs[MCONTEXT_REG_R15];
    regs->rdi = gregs[MCONTEXT_REG_RDI];
    regs->rsi = gregs[MCONTEXT_REG_RSI];
    regs->rbp = gregs[MCONTEXT_REG_RBP];
    regs->rbx = gregs[MCONTEXT_REG_RBX];
    regs->rdx = gregs[MCONTEXT_REG_RDX];
    regs->rax = gregs[MCONTEXT_REG_RAX];
    regs->rcx = gregs[MCONTEXT_REG_RCX];
    regs->rsp = gregs[MCONTEXT_REG_RSP];
    regs->rip = gregs[MCONTEXT_REG_RIP];
    regs->rflags = (gregs[MCONTEXT_REG_EFL] & 0x50DD5) | 0x202;
    regs->cs = 0x1b;
    regs->ss = 0x23;

    // amazing code
    if (uctx->uc_mcontext.fpregs) {
        if (vmm_validate((uintptr_t)uctx->uc_mcontext.fpregs, sizeof(struct _fpstate), VMM_PTR_USER)) {
            uint8_t fxbuf[512] __attribute__((aligned(16)));
            memcpy(fxbuf, uctx->uc_mcontext.fpregs, sizeof(fxbuf));

            *(uint32_t *)(fxbuf + 24) &= 0xFFBF; // fixes a weird MXCSR bug i was seeing

            asm volatile ("fxrstor64 (%0)" :: "r"(fxbuf) : "memory");
            memcpy(thr->fp_regs, fxbuf, sizeof(fxbuf));
        }
    }

    return 0;
}

/* Init routines */
FS_INIT_ROUTINE(arch_systemfs, INIT_FLAG_DEFAULT, arch_mount_systemfs, systemfs);
