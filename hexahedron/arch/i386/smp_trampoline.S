/**
 * @file hexahedron/arch/i386/smp_trampoline.S
 * @brief AP initial startup file
 * 
 * This will get the APs ready for 32-bit protected mode and prepare any data needed.
 * NOTE: This file is based off of predetermined addresses and load sections.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.global _ap_parameters
.global _ap_startup
.global _ap_gdtr
.global _ap_bootstrap_start
.global _ap_bootstrap_end
.extern smp_finalizeAP
.extern _ap_stack_base
.section .ap_bootstrap

.code16

_ap_bootstrap_start:

_ap_startup:    /* 0x1000 */
    cli
    cld
    ljmp $0, $0x1040

.align 16
_ap_gdt_table:  /* 0x1010 */
    .long 0, 0

    /* Code segment - 32-bit kernel mode (0x08) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0xCF
    .byte 0x00
 
    /* Data segment - 32-bit kernel mode (0x10) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xCF
    .byte 0x00

    .long 0, 0

_ap_gdtr:       /* 0x1030 */
    .word _ap_gdtr - _ap_gdt_table - 1
    .long 0x1010
    .long 0, 0

.align 64
_ap_startup_real:   /* 0x1040 */
    xorw %ax, %ax
    movw %ax, %ds

    /* Load GDT */
    lgdtl 0x1030

    /* Prepare to switch to protected mode */
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0

    /* Jump to protected mode! */
    ljmp $0x08, $0x1060

.align 32
.code32


_ap_startup_pmode:  /* 0x1060 */
    /* Setup segments */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    /* We need a temporary stack first */
    movl $0x1000 + _ap_stack_temp - _ap_bootstrap_start, %esp

    /* We're good enough to jump to smp_finalizeAP */
    ljmp $0x08, $smp_finalizeAP

    cli
    hlt
    jmp .


/* This is a temporary stack for setting up IDT/paging */
.skip 16
_ap_stack_temp:

_ap_bootstrap_end: