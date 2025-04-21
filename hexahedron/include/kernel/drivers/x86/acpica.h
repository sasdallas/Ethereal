/**
 * @file hexahedron/include/kernel/drivers/x86/acpica.h
 * @brief ACPICA kernel interface header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifdef ACPICA_ENABLED

#ifndef DRIVERS_X86_ACPICA_H
#define DRIVERS_X86_ACPICA_H

/**** INCLUDES ****/
#include <acpica/acpi.h>
#include <stdint.h>

/* Architecture-specific includes */
#if defined(__ARCH_I386__)
#include <kernel/arch/i386/smp.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/smp.h>
#else
#error "Unsupported architecture - do not compile this file"
#endif


/**** FUNCTIONS ****/

/**
 * @brief Initialize ACPICA
 */
int ACPICA_Initialize();

/**
 * @brief Get SMP information from MADT
 * @returns A pointer to the SMP information object, or NULL if there was an error.
 */
smp_info_t *ACPICA_GetSMPInfo();

/**
 * @brief Print the ACPICA namespace to serial (for debug)
 */
void ACPICA_PrintNamespace();

#endif

#endif