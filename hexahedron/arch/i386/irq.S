/**
 * @file hexahedron/arch/i386/irq.S
 * @brief Handles exceptions, IRQs, IDT/GDT installation, etc.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.code32

.extern hal_exceptionHandler
.extern hal_interruptHandler
.extern lapic_base
.global hal_gdtr
.global hal_gdt_base
.global hal_gdt_end

/* Save registers macro */
.macro PUSH_REGISTERS
    /* Push general registers */
    pusha

    /* Push DS */
    movw %ds, %ax
    pushw %ax

    /* This is pushing extended registers */
    
    /* Toss the stack to below the idtr (uint16 + uint32 = 6 bytes * 2 = 12 bytes) */
    sub $12, %esp
    sgdt (%esp)
    sidt 6(%esp)

    movl %cr4, %eax
    pushl %eax
    movl %cr3, %eax
    push %eax
    movl %cr2, %eax
    pushl %eax
    movl %cr0, %eax
    push %eax

    
.endm

.macro RESTORE_REGISTERS
    /* Skip over extended registers */
    addl $28, %esp

    /* Restore segments */
    popw %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Now we're at the beginning of registers_t */
    /* Restore main registers */
    popa

    /* Skip over the err code and interrupt number */
    addl $8, %esp
.endm
    


/* Common exception handler */
halCommonExceptionHandler:
    PUSH_REGISTERS // Push registers

    leal 28(%esp), %eax // Load registers_t structure
    
    pushl %esp  // extended_registers_t
    pushl %eax  // registers_t
    
    // Switch to kernel data sgements
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // Call handler
    call hal_exceptionHandler

    // RESTORE_REGISTERS expects stack aligned
    addl $8, %esp

    RESTORE_REGISTERS

    iret

/* Common IRQ handler */
halCommonIRQHandler:
    PUSH_REGISTERS // Push registers

    leal 28(%esp), %eax // Load registers_t structure
    
    pushl %esp // extended_registers_t
    pushl %eax // registers_t 

    // Switch to kernel data sgements
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // Call handler
    call hal_interruptHandler

    // Negate last 2 pushes
    addl $8, %esp

    RESTORE_REGISTERS

    iret



/* Exception macros */
.macro ISR_NOERRCODE name index
    .global \name
    &name:
        pushl $0 // Push dummy error code
        pushl $\index   // Push index

        // Now go to common handler
        jmp halCommonExceptionHandler
.endm

.macro ISR_ERRCODE name index
    .global \name
    &name:
        pushl $\index   // Push index

        // Now go to common handler
        jmp halCommonExceptionHandler
.endm

/* IRQ macro */
.macro IRQ name index
    .global \name 
    &name:
        pushl $0        // Push dummy error code
        pushl $\index   // Push index

        // Now go to common handler
        jmp halCommonIRQHandler
.endm




/* Handler definitions */
ISR_NOERRCODE   halDivisionException,               0
// debugException is a stub
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
    
/* ISR 1 */
.global halDebugException
halDebugException:
    /* STUB */
    iret

/* ISR 123 */
.global halLocalAPICTimerInterrupt
halLocalAPICTimerInterrupt:
    // Acknowledge the IPI
    pushl %ecx
    movl (lapic_base), %ecx
    addl $0xB0, %ecx
    movl $0, (%ecx)
    popl %ecx

    pushl $0x00
    pushl $123
    jmp halCommonIRQHandler

     
/* ISR 124 */
.global halTLBShootdownInterrupt
halTLBShootdownInterrupt:
    // Acknowledge the IPI
    pushl %ecx
    movl (lapic_base), %ecx
    addl $0xB0, %ecx
    movl $0, (%ecx)
    popl %ecx

    pushl $0x00
    pushl $124
    jmp halCommonIRQHandler

/* ISR 128 */
.global halSystemCallInterrupt
halSystemCallInterrupt:
    pushl $0x00
    pushl $128
    jmp halCommonIRQHandler