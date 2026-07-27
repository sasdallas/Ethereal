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

/* change these if you modify the thread structure */
#define CONTEXT_OFFSET          0xA8    // thread->context
#define STATUS_OFFSET           0x10    // thread->status
#define THREAD_STATUS_SLEEPING  0x08
#define SLEEP_LOCK_OFFSET       0x74    // thread->sleep.lock.lock

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

/* Restore context from stack */
.global arch_restore_context
arch_restore_context:
    // fork and createUserThread need to bypass
    movq %rax, %rdi
    call thread_entrypoint

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

__unlock_thread:
    movq 0x10(%rdi), %rcx
    and $0x8, %rcx              // THREAD_STATUS_SLEEPING
    jz .no_need

    /* unlock thread sleep queue */
    movq %rdi, %rcx 
    add $0x74, %rcx
    movb $0, %al
    lock xchgb %al,(%rcx)
 
.no_need:
    ret

// main context switcher
// called at the very end of yield
.global arch_switch_context
arch_switch_context:
    /* previous thread in RDI, next in RSI, requeue flag in RDX, irq state in RCX */

    /* save */
    movq %rdx, %r11
    test %rdi, %rdi
    jz .start_load

    /* save the old thread's context */
    leaq 8(%rsp), %rax // reads the RSP from the stack
    movq %rax, (CONTEXT_OFFSET+0)(%rdi)
    movq %rbp, (CONTEXT_OFFSET+8)(%rdi)
    movq %rbx, (CONTEXT_OFFSET+16)(%rdi)
    movq %r12, (CONTEXT_OFFSET+24)(%rdi)
    movq %r13, (CONTEXT_OFFSET+32)(%rdi)
    movq %r14, (CONTEXT_OFFSET+40)(%rdi)
    movq %r15, (CONTEXT_OFFSET+48)(%rdi)
    movq $0xC0000100, %rcx
    rdmsr
    movl %eax, (CONTEXT_OFFSET+56)(%rdi)
    movl %edx, (CONTEXT_OFFSET+60)(%rdi)
    movq (%rsp), %rax // reads IP from the stack
    movq %rax, (CONTEXT_OFFSET+64)(%rdi)

.start_load:

    /* begin loading new thread's context */
    movq (CONTEXT_OFFSET+0)(%rsi), %rsp
    movq (CONTEXT_OFFSET+8)(%rsi), %rbp
    movq (CONTEXT_OFFSET+16)(%rsi), %rbx
    movq (CONTEXT_OFFSET+24)(%rsi), %r12
    movq (CONTEXT_OFFSET+32)(%rsi), %r13
    movq (CONTEXT_OFFSET+40)(%rsi), %r14
    movq (CONTEXT_OFFSET+48)(%rsi), %r15
    
    /* the thread's sleep queue can now be unlocked */
    test %rdi, %rdi
    jz .done_unlock
    call __unlock_thread

.done_unlock:

    /* restore fsbase */
    movl (CONTEXT_OFFSET+56)(%rsi), %eax
    movl (CONTEXT_OFFSET+60)(%rsi), %edx
    movq $0xC0000100, %rcx
    wrmsr

    /* this is the RIP to get back to */
    movq (CONTEXT_OFFSET+64)(%rsi), %rcx

    /* depending on reschedule flag, return thread in RAX */
    xorq %rax, %rax
    test %r11, %r11
    cmovnzq %rdi, %rax

    /* jump into the new thread */
    pushq %rcx
    ret

.global arch_thread_entry
.extern thread_entrypoint
arch_thread_entry:
    movq %rax, %rdi
    call thread_entrypoint
    popq %rdi // context
    popq %rsi // function
    jmpq *%rsi

.global arch_handle_threadexit
.extern thread_safeExit
arch_handle_threadexit:
    // What this does is switch to the idle thread's stack then execute a function off of that.
    // Idle thread in %rdi, next thread in %rsi

    // Load the kernel stack for the idle thread
    addq $0xA8, %rdi
    movq 0(%rdi), %rsp
    movq 8(%rdi), %rbp
    movq 16(%rdi), %rbx
    movq 24(%rdi), %r12
    movq 32(%rdi), %r13
    movq 40(%rdi), %r14
    movq 48(%rdi), %r15

    movq %rsi, %rdi
    jmp thread_safeExit

.global arch_execute_safe
arch_execute_safe:
    movq %rdx, %rsp
    andq $0xFFFFFFFFFFFFFFF0, %rsp
    xchg %rdi, %rsi
    callq %rsi
