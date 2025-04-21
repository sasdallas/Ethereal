/**
 * @file hexahedron/include/kernel/arch/i386/interrupt.h
 * @brief Interrupt declarations for the I386 architecture
 * 
 * Implements basic structures for things like GDT/IDT
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_INTERRUPT_H
#define KERNEL_ARCH_I386_INTERRUPT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/i386/registers.h>

/**** TYPES ****/

typedef struct _i386_interrupt_descriptor {
    uint16_t base_lo;               // Low 16-bits of interrupt routine address
    uint16_t segment_selector;      // Code segment selector (in GDT)
    uint8_t reserved;               // Reserved
    uint8_t flags;                  // Gate type, DPL, P fields, etc.
    uint16_t base_hi;               // High 16-bits of interrupt routine address
} __attribute__((packed)) i386_interrupt_descriptor_t;

typedef struct _i386_idtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) i386_idtr_t;


// GDT descriptor
typedef struct _i386_gdt_descriptor {
    uint16_t limit;                 // Maximum address
    uint16_t base_lo;               // Lower 16 bits of the base
    uint8_t base_mid;               // Next 8 bits of the base
    uint8_t access;                 // Access bits (determines segment ring)
    uint8_t flags;                  // Should contain flags, but also contains part of the 20-bit limit address.
                                    // Why would you do this, Intel?
    uint8_t base_hi;                // Final 8 bits of base
} __attribute__((packed)) i386_gdt_descriptor_t;

// GDTR
typedef struct _i386_gdtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) i386_gdtr_t;

// TSS descriptor (NOTE: For certain registers, the top bits are reserved)
typedef struct _i386_tss {
    uint32_t link;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint32_t iopb;  // The bottom 16 bits of this are reserved
    uint32_t ssp;
} __attribute__((packed)) i386_tss_t;

// Full I386 GDT (packed structure for each core, makes installing GDTs much easier)
typedef struct _i386_gdt {
    i386_gdt_descriptor_t   entries[8];
    i386_tss_t              tss;
    i386_gdtr_t             gdtr;
} __attribute__((packed)) __attribute__((aligned(0x10))) i386_gdt;



// Interrupt/exception handlers

/**
 * @brief Interrupt handler that accepts registers
 * @param exception_index The index of the exception
 * @param interrupt_no IRQ number
 * @param regs Registers
 * @param extended Extended registers (for debugging, will not be restored)
 * @return 0 on success, anything else is failure
 */
typedef int (*interrupt_handler_t)(uintptr_t exception_index, uintptr_t interrupt_no, registers_t* regs, extended_registers_t* extended);

/**
 * @brief Exception handler
 * @param exception_index The index of the exception
 * @param regs Registers
 * @param extended Extended registers (for debugging, will not be restored)
 * @return 0 on success, anything else is failure
 */
typedef int (*exception_handler_t)(uintptr_t exception_index, registers_t* regs, extended_registers_t* extended);

/**
 * @brief Interrupt handler with context
 * @param context The context given to @c hal_registerInterruptContext
 * @returns 0 on success
 */
typedef int (*interrupt_handler_context_t)(void *context);

/**** DEFINITIONS ****/

// Copied straight from old kernel. Provide interrupt descriptor types.
#define I86_IDT_DESC_TRAP   0x0F    // 00001111
#define I86_IDT_DESC_BIT16  0x06    // 00000110
#define I86_IDT_DESC_BIT32  0x0E    // 00001110
#define I86_IDT_DESC_RING1  0x40    // 01000000
#define I86_IDT_DESC_RING2  0x20    // 00100000
#define I86_IDT_DESC_RING3  0x60    // 01100000
#define I86_IDT_DESC_PRESENT 0x80   // 10000000
#define I86_MAX_INTERRUPTS  255
#define I86_MAX_EXCEPTIONS  31

// PIC definitions
#define I86_PIC1_ADDR       0x20                // Master PIC address
#define I86_PIC2_ADDR       0xA0                // Slave PIC address
#define I86_PIC1_COMMAND    (I86_PIC1_ADDR)     // Command address for master PIC
#define I86_PIC2_COMMAND    (I86_PIC2_ADDR)     // Command address for slave PIC
#define I86_PIC1_DATA       (I86_PIC1_ADDR)+1   // Data address for master PIC
#define I86_PIC2_DATA       (I86_PIC2_ADDR)+1   // Data address for slave PIC

#define I86_PIC_EOI             0x20        // End of interrupt code

// PIC ICW (initialization words)
#define I86_PIC_ICW1_ICW4       0x01        // Indicates that ICW4 is present
#define I86_PIC_ICW1_SINGLE     0x02        // Enables PIC cascade mode
#define I86_PIC_ICW1_INTERVAL4  0x04        // Call address interval 4
#define I86_PIC_ICW1_LEVEL      0x08        // Edge mode
#define I86_PIC_ICW1_INIT       0x10        // Initialization bit

#define I86_PIC_ICW4_8086       0x01        // Enables 8086/88 mode
#define I86_PIC_ICW4_AUTO       0x02        // Auto EOI 
#define I86_PIC_ICW4_BUF_SLAVE  0x08        // Enable buffered mode (slave)
#define I86_PIC_ICW4_BUF_MASTER 0x0C        // Enable buffered mode (master)
#define I86_PIC_ICW4_SFNM       0x10        // Special fully nested

/**** FUNCTIONS ****/

/* EXTERNAL HANDLERS FROM irq.S */
extern void halDivisionException(void);
extern void halDebugException(void);
extern void halNMIException(void);
extern void halBreakpointException(void);
extern void halOverflowException(void);
extern void halBoundException(void);
extern void halInvalidOpcodeException(void);
extern void halNoFPUException(void);
extern void halDoubleFaultException(void);
extern void halCoprocessorSegmentException(void);
extern void halInvalidTSSException(void);
extern void halSegmentNotPresentException(void);
extern void halStackSegmentException(void);
extern void halGeneralProtectionException(void);
extern void halPageFaultException(void);
extern void halReservedException(void);
extern void halFloatingPointException(void);
extern void halAlignmentCheck(void);
extern void halMachineCheck(void);
extern void halSIMDFloatingPointException(void);
extern void halVirtualizationException(void);
extern void halControlProtectionException(void);
extern void halHypervisorInjectionException(void);
extern void halVMMCommunicationException(void);
extern void halSecurityException(void);
extern void halReserved2Exception(void);

extern void halLocalAPICTimerInterrupt(void);
extern void halTLBShootdownInterrupt(void);
extern void halSystemCallInterrupt(void);

extern void halIRQ0(void); // Interrupt number 32
extern void halIRQ1(void); // Interrupt number 33
extern void halIRQ2(void); // Interrupt number 34
extern void halIRQ3(void); // Interrupt number 35
extern void halIRQ4(void); // Interrupt number 36
extern void halIRQ5(void); // Interrupt number 37
extern void halIRQ6(void); // Interrupt number 38
extern void halIRQ7(void); // Interrupt number 39
extern void halIRQ8(void); // Interrupt number 40
extern void halIRQ9(void); // Interrupt number 41
extern void halIRQ10(void); // Interrupt number 42
extern void halIRQ11(void); // Interrupt number 43
extern void halIRQ12(void); // Interrupt number 44
extern void halIRQ13(void); // Interrupt number 45
extern void halIRQ14(void); // Interrupt number 46
extern void halIRQ15(void); // Interrupt number 47

#endif