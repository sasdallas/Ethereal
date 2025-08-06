/**
 * @file hexahedron/kernel/config.c
 * @brief Configuration options for kernel.
 * 
 * Edit this file to change things like:
 * - Debugging port settings
 * - Version information
 * etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/config.h>


// This is part of vsnprintf() style formatting. TODO: Maybe not?
// Example: 1.0.0-i386-DEBUG
const char *__kernel_version_format = "%d.%d.%d-%s-%s";

// This file is deleted and rebuilt every build to update these.
const char *__kernel_build_date = __DATE__;
const char *__kernel_build_time = __TIME__;

// Versioning information
const int __kernel_version_major = 1;
const int __kernel_version_minor = 8;
const int __kernel_version_lower = 0;

// Codename
const char *__kernel_version_codename = "Neutron";

// ASCII art (looks weird because fmting)
const char *__kernel_ascii_art_formatted = "\
    .+------+\n \
 .' |    .'|\n \
+---+--+'  |\tH E X A H E D R O N   K E R N E L\n \
|   |  |   |\tCreated by @sasdallas\n \
|  .+--+---+\n \
|.'    | .'\n \
+------+'\n";



/**** DEBUG SETTINGS ****/

#define DEBUG_COM_PORT 1        // This is the default COM port that Hexahedron will initialize and provide debug logging.
                                // WARNING: It is very much not recommended to use any port besides COM1.

#define DEBUG_BAUD_RATE 9600    // This is the default baud rate Hexahedron will set

const int __debug_output_com_port = DEBUG_COM_PORT;
const int __debug_output_baud_rate = DEBUG_BAUD_RATE;

#define DEBUGGER_COM_PORT 2     // This is the default COM port that Hexahedron will (try to) initialize and provide debug packets on
#define DEBUGGER_BAUD_RATE 9600 

const int __debugger_com_port = DEBUGGER_COM_PORT;
const int __debugger_baud_rate = DEBUGGER_BAUD_RATE;
const int __debugger_enabled = 1;       // The debugger may annoy you because it requires a connection.
const int __debugger_wait_time = 5000;  // Wait time (in ms) for the debugger to wait on a packet


/**** AUTO-GENERATED VERSIONING INFO ****/


// Configuration
#if defined(__KERNEL_DEBUG__)
const char *__kernel_build_configuration = "DEBUG";
#elif defined(__KERNEL_RELEASE__)
const char *__kernel_build_configuration = "RELEASE";
#else
const char *__kernel_build_configuration = "UNKNOWN";
#endif

// Architecture information (TODO: Can't get the __ARCH__ variable working)
#if defined(__ARCH_I386__)
const char *__kernel_architecture = "i386";
#elif defined(__ARCH_X86_64__)
const char *__kernel_architecture = "x86_64";
#elif defined(__ARCH_AARCH64__)
const char *__kernel_architecture = "aarch64";
#else
const char *__kernel_architecture = "unknownarch";
#endif

// Compiler information
#if (defined(__GNUC__) || defined(__GNUG__)) && !(defined(__clang__) || defined(__INTEL_COMPILER))
#define KERNEL_COMPILER_VERSION "GCC " __VERSION__;
#else
#define KERNEL_COMPILER_VERSION "Unknown Compiler"
#endif

const char *__kernel_compiler = KERNEL_COMPILER_VERSION;