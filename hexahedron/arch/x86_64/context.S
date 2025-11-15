/**
 * @file hexahedron/arch/x86_64/context.S
 * @brief Context switching/loading/execution starting
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.section .text
.align 8

.global arch_start_execution
arch_start_execution:
    /* Clear RBP to a known value for debug */
    movq $0xDEADDEAD, %rbp  // Thread stack marker (debug)

    /* Now we need to push the frame that IRET wants */
    /* It expects SS, the stack, EFLAGS, CS, and EIP */
    pushq $0x23
    pushq %rsi
    
    /* We should set the IF bit in EFLAGS */
    pushf
    popq %rcx
    or $0x200, %rcx
    pushq %rcx

    /* Now push CS:EIP and IRET */
    push $0x1b
    push %rdi
    swapgs
    iretq


.global arch_save_context
arch_save_context:
    /*
        Layout of the context structure:
        - RSP: 0
        - RBP: 8
        - RBX: 16
        - R12: 24
        - R13: 32
        - R14: 40
        - R15: 48
        - FSBASE: 56
        - RIP: 64
    */

    /* Load RSP */
    leaq 8(%rsp), %rax
    movq %rax, 0(%rdi)
    
    /* Load other variables */
    movq %rbp, 8(%rdi)
    movq %rbx, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 32(%rdi)
    movq %r14, 40(%rdi)
    movq %r15, 48(%rdi)

    /* We need to read from the FSBASE MSR - 0xC0000100 */
    movq $0xC0000100, %rcx
    rdmsr

    /* RDMSR splits into low bits in EAX, high bits in EDX */
    movl %eax, 56(%rdi)
    movl %edx, 60(%rdi)

    /* Now read EIP */
    movq (%rsp), %rax
    movq %rax, 64(%rdi)

    /* Clear EAX for return value */
    xor %rax, %rax
    retq

.global arch_load_context
arch_load_context:
    /*
        Layout of the context structure:
        - RSP: 0
        - RBP: 8
        - RBX: 16
        - R12: 24
        - R13: 32
        - R14: 40
        - R15: 48
        - FSBASE: 56
        - RIP: 64
    */
    movq 0(%rdi), %rsp
    movq 8(%rdi), %rbp
    movq 16(%rdi), %rbx
    movq 24(%rdi), %r12
    movq 32(%rdi), %r13
    movq 40(%rdi), %r14
    movq 48(%rdi), %r15


    /* Restore FSBASE */
    movl 56(%rdi), %eax
    movl 60(%rdi), %edx
    movq $0xC0000100, %rcx
    wrmsr

    /* Set RAX to 1 for return value */
    mov $1, %rax

    /* Jump to RIP */
    jmpq *64(%rdi)

/* Enter kernel thread */
.global arch_enter_kthread
arch_enter_kthread:
    /* Pop function */
    popq %rsi

    /* Pop data into RDI */
    popq %rdi

    // The kernel is stupid and doesn't usually align our stack
    // So align the stack to 16-byte boundary (SSE instructions and whatnot need this)
    and $0xFFFFFFFFFFFFFFF0, %rsp

    /* Jump */
    xor %rbp, %rbp
    jmpq *%rsi

/* Restore context from stack */
.global arch_restore_context
arch_restore_context:
    popw %ax


    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx 
    popq %rax

    // Skip over error code and int no
    add $16, %rsp

    /* swapgs and return */
    swapgs
    iretq


/*     // Need to unlock the current CPU's queue?
    cmpq $1, %rsi
    jne .cont

    // We're out of the thread's kernel stack so we need to unlock
    movq %gs:0x90, %rax // %gs:0x90 = sched_cpu_t
    movb $0, %cl
    lock xchgb %cl, 0xC(%rax) // unlock the current CPU's queue 
.cont:
*/

__unlock_thread:
    movq 0x8(%rdi), %rcx
    and $0x8, %rcx              // THREAD_STATUS_SLEEPING
    jz .no_need

    /* unlock thread sleep queue */
    movq %rdi, %rcx 
    add $0x64, %rcx
    movb $0, %al
    lock xchgb %al, (%rcx)

.no_need:
    ret

.global arch_yield
arch_yield:
    // We have a decent amount of work to do here
    // thread->status (0x8), thread->sleep (0x48), thread->sleep.lock (0x58), thread->context (0x78)
    // prev in %rdi and dest in %rsi

    /* Load destination context */
    addq $0x88, %rsi
    movq 0(%rsi), %rsp
    movq 8(%rsi), %rbp
    movq 16(%rsi), %rbx
    movq 24(%rsi), %r12
    movq 32(%rsi), %r13
    movq 40(%rsi), %r14
    movq 48(%rsi), %r15


    /* Now we are off the thread's stack */
    call __unlock_thread

    // Unlock the CPU's queue
    movq %gs:0x90, %rax // %gs:0x90 = sched_cpu_t
    movb $0, %cl
    lock xchgb %cl, 0xC(%rax) // unlock the current CPU's queue 

    /* Restore FSBASE */
    movl 56(%rsi), %eax
    movl 60(%rsi), %edx
    movq $0xC0000100, %rcx
    wrmsr

    /* Set RAX to 1 for return value */
    mov $1, %rax

    /* Jump to RIP */
    jmpq *64(%rsi)