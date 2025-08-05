/**
 * @file hexahedron/drivers/x86/acpica_osl.c
 * @brief ACPICA interface for Hexahedron
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * ACPICA and its components are licensed under BSD license, the same as Hexahedron.
 * ACPICA is created by Intel corporation.
 * Licensing of ACPICA is available in hexahedron/drivers/x86/acpica/LICENSE
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifdef ACPICA_ENABLED

#include <acpica/acpi.h>
#include <acpica/actypes.h>


#include <kernel/arch/arch.h>

// Kernel includes
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/spinlock.h>
#include <kernel/drivers/clock.h>
#include <kernel/misc/semaphore.h>
#include <kernel/drivers/pci.h>

#include <stdarg.h>
#include <errno.h>


#define FUNC_UNIMPLEMENTED(FUNCTION)    \
    LOG(WARN, "%s: Unimplemented\n", FUNCTION);    \
    kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "ACPICA", "*** %s not implemented\n", FUNCTION);   \
    __builtin_unreachable();

#define LOG(status, message, ...) dprintf_module(status, "ACPICA:OSL", message, ## __VA_ARGS__)


/* INITIALIZE/TERMINATE FUNCTIONS */

ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate() {
    return AE_OK;
}

/* Get the RSDP */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    // Try to ask HAL (loading from EFI provides RSDP in ST)
    ACPI_PHYSICAL_ADDRESS rsdp = (ACPI_PHYSICAL_ADDRESS)hal_getRSDP();

    if (rsdp == 0x0) {
        ACPI_STATUS status = AcpiFindRootPointer(&rsdp); // Use ACPICA to find the pointer.
        if (ACPI_FAILURE(status)) return 0x0;
    }

    return rsdp;
}

/* OVERRIDE FUNCTIONS */

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) {
    *NewValue = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
    *NewTable = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength) {
    *NewAddress = 0x0;
    return AE_OK;
}

/* MEMORY FUNCTIONS */

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
    // LOG(DEBUG, "AcpiOsMapMemory %p 0x%x\n", (uintptr_t)PhysicalAddress, Length);
    return (void*)mem_remapPhys(PhysicalAddress, Length);

}

/* Unmap memory */
void AcpiOsUnmapMemory(void *where, ACPI_SIZE Length) {
    // LOG(DEBUG, "AcpiOsUnmapMemory 0x%x 0x%x\n", where, Length);

    // !!!: I have genuinely no idea why, but when unmapping memory ACPICA freaks out and tries to allocate a ton of chunks, which the pool system catches and panics.
    // mem_unmapPhys((uintptr_t)where, Length);

}

/* Get a physical address */
ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress) {
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)mem_getPhysicalAddress(NULL, (uintptr_t)LogicalAddress);
    return AE_OK;
}

/* Allocate */
void *AcpiOsAllocate(ACPI_SIZE Size) {
    void *ptr = kmalloc(Size);
    return ptr;
}

/* Free */
void AcpiOsFree(void *Memory) {
    return kfree(Memory);
}

/* Are the memory regions readable? */
BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) {
    FUNC_UNIMPLEMENTED("AcpiOsReadable");
}

/* Are the memory regions writable? */
BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length) {
    FUNC_UNIMPLEMENTED("AcpiOsWritable");
}

/* UNIMPLEMENTED THREAD FUNCTIONS (SINGLE-THREADED) */

ACPI_THREAD_ID AcpiOsGetThreadId() {
    return 1; // Constant thread. According to ACPICA docs this should work.
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
    return AE_OK;
}


void AcpiOsSleep(UINT64 Milliseconds) {
    return clock_sleep( Milliseconds );
}

void AcpiOsStall(UINT32 Microseconds) {
    FUNC_UNIMPLEMENTED("AcpiOsStall");
}

void AcpiOsWaitEventsComplete() {
    LOG(WARN, "Unimplemented AcpiOsWaitEventsComplete - not critical\n");
}

/* SEMAPHORE FUNCTIONS */

/* Create a semaphore */
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) {
    *OutHandle = semaphore_create("acpica_sem", InitialUnits, MaxUnits);
    return AE_OK;
}

/* Delete semaphore */
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
    semaphore_destroy((semaphore_t*)Handle);
    return AE_OK;
}

/* Wait on semaphore */
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
    if (Timeout == 0) {
        // Don't wait at all? Just check if there are enough units
        if ((UINT32)semaphore_getItems(Handle) >= Units) {
            semaphore_wait(Handle, Units);
            return AE_OK;
        }

        return AE_TIME;
    }

    time_t start_time = now(); // Start time in ms
    UINT32 remaining = Units;
    while (remaining > 0 && now() - start_time < Timeout) {
        remaining -= semaphore_wait(Handle, remaining);
    }

    if (remaining > 0) {
        // Timeout exceeded
        semaphore_signal(Handle, Units - remaining);
        return AE_TIME; 
    }

    return AE_OK;
}

/* Signal the semaphore */
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    int added = semaphore_signal(Handle, Units);
    if ((UINT32)added != Units) return AE_LIMIT;
    return AE_OK;
}

/* LOCK FUNCTIONS */

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    // this we can actually do
    spinlock_t *lock = spinlock_create("acpi_lock");

    // TODO: NOT DISABLING INTERRUPTS 
    *OutHandle = lock;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
    kfree(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
    spinlock_acquire((spinlock_t*)Handle);
    return (ACPI_CPU_FLAGS)NULL;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
    spinlock_release((spinlock_t*)Handle);
}

/* INTERRUPT FUNCTIONS */
/* Hexahedron uses a different IRQ handler style, so we have to use a handler */

#define MAX_ACPI_INTERRUPT_HANDLERS 16

// Store data & handlers
ACPI_OSD_HANDLER ACPI_interruptHandlers[MAX_ACPI_INTERRUPT_HANDLERS];
void *ACPI_interruptContext[MAX_ACPI_INTERRUPT_HANDLERS];

int ACPICA_InterruptHandler(uintptr_t exception_number, uintptr_t int_number, registers_t *registers, extended_registers_t *extended) {
    if (ACPI_interruptHandlers[int_number]) {
        ACPI_interruptHandlers[int_number](ACPI_interruptContext[int_number]);
    }

    return 0;
}



ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context) {
    // LOG(INFO, "AcpiOsInstallInterruptHandler 0x%x 0x%x 0x%x\n", InterruptLevel, Handler, Context);

    // Install the data
    if (InterruptLevel > MAX_ACPI_INTERRUPT_HANDLERS || Handler == NULL) {
        return AE_BAD_PARAMETER;
    }

    if (ACPI_interruptHandlers[InterruptLevel]) {
        return AE_ALREADY_EXISTS;
    }

    ACPI_interruptHandlers[InterruptLevel] = Handler;
    ACPI_interruptContext[InterruptLevel] = Context;

    
    int r = hal_registerInterruptHandlerRegs(InterruptLevel, ACPICA_InterruptHandler);
    if (r != 0) {
        // This seems like a kernel fault
        LOG(ERR, "hal_registerInterruptHandler(%i, 0x%x) returned %i\n", InterruptLevel, ACPICA_InterruptHandler, r);
        return AE_ERROR;
    }
    return AE_OK;
}
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler) {
    if (InterruptLevel >= MAX_ACPI_INTERRUPT_HANDLERS || Handler == NULL ) {
        return AE_BAD_PARAMETER;
    }

    if (ACPI_interruptHandlers[InterruptLevel] != Handler) {
        // Doesn't exist or wrong one
        return AE_NOT_EXIST;
    }

    ACPI_interruptHandlers[InterruptLevel] = NULL;
    ACPI_interruptContext[InterruptLevel] = NULL;
    hal_unregisterInterruptHandler(InterruptLevel);
    
    return AE_OK;
}

/* LOGGING */

/* 
 * NOTE: Most logs come in with a header (e.g. "Firmware Error (ACPI)") that is in AcpiOsPrintf,
 * and then error content is put into AcpiOsVprintf
 */
void AcpiOsPrintf(const char *Format, ...) {
    va_list ap;
    va_start(ap, Format);
    dprintf_va("ACPICA", NOHEADER, (char*)Format, ap);
    va_end(ap);
}

void AcpiOsVprintf(const char *Format, va_list Args) {
    dprintf_va("ACPICA", NOHEADER, (char*)Format, Args);
}

/* MORE MEMORY */

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    LOG(DEBUG, "AcpiOsReadMemory 0x%llX 0x%x\n", Address, Width);

    void *ptr = (void*)mem_remapPhys((uintptr_t)Address, 0x1000); // !!!: HORRIBLY INEFFICIENT!!!!!!!!!

    switch (Width) {
        case 8:
            *Value = *(UINT8*)ptr; 
            break;
        case 16:
            *Value = *(UINT16*)ptr; 
            break;
        case 32:
            *Value = *(UINT32*)ptr; 
            break;
        case 64:
            *Value = *(UINT64*)ptr; 
            break;
        default:
            kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "ACPICA", "*** AcpiOsReadMemory received bad width argument 0x%x\n", Width);
    }

    mem_unmapPhys((uintptr_t)ptr, 0x1000);
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    void *ptr = (void*)mem_remapPhys((uintptr_t)Address, 0x1000); // !!!: HORRIBLY INEFFICIENT!!!!!!!!!

    switch (Width) {
        case 8:
            *(UINT8*)ptr = Value;
            break;
        case 16:
            *(UINT16*)ptr = Value;
            break;
        case 32:
            *(UINT32*)ptr = Value;
            break;
        case 64:
            *(UINT64*)ptr = Value;
            break;
        default:
            kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "ACPICA", "*** AcpiOsReadMemory received bad width argument 0x%x\n", Width);
    }

    mem_unmapPhys((uintptr_t)ptr, 0x1000);
    return AE_OK;
}


/* I/O FUNCTIONS */

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
    switch (Width) {
        case 8:
            *Value = inportb(Address);
            break;
        case 16:
            *Value = inportw(Address);
            break;
        case 32:
            *Value = inportl(Address);
            break;
        default:
            kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "ACPICA", "*** AcpiOsReadPort received bad width argument 0x%x\n", Width);
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    switch (Width) {
        case 8:
            outportb(Address, Value);
            break;
        case 16:
            outportw(Address, Value);
            break;
        case 32:
            outportl(Address, Value);
            break;
        default:
            kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "ACPICA", "*** AcpiOsReadPort received bad width argument 0x%x\n", Width);
    }

    return AE_OK;
}

/* PCI FUNCTIONS */

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 *Value, UINT32 Width) {
    // LOG(DEBUG, "AcpiOsReadPciConfiguration Bus %04x Slot %04x Func %04x Offset %04x Register 0x%x Width 0x%x\n", PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, Register, Width);
    uint32_t value = pci_readConfigOffset(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment + Register, Width / 8);
    
    if (value == PCI_NONE) {
        return AE_ERROR;
    }

    *Value = (UINT64)value; 
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 Value, UINT32 Width) {
    // LOG(DEBUG, "AcpiOsWritePciConfiguration Bus %04x Slot %04x Func %04x Offset %04x Register 0x%x Width 0x%x Value 0x%llX\n", PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, Register, Width, Value);

    if (pci_writeConfigOffset(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment + Register, (UINT32)(Value & 0xFFFFFFFF), Width)) {
        return AE_ERROR;
    }

    return AE_OK;
}

/* MISC. */

UINT64 AcpiOsGetTimer() {
    return now() * 10 * 1000;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) {
    /* It might send other stuff but we need to catch fatal signals */
    switch (Function) {
        case ACPI_SIGNAL_FATAL: ;
            ACPI_SIGNAL_FATAL_INFO *error_info = Info;
            kernel_panic_extended(ACPI_SYSTEM_ERROR, "ACPICA", "*** ACPI AML error: Fatal error detected. Type: 0x%x Code: 0x%x Argument: 0x%x", 
                    error_info->Type,
                    error_info->Code,
                    error_info->Argument);
            break;
        default:
            LOG(DEBUG, "ACPI AML signal 0x%x received\n", Function);
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep (UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    LOG(INFO, "Preparing to enter sleep state S%d\n", SleepState);
    return AE_OK;
}

#endif
