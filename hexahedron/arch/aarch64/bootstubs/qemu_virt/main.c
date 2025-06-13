/**
 * @file hexahedron/arch/aarch64/bootstubs/qemu_virt/main.c
 * @brief QEMU Virt machine bootstub
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdint.h>
#include <kernel/arch/aarch64/mem.h>

/**** DEFINITIONS ****/
#define QEMU_VIRT_UART_ADDRESS      (volatile uint8_t*)0x09000000
#define QEMU_VIRT_LOAD_BASE         (uintptr_t)0x40000000 // DTB is also positioned here
#define QEMU_VIRT_BOOTSTUB_BASE     (uintptr_t)0x41000000
#define QEMU_VIRT_PHYSMEM_REGION    (uintptr_t)0xffffff8000000000

/* SCTLR */
#define SCTLR_M                     (1 << 0)    // MMU enable
#define SCTLR_C                     (1 << 2)    // Cache
#define SCTLR_ITD                   (1 << 7)    // IT disable
#define SCTLR_I                     (1 << 12)   // Instruction cacheability
#define SCTLR_SPAN                  (1 << 23)   // Set Privileged Access Never
#define SCTLR_NTLSMD                (1 << 28)   // No Trap Load Multiple and Store Multiple
#define SCTLR_LSMAOE                (1 << 29)   // Load Multiple and Store Multiple Atomicity and Ordering Enable

/* TCR */
#define TCR_T0SZ_48_BIT             16          // Size offset of memory region in TTBR0_EL1 (48-bit)
#define TCR_IRGN0                   (1 << 8)    // Normal memory, inner WT, RA, no WA, C
#define TCR_ORGN0                   (1 << 10)   // Normal memory, outer WB, RA, WA, C
#define TCR_SH0_INNER_SHAREABLE     (3 << 12)   // Inner shareable
#define TCR_TG0_4KB                 0           // 4KB granule size
#define TCR_T1SZ_48_BIT             (16 << 16)  // Size offset of memory region in TTBR1_EL1 (48-bit)
#define TCR_IRGN1                   (1 << 24)   // Normal memory, inner WT, RA, no WA, C
#define TCR_ORGN1                   (1 << 26)   // Normal memory, outer WB, RA, WA, C
#define TCR_SH1_INNER_SHAREABLE     (3 << 28)   // Inner shareable
#define TCR_TG1_4KB                 (2 << 30)   // 4KB granule
#define TCR_IPS_4TB                 (3UL << 32) // 4TB intermediate physical address size

/**** CODE ****/

/* MMU tables */
page_t mmu_pml[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_high_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_low_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_kernel_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };

int use_physmap = 0;

/**
 * @brief Print method to the QEMU_VIRT address
 */
int terminal_print(void *user, char c) {
    if (!use_physmap) *QEMU_VIRT_UART_ADDRESS = c;
    else *(volatile uint8_t*)(QEMU_VIRT_PHYSMEM_REGION | (uintptr_t)QEMU_VIRT_UART_ADDRESS) = c;
    return 0;
}

/**
 * @brief Debug printf method
 */
void dprintf(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    xvasprintf(terminal_print, NULL, fmt, ap);
    va_end(ap);
}

/**
 * @brief Initialize the MMU, build default kernel tables and set them in TTBR
 * @returns 0 on success
 */
int bootstub_initializeMMU() {
    // Initial PML mappings of the kernel 
    dprintf("bootstub: mmu: Map bootstub at %016llX\n", QEMU_VIRT_LOAD_BASE);
    dprintf("bootstub: mmu: Make physical memory mappings at %016llX\n", QEMU_VIRT_PHYSMEM_REGION);

    // Create low and high memory mappings
    mmu_pml[0].data = (uintptr_t)&mmu_low_gbs;
    mmu_pml[0].bits.present = 1;
    mmu_pml[0].bits.table = 1;
    mmu_pml[0].bits.af = 1;

    mmu_pml[511].data = (uintptr_t)&mmu_high_gbs;
    mmu_pml[511].bits.present = 1;
    mmu_pml[511].bits.table = 1;
    mmu_pml[511].bits.af = 1;

    // Make a mapping for us, the bootstub
    mmu_low_gbs[1].data = QEMU_VIRT_LOAD_BASE;
    mmu_low_gbs[1].bits.present = 1;
    mmu_low_gbs[1].bits.af = 1;
    mmu_low_gbs[1].bits.sh = 2;
    mmu_low_gbs[1].bits.indx = 1;

    // Physical memory mapping
    for (size_t i = 0; i < 64; i++) {
        mmu_high_gbs[i].data = (i << 30);
        mmu_high_gbs[i].bits.present = 1;
        mmu_high_gbs[i].bits.af = 1;
        mmu_high_gbs[i].bits.sh = 2;
        mmu_high_gbs[i].bits.indx = 1;
    }


    // SCTLR
	uint64_t sctlr = SCTLR_M | SCTLR_C | SCTLR_ITD | SCTLR_I | SCTLR_SPAN | SCTLR_NTLSMD | SCTLR_LSMAOE;

    // TCR
	uint64_t tcr = TCR_TG0_4KB | TCR_TG1_4KB |
                    TCR_IRGN0 | TCR_IRGN1 | TCR_ORGN0 | TCR_ORGN1 |
                    TCR_SH0_INNER_SHAREABLE | TCR_SH1_INNER_SHAREABLE |
                    TCR_T0SZ_48_BIT | TCR_T1SZ_48_BIT |
                    TCR_IPS_4TB;

	// MAIR setup
	uint64_t mair  = (0x000000000044ff00);
	asm volatile ("msr MAIR_EL1,%0" :: "r"(mair));

	// Configure TTBR0 and TTBR1
	dprintf("bootstub: mmu: PML @ %016llX\n", &mmu_pml);
	asm volatile ("msr TTBR0_EL1,%0" : : "r"(&mmu_pml));
	asm volatile ("msr TTBR1_EL1,%0" : : "r"(&mmu_pml));
	asm volatile ("msr TCR_EL1,%0" : : "r"(tcr));
	
    // Sync
	asm volatile ("dsb ishst\n"
                    "tlbi vmalle1is\n"
                    "dsb ish\n"
                    "isb\n" ::: "memory");
    
    // Set SCTLR       
    dprintf("bootstub: mmu: Enabling MMU\n");
	asm volatile ("msr SCTLR_EL1, %0" :: "r"(sctlr));
	asm volatile ("isb" ::: "memory");

    use_physmap = 1;
    dprintf("bootstub: mmu: MMU enabled successfully\n");

    return 0;
}

/**
 * @brief Main function of bootstub
 */
void bootstub_main() {
    dprintf("================= BOOTSTUB RUNNING =================\n");
    dprintf("bootstub: Compiled on %s %s for the QEMU Virt machine\n", __DATE__, __TIME__);

    dprintf("bootstub: Initializing MMU\n");
    if (bootstub_initializeMMU()) {
        dprintf("bootstub: MMU init failed");
        return;
    }


}