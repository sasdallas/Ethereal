/**
 * @file hexahedron/kernel/panic.c
 * @brief Kernel panic handler (generic)
 * 
 * Hexahedron uses two panic systems:
 * - Generic errors can be handled using bugcodes, such as MEMORY_MANAGEMENT_ERROR
 * - Nongeneric errors can call kernel_panic_prepare() and kernel_panic_finalize() to handle PRINT OUT the error in their own way
 *      * IMPORTANT: If you need to handle the error in your own way, use arch_panic_prepare() and arch_panic_finalize()
 * 
 * These stop codes are supported by a string lookup table, that @todo should be moved to a separate file.
 * (ReactOS uses .mc files, dunno about Linux - that's for localization however)
 * 
 * This file exposes 2 functions
 * - kernel_panic(BUGCODE, MODULE)
 * - kernel_panic_extended(BUGCODE, MODULE, FORMAT, ...)
 * 
 * kernel_panic() will take in a bugcode and a module (e.g. "vfs" or "mem") and use a generic string table to
 * print basic information
 * 
 * kernel_panic_extended() will take in a bugcode and a module, but will also take in va_args().
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/panic.h>
#include <kernel/debug.h>
#include <kernel/arch/arch.h>
#include <kernel/debugger.h>
#include <kernel/mem/alloc.h>

#include <stdint.h>
#include <stdio.h>


/* Basic debug messages */
#define DEBUG_MESSAGES \
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD   "\n\nFATAL: Kernel panic detected!\n"); \
    dprintf(NOHEADER, COLOR_CODE_RED        "Hexahedron has experienced a critical fault that cannot be resolved\n"); \
    dprintf(NOHEADER, COLOR_CODE_RED        "Please start an issue on GitHub if you believe this to be a bug.\n"); \
    dprintf(NOHEADER, COLOR_CODE_RED        "Apologies for any inconveniences caused by this error.\n\n"); \

/* Normal messages printed to console */
#define CONSOLE_MESSAGES \
    printf(COLOR_CODE_RED   "FATAL: Kernel panic detected. Hexahedron needs to shutdown.\n"); \
    printf(COLOR_CODE_RED   "Please start an issue on GitHub if you believe this to be a bug.\n"); \
    printf(COLOR_CODE_RED   "Apologies for any inconveniences caused by this error.\n\n")

/* Are we currently in a panic state? */
int kernel_in_panic_state = 0;

/**
 * @brief xvas_callback
 */
static int kernel_panic_putchar(void *user, char ch) {
    dprintf(NOHEADER, "%c", ch);
    printf("%c", ch);
    return 0;
}

/**
 * @brief Send panic packet to debugger
 */
void kernel_panic_sendPacket(uint32_t bugcode, char *module, char *additional) {
    if (!debugger_isConnected()) return;

    json_value *data = json_object_new(3);
    if (bugcode) json_object_push(data, "bugcode", json_integer_new(bugcode));
    if (module) json_object_push(data, "module", json_string_new(module));
    if (additional) json_object_push(data, "additional", json_string_new(additional));

    debugger_sendPacket(PACKET_TYPE_PANIC, data);
}

/**
 * @brief Immediately panic and stop the kernel.
 * 
 * @param bugcode Takes in a kernel bugcode, e.g. KERNEL_DEBUG_TRAP
 * @param module The module the fault occurred in, e.g. "vfs"
 * @param format String to print out (this is va_args, you may use formatting)
 */
void kernel_panic_extended(uint32_t bugcode, char *module, char *format, ...) {
    if ((int)bugcode >= __kernel_stop_codes) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, module, "*** kernel_panic_extended() received an invalid bugcode (0x%x)\n", bugcode);
        // Doesn't return
    }

    // Make sure we aren't already in a panic state
    if (kernel_in_panic_state) {
        dprintf(NOHEADER, COLOR_CODE_RED_BOLD "*** FULL STOP: Kernel attempted to panic while already in panic state (%s).\n", kernel_bugcode_strings[bugcode]);
        printf("*** Kernel encountered another fatal error while in panic state (this is likely a bug).\n");
        for (;;); // arch_panic_finalize could have some extra unknown stuff that is crashing
    }

    kernel_in_panic_state = 1;

    // Prepare for the panic
    arch_panic_prepare();

    // Start by printing out debug messages
    DEBUG_MESSAGES
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD   "*** STOP: cpu%i: %s (module \'%s\')\n", arch_current_cpu(), kernel_bugcode_strings[bugcode], module);

    // Normal messages
    CONSOLE_MESSAGES;
    printf(COLOR_CODE_RED   "*** STOP: cpu%i: %s (module \"%s\")\n", arch_current_cpu(), kernel_bugcode_strings[bugcode], module);
    
    // Print out anything additional
    char additional[512];
    va_list ap;
    va_start(ap, format);
    xvasprintf(kernel_panic_putchar, NULL, format, ap);
    vsnprintf(additional, 512, format, ap);
    va_end(ap);

   
    // Notify debugger
    kernel_panic_sendPacket(bugcode, module, additional);

    // Print out a generic message
    dprintf(NOHEADER, COLOR_CODE_RED        "\nAdditional information: %s", kernel_panic_messages[bugcode]);
    printf(COLOR_CODE_RED                   "\nAdditional information: %s", kernel_panic_messages[bugcode]);



    // Finish the panic
    if (debugger_isConnected()) BREAKPOINT();
    arch_panic_finalize();
}

/**
 * @brief Immediately panic and stop the kernel.
 * @param bugcode Takes in a kernel bugcode, e.g. KERNEL_DEBUG_TRAP
 * @param module The module the fault occurred in, e.g. "vfs"
 */
void kernel_panic(uint32_t bugcode, char *module) {
    if ((int)bugcode >= __kernel_stop_codes) {
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, module, "*** kernel_panic() received an invalid bugcode (0x%x)\n", bugcode);
        // Doesn't return
    }

    // Make sure we aren't already in a panic state
    if (kernel_in_panic_state) {
        dprintf(NOHEADER, COLOR_CODE_RED_BOLD "*** FULL STOP: Kernel attempted to panic while already in panic state (%s).\n", kernel_bugcode_strings[bugcode]);
        printf("*** Kernel encountered another fatal error while in panic state (this is likely a bug).\n");
        for (;;); // arch_panic_finalize could have some extra unknown stuff that is crashing
    }

    kernel_in_panic_state = 1;
    
    // Prepare for the panic
    arch_panic_prepare();

    // Start by printing out debug messages
    DEBUG_MESSAGES;
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD   "*** STOP: cpu%i: %s (module \'%s\')\n", arch_current_cpu(), kernel_bugcode_strings[bugcode], module);
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD   "*** %s" COLOR_CODE_RED, kernel_panic_messages[bugcode]);

    // Normal messages
    CONSOLE_MESSAGES;
    printf(COLOR_CODE_RED   "*** STOP: cpu%i: %s (module \"%s\")\n", arch_current_cpu(), kernel_bugcode_strings[bugcode], module);
    printf(COLOR_CODE_RED   "*** %s\n", kernel_panic_messages[bugcode]);

    // Send debugger packet to say we panicked
    kernel_panic_sendPacket(bugcode, module, NULL);

    // Finish the panic
    if (debugger_isConnected()) BREAKPOINT();
    arch_panic_finalize();
}

/**
 * @brief Prepare the system to enter a panic state
 * 
 * @param bugcode Optional bugcode to display. Leave as NULL to not use (note: generic string will not be printed)
 */
void kernel_panic_prepare(uint32_t bugcode) {
    // Make sure we aren't already in a panic state
    if (kernel_in_panic_state) {
        dprintf(NOHEADER, COLOR_CODE_RED_BOLD "*** FULL STOP: Kernel attempted to panic while already in panic state.\n");
        printf("*** Kernel encountered another fatal error while in panic state (this is likely a bug).\n");
        for (;;); // arch_panic_finalize could have some extra unknown stuff that is crashing
    }

    kernel_in_panic_state = 1;

    // Do arch-specific panic preparation
    arch_panic_prepare();

    // Start out by printing debug messages
    DEBUG_MESSAGES;
    
    // Normal messages
    CONSOLE_MESSAGES;

    // Bugcode
    if (bugcode) {
        dprintf(NOHEADER, COLOR_CODE_RED_BOLD   "*** STOP: cpu%i: %s\n", arch_current_cpu(), kernel_bugcode_strings[bugcode]);
        printf(COLOR_CODE_RED   "*** STOP: cpu%i: %s\n", arch_current_cpu(), kernel_bugcode_strings[bugcode]);
    }

    kernel_panic_sendPacket(bugcode, NULL, NULL);
}

/**
 * @brief Finalize the panic state
 */
void kernel_panic_finalize() {
    if (debugger_isConnected()) BREAKPOINT();
    arch_panic_finalize();
}