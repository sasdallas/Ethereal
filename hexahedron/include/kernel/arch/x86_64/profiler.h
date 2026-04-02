/**
 * @file hexahedron/include/kernel/arch/x86_64/profiler.h
 * @brief Kernel profiler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_PROFILER_H
#define KERNEL_ARCH_X86_64_PROFILER_H

/**** DEFINITIONS ****/
#define AMD_PERFEVTSEL_USR      (1 << 16)
#define AMD_PERFEVTSEL_OS       (1 << 17)
#define AMD_PERFEVTSEL_INT      (1 << 20)
#define AMD_PERFEVTSEL_EN       (1 << 22)

/**** FUNCTIONS ****/

/**
 * @brief Initialize architecture profiler
 */
void arch_profiler_init();

#endif