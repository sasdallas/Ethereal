/**
 * @file hexahedron/include/kernel/arch/x86_64/interrupt.h
 * @brief x86_64 interrupt handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_INTERRUPT_H
#define KERNEL_ARCH_X86_64_INTERRUPT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/x86_64/registers.h>

/**** TYPES ****/

/* IDT structures */

typedef struct _x86_64_interrupt_descriptor {
    uint16_t base_lo;       // Low bits of the base (0 - 15)
    uint16_t selector;      // Code segment selector in GDT or LDT
    uint8_t ist;            // Interrupt stack table offset
    uint8_t flags;          // Gate type, DPL, and P fields
    uint16_t base_mid;      // Middle bits of the base (16 - 31)
    uint32_t base_hi;       // High bits of the base
    uint32_t reserved;      // Reserved (0)
} __attribute__((packed)) x86_64_interrupt_descriptor_t;

typedef struct _x86_64_idtr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) x86_64_idtr_t;


/* GDT structures */

// This structure is ugly to implement
typedef struct _x86_64_gdt_entry {
    uint16_t limit;                 // Maximum address
    uint16_t base_lo;               // Lower 16 bits of the base
    uint8_t base_mid;               // Next 8 bits of the base
    uint8_t access;                 // Access bits (determines segment ring)
    uint8_t flags;                  // Should contain flags, but also contains part of the 20-bit limit address.
                                    // Why would you do this, Intel?
    uint8_t base_hi;                // Final 8 bits of base
} __attribute__((packed)) x86_64_gdt_entry_t;

typedef struct _x86_64_gdtr {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) x86_64_gdtr_t;

// I hate long mode
typedef struct _x86_64_gdt_entry_hi {
    uint32_t base_higher;
    uint32_t reserved; // what are they saving these bits for?? x86_128?
} __attribute__((packed)) x86_64_gdt_entry_hi_t;

// I hate long mode again
typedef struct _x86_64_tss_entry {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) x86_64_tss_entry_t;

typedef struct _x86_64_gdt_table{
    x86_64_gdt_entry_t      entries[7];
    x86_64_gdt_entry_hi_t   tss_extra; // I hate you so much Intel. This is horrible.
} __attribute__((packed)) __attribute__((aligned(0x10))) x86_64_gdt_table_t;

// This structure is used because we need to have a more complex control
// over the GDT. As well as that, it needs to be aligned.
typedef struct _x86_64_gdt {
    x86_64_gdt_table_t  table;  // GDT table
    x86_64_tss_entry_t  tss;    // TSS
    x86_64_gdtr_t       gdtr;   // GDTR
} __attribute__((packed)) __attribute__((aligned(0x10))) x86_64_gdt_t;


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
#define X86_64_IDT_DESC_BIT16 0x06     // 00000110
#define X86_64_IDT_DESC_BIT32 0x0E     // 00001110
#define X86_64_IDT_DESC_RING1 0x40     // 01000000
#define X86_64_IDT_DESC_RING2 0x20     // 00100000
#define X86_64_IDT_DESC_RING3 0x60     // 01100000
#define X86_64_IDT_DESC_PRESENT 0x80   // 10000000
#define X86_64_MAX_INTERRUPTS  255
#define X86_64_MAX_EXCEPTIONS  31

// PIC definitions
#define X86_64_PIC1_ADDR       0x20                // Master PIC address
#define X86_64_PIC2_ADDR       0xA0                // Slave PIC address
#define X86_64_PIC1_COMMAND    (X86_64_PIC1_ADDR)     // Command address for master PIC
#define X86_64_PIC2_COMMAND    (X86_64_PIC2_ADDR)     // Command address for slave PIC
#define X86_64_PIC1_DATA       (X86_64_PIC1_ADDR)+1   // Data address for master PIC
#define X86_64_PIC2_DATA       (X86_64_PIC2_ADDR)+1   // Data address for slave PIC

#define X86_64_PIC_EOI             0x20        // End of interrupt code

// PIC ICW (initialization words)
#define X86_64_PIC_ICW1_ICW4       0x01        // Indicates that ICW4 is present
#define X86_64_PIC_ICW1_SINGLE     0x02        // Enables PIC cascade mode
#define X86_64_PIC_ICW1_INTERVAL4  0x04        // Call address interval 4
#define X86_64_PIC_ICW1_LEVEL      0x08        // Edge mode
#define X86_64_PIC_ICW1_INIT       0x10        // Initialization bit

#define X86_64_PIC_ICW4_8086       0x01        // Enables 8086/88 mode
#define X86_64_PIC_ICW4_AUTO       0x02        // Auto EOI 
#define X86_64_PIC_ICW4_BUF_SLAVE  0x08        // Enable buffered mode (slave)
#define X86_64_PIC_ICW4_BUF_MASTER 0x0C        // Enable buffered mode (master)
#define X86_64_PIC_ICW4_SFNM       0x10        // Special fully nested

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
extern void halSyscallEntrypoint(void);

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