/**
 * @file hexahedron/arch/aarch64/smp.c
 * @brief SMP
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/arch/aarch64/smp.h>
#include <kernel/arch/aarch64/arch.h>
#include <kernel/processor_data.h>

int processor_count = 0;
processor_t processor_data[MAX_CPUS];
