/**
 * @file hexahedron/arch/x86_64/interrupt.c
 * @brief x86_64 interrupts and exceptions handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/interrupt.h>
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/smp.h>
#include <kernel/arch/x86_64/arch.h>
#include <kernel/drivers/x86/pic.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/hal.h>

#include <errno.h>
#include <string.h>

/* GDT */
x86_64_gdt_t gdt[MAX_CPUS] __attribute__((used)) = {{
    // Define basic core data - the code will use hal_setupGDTCoreData on each CPU core
    // to setup TSS data, and use another function to copy it to the SMP trampoline.
    {
        {
            {0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00},       // Null entry
            {0xFFFF, 0x0000, 0x00, 0x9A, 0xAF, 0x00},       // 64-bit kernel-mode code segment
            {0xFFFF, 0x0000, 0x00, 0x92, 0xAF, 0x00},       // 64-bit kernel-mode data segment
            {0xFFFF, 0x0000, 0x00, 0xFA, 0xAF, 0x00},       // 64-bit user-mode code segment
            {0xFFFF, 0x0000, 0x00, 0xF2, 0xAF, 0x00},       // 64-bit user-mode data segment
            {0xFFFF, 0x0000, 0x00, 0xFA, 0xAF, 0x00},       // (another) 64-bit user-mode code segment for SYSCALL instruction
            {0x0067, 0x0000, 0x00, 0xE9, 0x00, 0x00},       // 64-bit TSS
        },
        {0x00000000, 0x00000000}                            // Additional TSS data
    },
	{0, {0, 0, 0}, 0, {0, 0, 0, 0, 0, 0, 0}, 0, 0, 0},      // TSS
    {0x0000, 0x0000000000000000}                            // GDTR
}};

/* IDT */
x86_64_interrupt_descriptor_t hal_idt_table[X86_64_MAX_INTERRUPTS];

/* Exception handler table - TODO: More than one handler per exception? */
exception_handler_t hal_exception_handler_table[X86_64_MAX_EXCEPTIONS] = { 0 };

/* String table for exceptions */
const char *hal_exception_table[X86_64_MAX_EXCEPTIONS] = {
    "division error",
    "debug trap",
    "NMI exception",
    "breakpoint trap",
    "overflow trap",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack-segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "FPU exception",
    "alignment check",
    "machine check",
    "SIMD floating-point exception",
    "virtualization exception",
    "control protection exception",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "hypervisor injection exception",
    "VMM communication exception",
    "security exception"
};


/**
 * @brief Sets up a core's data in the global GDT
 * @param core The core number to setup
 */
static void hal_setupGDTCoreData(int core) {
    if (core > MAX_CPUS) return;

    // Copy the core's data from BSP
    if (core != 0) memcpy(&gdt[core], &gdt[0], sizeof(*gdt));

    // Setup the GDTR
    gdt[core].gdtr.limit = sizeof(gdt[core].table.entries) + sizeof(gdt[core].table.tss_extra) - 1;
    gdt[core].gdtr.base = (uintptr_t)&gdt[core].table.entries;

    // Configure the TSS entry
    uintptr_t tss = (uintptr_t)&gdt[core].tss;
    gdt[core].table.entries[6].limit = sizeof(gdt[core].tss);
    gdt[core].table.entries[6].base_lo = (tss & 0xFFFF);
    gdt[core].table.entries[6].base_mid = (tss >> 16) & 0xFF;
    gdt[core].table.entries[6].base_hi = (tss >> 24) & 0xFF;
    gdt[core].table.tss_extra.base_higher = (tss >> 32) & 0xFFFFFFFFF;
}

/**
 * @brief Load kernel stack
 * @param stack The stack to load
 */
void hal_loadKernelStack(uintptr_t stack) {
    gdt[smp_getCurrentCPU()].tss.rsp[0] = stack;
    current_cpu->kstack = stack;
}

/**
 * @brief Setup a core's data
 * @param core The core to setup data for
 * @param rsp The stack for the TSS
 */
void hal_gdtInitCore(int core, uintptr_t rsp) {
    if (!core) return;

    // Setup the TSS' RSP to point to our top of the stac
    gdt[core].tss.rsp[0] = (uintptr_t)rsp;

    // Load and install
    asm volatile (
        "lgdt %0\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw $0x30, %%ax\n" // 0x30 = 7th entry in the GDT (TSS)
        "ltr %%ax\n"
        :: "m"(gdt[core].gdtr) : "rax"
    ); 
}

/**
 * @brief Initializes and installs the GDT
 */
void hal_gdtInit() {
    // For every CPU core setup its data
    for (int i = 0; i < MAX_CPUS; i++) hal_setupGDTCoreData(i);

    // Setup the TSS' RSP to point to our top of the stack
    extern void *__stack_top;
    gdt[0].tss.rsp[0] = (uintptr_t)&__stack_top;

    // Load and install
    asm volatile (
        "lgdt %0\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw $0x30, %%ax\n" // 0x30 = 7th entry in the GDT (TSS)
        "ltr %%ax\n"
        :: "m"(gdt[0].gdtr) : "rax"
    ); 
}

/**
 * @brief Register a vector in the IDT table.
 * @warning THIS IS FOR INTERNAL USE ONLY. @see hal_registerInterruptHandler for
 *          a proper handler register 
 * 
 * @note This isn't static because some more advanced functions will need
 *       to set vectors up as usermode (with differing CPL/DPL)
 * 
 * @param index Index in the kernel IDT table (side note: this value cannot be wrong.)
 * @param flags The flags. Recommended to use X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32 for kernel mode,
 *              although you can OR by X86_64_IDT_DESC_RING3 for usermode
 * @param segment The segment selector. Should always be 0x08.
 * @param base The interrupt handler
 *
 * @returns 0. Anything else, and you're in for a REALLY bad day. 
 */
int hal_registerInterruptVector(uint8_t index, uint8_t flags, uint16_t segment, uint64_t base) {
    hal_idt_table[index].base_lo = (base & 0xFFFF);
    hal_idt_table[index].base_mid = (base >> 16) & 0xFFFF;
    hal_idt_table[index].base_hi = (base >> 32) & 0xFFFFFFFF;
    hal_idt_table[index].selector = segment;
    hal_idt_table[index].flags = flags;
    hal_idt_table[index].reserved = 0;
    hal_idt_table[index].ist = 0;

    return 0;
}


/**
 * @brief Handle ending an interrupt
 */
inline void hal_endInterrupt(uintptr_t interrupt_number) {
    pic_eoi(interrupt_number);
}

/**
 * @brief Common exception handler
 */
void hal_exceptionHandler(registers_t *regs, extended_registers_t *regs_extended) {
    uintptr_t exception_index = regs->int_no;

    // Call the exception handler
    if (hal_exception_handler_table[exception_index] != NULL) {
        exception_handler_t handler = (hal_exception_handler_table[exception_index]);
        int return_value = handler(exception_index, regs, regs_extended);

        if (return_value != 0) {
            kernel_panic(IRQ_HANDLER_FAILED, "hal");
            __builtin_unreachable();
        }

        // Now we're finished so return
        if (current_cpu->current_process && current_cpu->current_thread) {
            signal_handle(current_cpu->current_thread, regs);
        }

        return;
    }

    // NMIs are fired as of now only for a core shutdown. If we receive one, just halt.
    if (exception_index == 2) {
        smp_acknowledgeCoreShutdown();
        for (;;);
    }

    // Was this in userspace?
    if (regs->cs == 0x2B) {
        dprintf(ERR, "A userspace exception was detected.\n");
        dprintf(ERR, "Thread %p (%d) on process \'%s\' (%d)\n", current_cpu->current_thread, current_cpu->current_thread->tid, current_cpu->current_process->name, current_cpu->current_process->pid);
        
        if (exception_index < X86_64_MAX_EXCEPTIONS) {
            dprintf(ERR, "Exception: %s (%i)\n", hal_exception_table[exception_index], exception_index);
        } else {
            dprintf(ERR, "Exception: ??? (%d)\n", exception_index);
        }

        dprintf(ERR, "RAX %016llX RBX %016llX RCX %016llX RDX %016llX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        dprintf(ERR, "RDI %016llX RSI %016llX RBP %016llX RSP %016llX\n", regs->rdi, regs->rsi, regs->rbp, regs->rsp);
        dprintf(ERR, "R8  %016llX R9  %016llX R10 %016llX R11 %016llX\n", regs->r8, regs->r9, regs->r10, regs->r11);
        dprintf(ERR, "R12 %016llX R13 %016llX R14 %016llX R15 %016llX\n", regs->r12, regs->r13, regs->r14, regs->r15);
        dprintf(ERR, "ERR %016llX RIP %016llX RFL %016llX\n", regs->err_code, regs->rip, regs->rflags);
        dprintf(ERR, "CS %04X DS %04X SS %04X\n", regs->cs, regs->ds, regs->ss);
        dprintf(ERR, "CR0 %08X CR2 %016llX CR3 %016llX\n", regs_extended->cr0, regs_extended->cr2, regs_extended->cr3);
        dprintf(ERR, "Killing process.\n");

        process_exit(current_cpu->current_process, 1);
        return;
    }
    
    // Looks like no one caught this exception.
    kernel_panic_prepare(CPU_EXCEPTION_UNHANDLED);

    if (exception_index == 14) {
        // Page fault, get the address
        uintptr_t page_fault_addr = 0x0;
        asm volatile ("movq %%cr2, %0" : "=a"(page_fault_addr));

        // Print it out
        dprintf(NOHEADER, "*** ISR detected exception: Page fault at address 0x%016llX\n\n", page_fault_addr);
        printf("*** Page fault at address 0x%016llX detected in kernel.\n", page_fault_addr);
    } else if (exception_index < X86_64_MAX_EXCEPTIONS) {
        // Other exception
        dprintf(NOHEADER, "*** ISR detected exception %i - %s\n\n", exception_index, hal_exception_table[exception_index]);
        printf("*** ISR detected exception %i - %s\n", exception_index, hal_exception_table[exception_index]);
    } else {
        dprintf(NOHEADER, "*** ISR detected exception %i - UNKNOWN TYPE\n\n", exception_index);
        printf("*** ISR detected unknown exception: %i\n", exception_index);
    }
    
    

    dprintf(NOHEADER, "\033[1;31mFAULT REGISTERS:\n\033[0;31m");

    dprintf(NOHEADER, "RAX %016llX RBX %016llX RCX %016llX RDX %016llX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    dprintf(NOHEADER, "RDI %016llX RSI %016llX RBP %016llX RSP %016llX\n", regs->rdi, regs->rsi, regs->rbp, regs->rsp);
    dprintf(NOHEADER, "R8  %016llX R9  %016llX R10 %016llX R11 %016llX\n", regs->r8, regs->r9, regs->r10, regs->r11);
    dprintf(NOHEADER, "R12 %016llX R13 %016llX R14 %016llX R15 %016llX\n", regs->r12, regs->r13, regs->r14, regs->r15);
    dprintf(NOHEADER, "ERR %016llX RIP %016llX RFL %016llX\n\n", regs->err_code, regs->rip, regs->rflags);

    dprintf(NOHEADER, "CS %04X DS %04X SS %04X\n\n", regs->cs, regs->ds, regs->ss);
    dprintf(NOHEADER, "CR0 %08X CR2 %016llX CR3 %016llX\n", regs_extended->cr0, regs_extended->cr2, regs_extended->cr3);
    dprintf(NOHEADER, "GDTR %016llX %04X\n", regs_extended->gdtr.base, regs_extended->gdtr.limit);
    dprintf(NOHEADER, "IDTR %016llX %04X\n", regs_extended->idtr.base, regs_extended->idtr.limit);

    // !!!: not conforming (should call kernel_panic_finalize) but whatever
    // We want to do our own traceback.
    arch_panic_traceback(10, regs);

    // Show core processes
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "\nCPU DATA:\n" COLOR_CODE_RED);

    for (int i = 0; i < MAX_CPUS; i++) {
        if (processor_data[i].cpu_id || !i) {
            // We have valid data here
            if (processor_data[i].current_thread != NULL) {
                dprintf(NOHEADER, COLOR_CODE_RED "CPU%d: Executing thread %d (%p) (process '%s', PID %d)\n", i, processor_data[i].current_thread->tid, processor_data[i].current_thread, processor_data[i].current_process->name, processor_data[i].current_process->pid);
            } else {
                dprintf(NOHEADER, COLOR_CODE_RED "CPU%d: No thread available. Page directory %p\n", processor_data[i].current_context->dir);
            }
        }
    }

    // Display message
    dprintf(NOHEADER, COLOR_CODE_RED "\nThe kernel will now permanently halt. Connect a debugger for more information.\n");

    // Disable interrupts & halt
    asm volatile ("cli\nhlt");
    for (;;);

    // kernel_panic_finalize();

    for (;;);
}


/**
 * @brief System call handler
 */
void hal_syscallHandler(registers_t *regs, extended_registers_t *regs_extended) {
    syscall_t syscall;
    syscall.syscall_number = regs->rax;
    syscall.parameters[0] = regs->rdi;
    syscall.parameters[1] = regs->rsi;
    syscall.parameters[2] = regs->rdx;
    syscall.parameters[3] = regs->r10;
    syscall.parameters[4] = regs->r8;
    syscall.parameters[5] = regs->r9;

    // HACK
    current_cpu->current_thread->regs = regs;
    current_cpu->current_thread->syscall = &syscall;

    // Handle syscall
    syscall_handle(&syscall);
    regs->rax = syscall.return_value;
    syscall_finish();

    // Handle any signals
    signal_handle(current_cpu->current_thread, regs);
    current_cpu->current_thread->syscall = NULL;
}

/**
 * @brief Common interrupt handler
 */
void hal_interruptHandler(registers_t *regs, extended_registers_t *regs_extended) {
    uintptr_t exception_index = regs->int_no;
    uintptr_t int_number = regs->int_no - 32;

    // TODO: Move this to IRQ system
    if (exception_index == ARCH_SYSCALL_NUMBER) {
        // Handle system call
        syscall_t syscall;
        syscall.syscall_number = regs->rax;
        syscall.parameters[0] = regs->rdi;
        syscall.parameters[1] = regs->rsi;
        syscall.parameters[2] = regs->rdx;
        syscall.parameters[3] = regs->r10;
        syscall.parameters[4] = regs->r8;
        syscall.parameters[5] = regs->r9;

        // HACK
        current_cpu->current_thread->regs = regs;
        current_cpu->current_thread->syscall = &syscall;

        // End the interrupt now
        hal_endInterrupt(int_number);

        // Handle system call
        syscall_handle(&syscall);
        regs->rax = syscall.return_value;
        syscall_finish();

        signal_handle(current_cpu->current_thread, regs);
        current_cpu->current_thread->syscall = NULL;
        return;
    }

    // Call IRQ handler
    irq_handler(regs->int_no, regs);

    // TODO move to IRQ handler probably
    if (current_cpu->current_process && current_cpu->current_thread) {
        signal_handle(current_cpu->current_thread, regs);
            
        if (current_cpu->current_thread->status & THREAD_STATUS_STOPPING) {
            thread_exit();
        }
    }
}

/**
 * @brief Register an exception handler
 * @param int_no Exception number
 * @param handler A handler. This should return 0 on success, anything else panics.
 *                It will take an exception number, registers, and extended registers as arguments.
 * @returns 0 on success, -EINVAL if handler is taken
 */
int hal_registerExceptionHandler(uintptr_t int_no, exception_handler_t handler) {
    if (hal_exception_handler_table[int_no] != NULL) {
        return -EINVAL;
    }

    hal_exception_handler_table[int_no] = handler;

    return 0;
}

/**
 * @brief Unregisters an exception handler
 */
void hal_unregisterExceptionHandler(uintptr_t int_no) {
    hal_exception_handler_table[int_no] = NULL;
}

/**
 * @brief Set interrupt state on the current CPU
 * @param state The interrupt state to set
 * @returns Last state
 */
int hal_setInterruptState(int state) {
    uintptr_t flags;
    asm volatile ("pushf\n"
                    "pop %0" : "=rm"(flags));
                    
    if (state == HAL_INTERRUPTS_ENABLED) {
        asm volatile ("sti");
    } else {
        asm volatile ("cli");
    }

    return (flags & (1 << 9)) ? HAL_INTERRUPTS_ENABLED : HAL_INTERRUPTS_DISABLED;
}

/**
 * @brief Get the interrupt state on the current CPU
 */
int hal_getInterruptState() {
    // Get flags
    uintptr_t flags;
    asm volatile ("pushf\n"
                    "pop %0" : "=rm"(flags));

    return (flags & (1 << 9)) ? HAL_INTERRUPTS_ENABLED : HAL_INTERRUPTS_DISABLED;
}

/**
 * @brief Installs the IDT in the current AP
 */
void hal_installIDT() {
    // Install the IDT
    x86_64_idtr_t idtr; 
    idtr.base = (uintptr_t)hal_idt_table;
    idtr.limit = sizeof(hal_idt_table) - 1;
    asm volatile("lidt %0" :: "m"(idtr));
}

/**
 * @brief Debug trap handler
 * @param exception_index Always 1
 * @param regs Registers at time of fault
 * @param extended_regs Extended registers at time of trap
 */
int hal_debugTrapHandler(uintptr_t exception_index, registers_t *regs, extended_registers_t *extended_regs) {
    if (regs->cs != 0x08) {
        // Must be from usermode
        current_cpu->current_thread->regs = regs;
        if (current_cpu->current_process->ptrace.tracer) {
            ptrace_event(PROCESS_TRACE_SINGLE_STEP);
        }
        return 0; // TODO: Send SIGTRAP
    }

    // Uh, kernel debug trap?
    kernel_panic(KERNEL_DEBUG_TRAP, "hal");
    return 0;
}

/**
 * @brief Initialize exception handlers (early)
 */
void hal_initializeExceptionHandlers() {
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    hal_gdtInit();

    // Clear the IDT table
    memset((void*)hal_idt_table, 0x00, sizeof(hal_idt_table));

    // Install the exception handlers
    hal_registerInterruptVector(0, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDivisionException);
    hal_registerInterruptVector(1, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDebugException);
    hal_registerInterruptVector(2, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halNMIException);
    hal_registerInterruptVector(3, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halBreakpointException);
    hal_registerInterruptVector(4, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halOverflowException);
    hal_registerInterruptVector(5, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halBoundException);
    hal_registerInterruptVector(6, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halInvalidOpcodeException);
    hal_registerInterruptVector(7, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halNoFPUException);
    hal_registerInterruptVector(8, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDoubleFaultException);
    hal_registerInterruptVector(9, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halCoprocessorSegmentException);
    hal_registerInterruptVector(10, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halInvalidTSSException);
    hal_registerInterruptVector(11, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSegmentNotPresentException);
    hal_registerInterruptVector(12, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halStackSegmentException);
    hal_registerInterruptVector(13, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halGeneralProtectionException);
    hal_registerInterruptVector(14, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halPageFaultException);
    hal_registerInterruptVector(15, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halReservedException);
    hal_registerInterruptVector(16, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halFloatingPointException);
    hal_registerInterruptVector(17, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halAlignmentCheck);
    hal_registerInterruptVector(18, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halMachineCheck);
    hal_registerInterruptVector(19, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSIMDFloatingPointException);
    hal_registerInterruptVector(20, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halVirtualizationException);
    hal_registerInterruptVector(21, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halControlProtectionException);
    hal_registerInterruptVector(28, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halHypervisorInjectionException);
    hal_registerInterruptVector(29, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halVMMCommunicationException);
    hal_registerInterruptVector(30, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSecurityException);
    hal_registerInterruptVector(31, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halReserved2Exception);

    hal_installIDT();
}

/**
 * @brief Initializes the PIC, GDT/IDT, TSS, etc.
 */
void hal_initializeInterrupts() {
    // Interrupts must be disabled
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    // Clear the IDT table
    memset((void*)hal_idt_table, 0x00, sizeof(hal_idt_table));

    // Install the exception handlers
    hal_registerInterruptVector(0, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDivisionException);
    hal_registerInterruptVector(1, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDebugException);
    hal_registerInterruptVector(2, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halNMIException);
    hal_registerInterruptVector(3, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halBreakpointException);
    hal_registerInterruptVector(4, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halOverflowException);
    hal_registerInterruptVector(5, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halBoundException);
    hal_registerInterruptVector(6, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halInvalidOpcodeException);
    hal_registerInterruptVector(7, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halNoFPUException);
    hal_registerInterruptVector(8, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halDoubleFaultException);
    hal_registerInterruptVector(9, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halCoprocessorSegmentException);
    hal_registerInterruptVector(10, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halInvalidTSSException);
    hal_registerInterruptVector(11, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSegmentNotPresentException);
    hal_registerInterruptVector(12, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halStackSegmentException);
    hal_registerInterruptVector(13, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halGeneralProtectionException);
    hal_registerInterruptVector(14, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halPageFaultException);
    hal_registerInterruptVector(15, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halReservedException);
    hal_registerInterruptVector(16, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halFloatingPointException);
    hal_registerInterruptVector(17, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halAlignmentCheck);
    hal_registerInterruptVector(18, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halMachineCheck);
    hal_registerInterruptVector(19, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSIMDFloatingPointException);
    hal_registerInterruptVector(20, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halVirtualizationException);
    hal_registerInterruptVector(21, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halControlProtectionException);
    hal_registerInterruptVector(28, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halHypervisorInjectionException);
    hal_registerInterruptVector(29, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halVMMCommunicationException);
    hal_registerInterruptVector(30, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halSecurityException);
    hal_registerInterruptVector(31, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, (uint64_t)&halReserved2Exception);

    // Register interrupt vectors
extern uintptr_t halIsrHandlerTable, halIsrHandlerTableEnd;
    size_t isr_entries = ((uintptr_t)&halIsrHandlerTableEnd - (uintptr_t)&halIsrHandlerTable) / sizeof(void*);
    uintptr_t *handler_table = &halIsrHandlerTable;
    for (unsigned i = 0; i < isr_entries; i++) {
        hal_registerInterruptVector(i+32, X86_64_IDT_DESC_PRESENT | X86_64_IDT_DESC_BIT32, 0x08, handler_table[i]);
    }

    // Reserve the exception vectors
    for (int i = 0; i < 32; i++) {
        irq_reserveVector(i);
    }

    // Install IDT in BSP
    hal_installIDT();

    // Initialize 8259 PICs
    pic_init(PIC_TYPE_8259, NULL);
    
    // Register debug interrupt handler
    hal_registerExceptionHandler(1, hal_debugTrapHandler);

    // Leave interrupts disabled for now. They can be re-enabled on this AP after irq_initCPU
    // This is fine because TLB shootdowns wont occur until all SMP processors have initialized which only happens after irq_initCPU
}
