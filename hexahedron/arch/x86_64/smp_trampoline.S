/**
 * @file hexahedron/arch/x86_64/smp_trampoline.S
 * @brief AP initial startup file
 * 
 * This will get the APs ready for 64-bit long mode and prepare any data needed.
 * NOTE: This file is based off of predetermined addresses and load sections.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
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
.extern mem_kernelPML
.section .ap_bootstrap

.code16

_ap_bootstrap_start:

_ap_startup:    /* 0x1000 */
    cli
    cld
    ljmp $0, $0x1040

.align 16
_ap_gdt_table:  /* 0x1010 */
    /* Null segment */
    .long 0
    .long 0

    /* Code segment - 64-bit kernel mode (0x08) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0x20
    .byte 0x00

    /* Data segment - 64-bit kernel mode (0x10) */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0x00
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

    /* Enable PAE and PGE */
    mov $0xA0, %eax
    mov %eax, %cr4

    /* Load the base PML */
    mov $(mem_kernelPML - 0xFFFFF00000000000), %eax
    mov %eax, %cr3

    /* Set LME bit */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    /* Enable long mode */
    mov $0x80000011, %ebx
    mov %ebx, %cr0

    addr32 lgdtl %cs:0x1030
    data32 ljmp $0x08, $0x1000 + _ap_startup_long - _ap_bootstrap_start



/* This is a temporary stack for setting up IDT/paging */
.skip 16
_ap_stack_temp:


.code64

_ap_startup_long_jump:
    callq smp_finalizeAP

    cli
    hlt


.align 16
_ap_startup_long:
    /* Made it to long mode, setup segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* We need a temporary stack first */
    movl $0x1000 + _ap_stack_temp - _ap_bootstrap_start, %esp

    // Indirect jump!!
    movabsq $_ap_startup_long_jump, %rax
    jmp *%rax


_ap_bootstrap_end: