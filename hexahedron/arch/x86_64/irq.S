/**
 * @file hexahedron/arch/x86_64/irq.S
 * @brief Handles exceptions, IRQs, etc.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.code64
.extern hal_exceptionHandler
.extern hal_interruptHandler
.extern lapic_base

/* Swap GS base (make sure points to err_code) */
.macro _swapgs
    // We only need to swapgs on a usermode -> kernel mode interrupt
    cmpq $8, 24(%rsp) // 24(%rsp) = CS pushed by CPU
    je 1f
    swapgs
1:
.endm

/* Save registers macro */
.macro PUSH_REGISTERS
    _swapgs // If usermode, then swapgs to switch to kernel GS base

    // AMD killed PUSHA for some weird reason, so we'll have to
    // do this manually - and this time we go alphabetical.
    pushq %rax
    pushq %rbx 
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp

    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8

    movw %ds, %ax
    pushw %ax

    /* This is pushing extended extended registers */
    
    /* Toss the stack to below the idtr (uint16 + uint64 = 10 bytes * 2 = 20 bytes) */
    sub $20, %rsp
    sgdt (%rsp)
    sidt 10(%rsp)

    
    movq %cr4, %rax
    pushq %rax
    movq %cr3, %rax
    pushq %rax
    movq %cr2, %rax
    pushq %rax
    movq %cr0, %rax
    pushq %rax
.endm

/* Restore registers macro */
.macro RESTORE_REGISTERS
    /* Skip over extended registers */
    add $52, %rsp

    /* Start popping registers */
    popw %ax
    movw %ax, %ds

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

    // Swap GS back
    _swapgs

    // Skip over error code and int no
    add $16, %rsp
.endm

/* Common exception handler */
halCommonExceptionHandler:
    PUSH_REGISTERS // Push registers

    // Clear directional flag
    cld

    leaq 52(%rsp), %rax     // Load address of registers_t

    movq %rax, %rdi                 // registers_t
    movq %rsp, %rsi                 // extended_registers_t

    movq %rsp, %rbp                 // ABI
    and $0xFFFFFFFFFFFFFFF0, %rsp   // Align stack for SSE instructions
    call hal_exceptionHandler 
    mov %rbp, %rsp                  // Restore stack
    
    RESTORE_REGISTERS // Restore registers
    
    iretq   // NOTE: The q is required!


/* Common interrupt handler */
halCommonIRQHandler:
    PUSH_REGISTERS // Push registers

    // Clear directional flag
    cld

    leaq 52(%rsp), %rax     // Load address of registers_t

    movq %rax, %rdi                 // registers_t
    movq %rsp, %rsi                 // extended_registers_t

    movq %rsp, %rbp                 // ABI
    and $0xFFFFFFFFFFFFFFF0, %rsp   // Align stack for SSE instructions
    call hal_interruptHandler 
    mov %rbp, %rsp                  // Restore stack

    RESTORE_REGISTERS // Restore registers
    
    iretq   // NOTE: The q is required!


/* Exception macros */
.macro ISR_NOERRCODE name index
    .global \name
    &name:
        pushq $0 // Push dummy error code
        pushq $\index
        jmp halCommonExceptionHandler
.endm

.macro ISR_ERRCODE name index
    .global \name
    &name:
        pushq $\index // Push index
        jmp halCommonExceptionHandler
.endm

/* IRQ macro */
.macro IRQ name index
    .global \name 
    &name:
        pushq $0 // Push dummy error code 
        pushq $\index // Push index
        jmp halCommonIRQHandler
.endm

.macro LAPIC_IRQ name index
    .global \name
    &name:
        pushq $0
        pushq $\index
        jmp halCommonIRQHandler
.endm


/* Handler definitions */
ISR_NOERRCODE   halDivisionException,               0
ISR_NOERRCODE   halDebugException,                  1
ISR_NOERRCODE   halNMIException,                    2
ISR_NOERRCODE   halBreakpointException,             3
ISR_NOERRCODE   halOverflowException,               4
ISR_NOERRCODE   halBoundException,                  5
ISR_NOERRCODE   halInvalidOpcodeException,          6
ISR_NOERRCODE   halNoFPUException,                  7
ISR_ERRCODE     halDoubleFaultException,            8
ISR_NOERRCODE   halCoprocessorSegmentException,     9 // If you get this, god help you. This is reserved.
ISR_ERRCODE     halInvalidTSSException,             10
ISR_ERRCODE     halSegmentNotPresentException,      11
ISR_ERRCODE     halStackSegmentException,           12
ISR_ERRCODE     halGeneralProtectionException,      13
ISR_ERRCODE     halPageFaultException,              14
ISR_NOERRCODE   halReservedException,               15
ISR_NOERRCODE   halFloatingPointException,          16
ISR_ERRCODE     halAlignmentCheck,                  17
ISR_NOERRCODE   halMachineCheck,                    18
ISR_NOERRCODE   halSIMDFloatingPointException,      19
ISR_NOERRCODE   halVirtualizationException,         20
ISR_ERRCODE     halControlProtectionException,      21
/* 22 - 27 are reserved */
ISR_NOERRCODE   halHypervisorInjectionException,    28 // mfw hypervisor injection exception
ISR_ERRCODE     halVMMCommunicationException,       29
ISR_ERRCODE     halSecurityException,               30
ISR_NOERRCODE   halReserved2Exception,              31

IRQ             halIRQ0,    32
IRQ             halIRQ1,    33
IRQ             halIRQ2,    34
IRQ             halIRQ3,    35
IRQ             halIRQ4,    36
IRQ             halIRQ5,    37
IRQ             halIRQ6,    38
IRQ             halIRQ7,    39
IRQ             halIRQ8,    40
IRQ             halIRQ9,    41
IRQ             halIRQ10,   42
IRQ             halIRQ11,   43
IRQ             halIRQ12,   44
IRQ             halIRQ13,   45
IRQ             halIRQ14,   46
IRQ             halIRQ15,   47

LAPIC_IRQ       halMSI0,    48
LAPIC_IRQ       halMSI1,    49
LAPIC_IRQ       halMSI2,    50
LAPIC_IRQ       halMSI3,    51
LAPIC_IRQ       halMSI4,    52
LAPIC_IRQ       halMSI5,    53
LAPIC_IRQ       halMSI6,    54
LAPIC_IRQ       halMSI7,    55
LAPIC_IRQ       halMSI8,    56
LAPIC_IRQ       halMSI9,    57
LAPIC_IRQ       halMSI10,   58
LAPIC_IRQ       halMSI11,   59
LAPIC_IRQ       halMSI12,   60
LAPIC_IRQ       halMSI13,   61
LAPIC_IRQ       halMSI14,   62
LAPIC_IRQ       halMSI15,   63


/* ISR 123 */
.global halLocalAPICTimerInterrupt
halLocalAPICTimerInterrupt:
    // Try to acknowledge EOI in APIC
    // Now process
    pushq $0x00
    pushq $123
    jmp halCommonIRQHandler

/* ISR 124 */
.global halTLBShootdownInterrupt
halTLBShootdownInterrupt:
    // Try to acknowledge EOI in APIC
    pushq %rcx
    mov (lapic_base)(%rip), %rcx    // RIP-relative addressing
    add $0xB0, %rcx                 // lapic_base + 0xB0 = EOI register
    movl $0, (%rcx)
    popq %rcx

    // Now process
    pushq $0x00
    pushq $124
    jmp halCommonIRQHandler

/* ISR 128 */
.global halSystemCallInterrupt
halSystemCallInterrupt:
    pushq $0x00
    pushq $128
    jmp halCommonIRQHandler

/* System call entrypoint (for SYSCALL instruction) */
.global halSyscallEntrypoint
.extern hal_syscallHandler
halSyscallEntrypoint:
    swapgs                          // CS isn't pushed, this only happens from usermode

    /* Fix stacks. Stolen from Toaru */
    mov %rsp, %gs:0x30
    mov %gs:0x28, %rsp

    /* Now we need to make a struct regs layout ourselves */
    pushq $0x23                     // SS
    pushq %gs:0x30                  // RSP
    push %r11                       // AMD: "SYSCALL" stores EFLAGS in R11
    pushq $0x1b                     // CS
    push %rcx                       // AMD: "SYSCALL" stores RIP in RCX

    // This is really stupid because we can't really touch RCX/R11 so we have to restore them
    pushq $0                        // err_code
    pushq $128                      // int_no

    push %rax                       // RAX
    push %rbx                       // RBX
    pushq $0                        // Don't bother pushing RCX
    push %rdx                       // RDX
    push %rsi                       // RSI
    push %rdi                       // RDI
    push %rbp                       // RBP
    
    push %r15                       // R15
    push %r14                       // R14
    push %r13                       // R13
    push %r12                       // R12
    pushq $0                        // Don't bother pushing R11
    push %r10                       // R10
    push %r9                        // R9
    push %r8                        // R8

    movw %ds, %ax
    pushw %ax

    /* This is pushing extended extended registers */
    
    /* Toss the stack to below the idtr (uint16 + uint64 = 10 bytes * 2 = 20 bytes) */
    sub $20, %rsp
    sgdt (%rsp)
    sidt 10(%rsp)

    // Clear directional flag
    cld
    
    movq %cr4, %rax
    pushq %rax
    movq %cr3, %rax
    pushq %rax
    movq %cr2, %rax
    pushq %rax
    movq %cr0, %rax
    pushq %rax

    leaq 52(%rsp), %rax             // Load address of registers_t

    movq %rax, %rdi                 // registers_t
    movq %rsp, %rsi       

    /* Call C handler */
    movq %rsp, %rbp                 // ABI
    and $0xFFFFFFFFFFFFFFF0, %rsp   // Align stack for SSE instructions
    call hal_syscallHandler 
    movq %rbp, %rsp                 // Restore stack
    
    
    /* Skip over extended registers */
    add $52, %rsp

    /* Start popping registers */
    popw %ax

    pop %r8
    pop %r9
    pop %r10
    add $8, %rsp
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    add $8, %rsp
    pop %rbx
    pop %rax

    /* Get RIP and restore stack/efl */
    add $16, %rsp
    pop %rcx
    add $8, %rsp  
    pop %r11
    pop %rsp

    /* Go! */
    swapgs
    sysretq 