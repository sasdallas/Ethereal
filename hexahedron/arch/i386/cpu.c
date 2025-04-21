/**
 * @file hexahedron/arch/i386/cpu.c
 * @brief I386 CPU interfacer
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/i386/cpu.h>
#include <kernel/panic.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Checks whether the RDMSR/WRMSR instructions are supported
 */
int cpu_msrAvailable() {
    static uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return edx & CPUID_FEAT_EDX_MSR;
}

/**
 * @brief Get a model-specific register
 * @param msr The MSR to get
 * @param lo The low bits (pointer output)
 * @param hi The high bits (pointer output)
 */
void cpu_getMSR(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    if (!cpu_msrAvailable()) return;
    asm volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

/**
 * @brief Set a model-specific register
 * @param msr The MSR to set
 * @param lo The low bits to set with
 * @param hi The high bits to set with
 */
void cpu_setMSR(uint32_t msr, uint32_t lo, uint32_t hi) {
    if (!cpu_msrAvailable()) return;
    asm volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

/**
 * @brief Perform a CPUID instruction (only for strings)
 * 
 * Yes, I know of the existance of @c __cpuid but it doesn't fit my needs for model strings.
 */
static inline void cpuid(int code, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    asm volatile ("cpuid": 
                    "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) 
                    : "a"(code));
}

/**
 * @brief Get the vendor name of a CPU, cleaned up
 */
char *cpu_getVendorName() {
    char vendor[17];
    uint32_t unused;
    cpuid(0, &unused, (uint32_t*)&vendor, (uint32_t*)&(vendor[8]), (uint32_t*)&(vendor[4]));
    vendor[16] = 0;

    if (!strncmp(vendor, CPUID_VENDOR_AMD, 16)) {
        return "AMD";
    } else if (!strncmp(vendor, CPUID_VENDOR_INTEL, 16)) {
        return "Intel";
    }

    return "???";
}

/**
 * @brief Get the model number of a CPU
 */
uint8_t cpu_getModelNumber() {
    uint32_t eax, unused;
    __cpuid(0, eax, unused, unused, unused);
    return (eax >> 4) & 0x0F;
}

/**
 * @brief Get the family of a CPU
 */
uint8_t cpu_getFamily() {
    uint32_t eax, unused;
    __cpuid(0, eax, unused, unused, unused);
    return (eax >> 8) & 0x0F;
}

/**
 * @brief Get the CPU brand string
 */
char *cpu_getBrandString() {
    char brand[48];

    snprintf(brand, 10, "Unknown");

    uint32_t eax, unused;
    __cpuid(CPUID_INTELEXTENDED, eax, unused, unused, unused);
    if (eax >= CPUID_INTELBRANDSTRINGEND) {
        // Supported!
        uint32_t brand_data[12];
		__cpuid(0x80000002, brand_data[0], brand_data[1], brand_data[2], brand_data[3]);
		__cpuid(0x80000003, brand_data[4], brand_data[5], brand_data[6], brand_data[7]);
		__cpuid(0x80000004, brand_data[8], brand_data[9], brand_data[10], brand_data[11]);
        memcpy(brand, brand_data, 48);
    }

    // !!!: this is ugly
    return strdup(brand);
}

/**
 * @brief Check whether an FPU is present via CPUID
 */
int cpu_hasFPU() {
    uint32_t edx, unused;
    __cpuid(CPUID_GETFEATURES, unused, unused, unused, edx);
    return edx & CPUID_FEAT_EDX_FPU;
}

/**
 * @brief Check whether the CPU supports SSE
 */
int cpu_hasSSE() {
    uint32_t edx, unused;
    __cpuid(CPUID_GETFEATURES, unused, unused, unused, edx);
    return edx & CPUID_FEAT_EDX_SSE;
}

/**
 * @brief Check whether the CPU supports SSE2
 */
int cpu_hasSSE2() {
    uint32_t edx, unused;
    __cpuid(CPUID_GETFEATURES, unused, unused, unused, edx);
    return edx & CPUID_FEAT_EDX_SSE2;
}

/**
 * @brief Initialize the FPU for the CPU, as well as SSE
 */
void cpu_fpuInitialize() {
    // An FPU is REQUIRED for Hexahedron, as is SSE.
    if (!cpu_hasSSE() && !cpu_hasFPU()) goto _no_fpu;

    // First, enable SSE
    asm volatile(
        "mov %%cr0, %%eax\n"
        "and $0xFFFB, %%ax\n"   // Clear CR0.EM 
        "or $0x2, %%ax\n"       // Set CR0.MP
        "mov %%eax, %%cr0\n"
        "mov %%cr4, %%eax\n"
        "or $0x600, %%ax\n"     // Set CR4.OSFXSR and CR4.OSXMMEXCPT
        "mov %%eax, %%cr4\n"   
    ::: "eax");

    // Valid, we'll load MXCSR now.
    uint32_t mxcsr = 0x1f80;
    asm volatile ("ldmxcsr %0" :: "m"(mxcsr));

    // Now turn on the FPu.
    // Clear the TS and EM bits in CR0
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2);
    cr0 &= ~(1 << 3);

    unsigned short test_word = 0xFFFF;
    asm volatile (
        "mov %1, %%cr0\n"
        "fninit\n"
        "fnstsw %0" : "=r"(test_word) : "r"(cr0)
    );

    if (test_word == 0x0) {
        // Valid, load FPU word
        uint32_t fpuword = 0x37A;
        asm volatile ("fldcw %0" :: "m"(fpuword));
    } else {
        // Invalid
        goto _no_fpu;
    }


    return;

_no_fpu:
    kernel_panic_extended(INSUFFICIENT_HARDWARE_ERROR, "cpu", "*** Hexahedron requires a floating-point unit and SSE support to operate.\n");
    __builtin_unreachable();
}