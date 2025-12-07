/**
 * @file hexahedron/include/kernel/panic.h
 * @brief Kernel panic header
 * 
 * Defines all stopcodes & panic functions
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H


/**** INCLUDES ****/
#include <stdint.h>

/**** STOP CODES ****/

#define     KERNEL_STOP_CODES               22

#define     KERNEL_DEBUG_TRAP               0x00000000 // Debugging trap
#define     MEMORY_MANAGEMENT_ERROR         0x00000001 // Memory management failure
#define     KERNEL_BAD_ARGUMENT_ERROR       0x00000002 // Critical function was passed a bad argument
#define     OUT_OF_MEMORY                   0x00000003 // Out of memory in the kernel (note that some mem_ functions take over panicking instead of this)
#define     IRQ_HANDLER_FAILED              0x00000004 // An IRQ handler did not succeed
#define     CPU_EXCEPTION_UNHANDLED         0x00000005 // A CPU exception was not handled
#define     UNSUPPORTED_FUNCTION_ERROR      0x00000006 // A function that was not supported was called
#define     ACPI_SYSTEM_ERROR               0x00000007 // An ACPI error by the HW occurred
#define     ASSERTION_FAILED                0x00000008 // An assertion by assert() failed
#define     INSUFFICIENT_HARDWARE_ERROR     0x00000009 // The system does not meet the requirements for Hexahedron.
#define     INITIAL_RAMDISK_CORRUPTED       0x0000000A // The initial ramdisk was corrupted or missing
#define     DRIVER_LOADER_ERROR             0x0000000B // The driver loader encountered an error
#define     DRIVER_LOAD_FAILED              0x0000000C // A critical driver failed to load
#define     SCHEDULER_ERROR                 0x0000000D // The task scheduler encountered an error
#define     CRITICAL_PROCESS_DIED           0x0000000E // A process critical to system execution has died unexpectedly and could not be respawned
#define     UNKNOWN_CORRUPTION_DETECTED     0x0000000F // A kernel data structure was corrupted in a way that makes continuing impossible
#define     UBSAN_TYPE_MISMATCH             0x00000010 // UBsan type mismatch
#define     UBSAN_SHIFT_OUT_OF_BOUNDS       0x00000011 // UBsan shift out of bounds
#define     UBSAN_POINTER_OVERFLOW          0x00000012 // UBsan pointer overflow
#define     STACK_SMASHING_DETECTED         0x00000013 // Kernel detected stack smash
#define     MISSING_INIT_DEPENDENCY         0x00000014 // Initcall dependency missing
#define     INIT_ROUTINE_FAILURE            0x00000015 // Critical init routine failed

/**** MESSAGES ****/

extern char *kernel_panic_messages[KERNEL_STOP_CODES];
extern char *kernel_bugcode_strings[KERNEL_STOP_CODES];

extern int __kernel_stop_codes;

/**** FUNCTIONS ****/

/**
 * @brief Immediately panic and stop the kernel.
 * @param bugcode Takes in a kernel bugcode, e.g. KERNEL_DEBUG_TRAP
 * @param module The module the fault occurred in, e.g. "vfs"
 */
void kernel_panic(uint32_t bugcode, char *module);

/**
 * @brief Immediately panic and stop the kernel.
 * 
 * @param bugcode Takes in a kernel bugcode, e.g. KERNEL_DEBUG_TRAP
 * @param module The module the fault occurred in, e.g. "vfs"
 * @param format String to print out (this is va_args, you may use formatting)
 */
void kernel_panic_extended(uint32_t bugcode, char *module, char *format, ...);

/**
 * @brief Prepare the system to enter a panic state
 * 
 * @param bugcode Optional bugcode to display. Leave as NULL to not use (note: generic string will not be printed)
 */
void kernel_panic_prepare(uint32_t bugcode);

/**
 * @brief Finalize the panic state
 */
void kernel_panic_finalize();



#endif