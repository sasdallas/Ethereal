/**
 * @file hexahedron/kernel/panic/panic_messages.c
 * @brief Kernel panic messages 
 * 
 * Contains a list of panic messages
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/panic.h>


int __kernel_stop_codes = KERNEL_STOP_CODES; // Used to keep this as the only needed-recomp file

char *kernel_bugcode_strings[KERNEL_STOP_CODES] = {
    "KERNEL_DEBUG_TRAP",
    "MEMORY_MANAGEMENT_ERROR",
    "KERNEL_BAD_ARGUMENT_ERROR",
    "OUT_OF_MEMORY",
    "IRQ_HANDLER_FAILED",
    "CPU_EXCEPTION_UNHANDLED",
    "UNSUPPORTED_FUNCTION_ERROR",
    "ACPI_SYSTEM_ERROR",
    "ASSERTION_FAILED",
    "INSUFFICIENT_HARDWARE_REQUIREMENTS", // This one is different, looks better.
    "INITIAL_RAMDISK_CORRUPTED",
    "DRIVER_LOADER_ERROR",
    "DRIVER_LOAD_FAILED",
    "TASK_SCHEDULER_ERROR",
    "CRITICAL_PROCESS_DIED",
    "UNKNOWN_CORRUPTION_DETECTED",
    "UBSAN_TYPE_MISMATCH",
    "UBSAN_SHIFT_OUT_OF_BOUNDS",
    "UBSAN_POINTER_OVERFLOW",
    "STACK_SMASHING_DETECTED"
};

char *kernel_panic_messages[KERNEL_STOP_CODES] = {
    "A trap was triggered to debug the kernel, but no debugger was connected.\n",
    "A fault has occurred in the memory management subsystem during a call.\n",
    "A bad argument was passed to a critical function. This is (unless specified) a bug in the kernel - please contact the developers.\n",
    "The system has run out of memory. Try closing applications or adjusting your pagefile.\n",
    "An IRQ handler did not return a success value. This could be caused by an external driver or an internal kernel driver.\n",
    "A CPU exception in the kernel was not handled correctly.\n",
    "An unsupported kernel function was called. This as a bug in the kernel - please contact the developers.\n",
    "Your computer is not compliant with ACPI specifications, or is not compatible with the ACPICA library.\n",
    "An assertion within the kernel failed.\n",
    "Your computer does not meet the requirements necessary to run Hexahedron.\n",
    "The initial startup disk (initrd.tar.img) was not found or was corrupted.\n",
    "The driver loader encountered a malformatted/invalid driver entry.\n",
    "A critical driver failed to load correctly.\n",
    "The task scheduler encountered an error.\n",
    "A process critical to the system has died and could not be respawned.\n",
    "A kernel data structure was corrupted in a way that makes continuing impossible.\n",
    "Undefined behavior sanitizer detected a type match.\n",
    "Undefined behavior sanitizer detected a shift out of bounds.\n",
    "Undefined behavior sanitizer detected a pointer overflow.\n",
    "Kernel stack smashing detected.\n",
};