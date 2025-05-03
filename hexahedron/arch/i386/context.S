/**
 * @file hexahedron/arch/i386/context.S
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
    /* Prevent leaking EBP and set it to a debug marker */
    mov $0xDEADDEAD, %ebp 

    /* The first passed parameter is entrypoint, the second is stack */
    /* EAX -> entrypoint, ECX -> stack */
    movl 4(%esp), %eax
    movl 8(%esp), %ecx

    /* Now we need to push the frame that IRET wants */
    /* It expects SS, the stack, EFLAGS, CS, and EIP */
    pushl $0x23
    pushl %ecx
    
    /* We should set the IF bit in EFLAGS */
    pushf
    pop %ecx
    or $0x200, %ecx
    pushl %ecx

    /* Now push CS:EIP and IRET */
    pushl $0x1b
    pushl %eax
    iret


.global arch_save_context
arch_save_context:
    /*
        Layout of the context structure:
        - ESP: 0
        - EBP: 4
        - EBX: 8
        - EDI: 12
        - ESI: 16
        - EIP: 20
    */

    /* Load structure -> ECX */
    movl 4(%esp), %ecx
    
    /* Load ESP into the structure */
    leal 8(%esp), %eax
    movl %eax, 0(%ecx)

    /* Load remaining variables */
    movl %ebp, 4(%ecx)
    movl %ebx, 8(%ecx)
    movl %edi, 12(%ecx)
    movl %esi, 16(%ecx)

    /* Load IP */
    movl 0(%esp), %eax
    movl %eax, 20(%ecx)

    /* Clear EAX for return value */
    xor %eax, %eax
    ret

.global arch_load_context
arch_load_context:
    /*
        Layout of the context structure:
        - ESP: 0
        - EBP: 4
        - EBX: 8
        - EDI: 12
        - ESI: 16
        - EIP: 20
    */

    /* Load structure -> ECX */
    movl 4(%esp), %ecx

    /* Restore */
    movl 0(%ecx), %esp
    movl 4(%ecx), %ebp
    movl 8(%ecx), %ebx
    movl 12(%ecx), %edi
    movl 16(%ecx), %esi

    /* arch_save_context returns twice, load that second return value in */
    movl $1, %eax
    jmpl *20(%ecx)

/* Enter kernel thread */
.global arch_enter_kthread
arch_enter_kthread:
    /* Pop kthread pointer and jump to it */
    popl %esi
    pushl $0xBAD11BAD       // Each thread will pop this as return address. THEY SHOULD NOT RETURN.
    jmpl *%esi

/* Restore context */
.global arch_restore_context
arch_restore_context:
    /* Restore segments */
    popw %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Restore main registers */
    popa

    /* Skip over the err code and interrupt number */
    addl $8, %esp

    /* Return */
    iret