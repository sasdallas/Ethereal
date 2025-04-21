/**
 * @file hexahedron/arch/x86_64/cpu.c
 * @brief x86_64 CPU interface
 * 
 * @note This file probably needs a cleanup.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/arch/x86_64/cpu.h>
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
 * @brief x86_64: Check if 5-level paging is supported
 */
int cpu_pml5supported() {
    // TODO: Add more checks - there's an Intel whitepaper on this that has about 5 more checks.

    // CPUID with EAX=07 will check this
    uint32_t ecx, unused;
    __cpuid(0x07, unused, unused, ecx, unused);
    return ecx & CPUID_FEAT_ECX_PML5;
}

/**
 * @brief x86_64: Get the maximum linear-address width supported by the CPU
 * 
 * CPUID check of 0x80000008
 */
uint32_t cpu_getMaxLinearAddress() {
    // CPUID with EAX=0x80000008 will check this
    uint32_t unused;
    CPUID_INTELADDRSIZE_EAX eax;
    __cpuid(CPUID_INTELADDRSIZE, eax, unused, unused, unused);
    return eax.bits.linear_address_bits;
}

/**
 * @brief Initialize the CPU floating point unit
 * 
 * This feels weirdly out of place being in cpu.c, but who cares.
 * Assembly code is sourced from ToaruOS (as with everything lol)
 */
void cpu_fpuInitialize() {
    // NOTE: x86_64 demands SSE, meaning we don't need to check.
    // If there's no FPU (literally not possible) then this will just crash, and that's probably better.

    // Not sure why this works. At this point I don't care 
    asm volatile (
		"clts\n"                // CLTS will clear CR0.TS bit 
		"mov %%cr0, %%rax\n"    
		"and $0xFFFD, %%ax\n"   // Clear CR0.MP
		"or $0x10, %%ax\n"      // Set CR0.NE
		"mov %%rax, %%cr0\n"
		"fninit\n"              // Initialize FPU
		
        // SSE initialization
        "mov %%cr0, %%rax\n"    
		"and $0xfffb, %%ax\n"   // Clear CR0.EM
		"or  $0x0002, %%ax\n"   // Set CR0.MP
		"mov %%rax, %%cr0\n"
		"mov %%cr4, %%rax\n"
		"or $0x600, %%rax\n"    // Set CR4.OSFXSR and CR4.OSXMMEXCPT
		"mov %%rax, %%cr4\n"   

        // Load MXCSR (SSE)
		"push $0x1F80\n"        // 0x1F80 = precision, underflow, overflow, div0, denormla, inv. operation
		"ldmxcsr (%%rsp)\n"
		"addq $8, %%rsp\n"
	: : : "rax");
    
    // TODO: Do we need to load an FPU control word like as is done on I386?

    return;
}