/**
 * @file hexahedron/arch/x86_64/boot.S
 * @brief Entrypoint for the x86_64 bootloader
 * 
 * Compatible with both Multiboot2 and Multiboot standards
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.extern __bss_start
.extern __kernel_start
.extern __kernel_end

.set KERNEL_LOAD_ADDRESS, 0xFFFFF00000000000

.section .multiboot.data, "aw"

/* ===== MULTIBOOT 1 ===== */

// Declare flags
.set ALIGN, 1 << 0
.set MEMORY_INFO, 1 << 1
.set FRAMEBUFFER, 1 << 2

// Declare the actual multiboot flags, magic, and checksum
.set MULTIBOOT_FLAGS, ALIGN | MEMORY_INFO | FRAMEBUFFER
.set MULTIBOOT_MAGIC, 0x1BADB002
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

// Declare the header
.align 4
multiboot_header:
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.long multiboot_header
.long (__kernel_start - KERNEL_LOAD_ADDRESS)
.long (__bss_start - KERNEL_LOAD_ADDRESS)
.long (__kernel_end - KERNEL_LOAD_ADDRESS)
.long _start

// Request framebuffer (1024x768x32)
.long 0x00000000
.long 1920
.long 1080
.long 32

/* ==== MULTIBOOT 2 ==== */
.set MULTIBOOT2_MAGIC, 0xE85250D6
.set MULITBOOT2_ARCHITECTURE, 0 // 0 means 32-bit protected mode of i386
.set MULITBOOT2_HEADER_LENGTH, (multiboot2_header_end - multiboot2_header_start)
.set MULITBOOT2_CHECKSUM, -(MULTIBOOT2_MAGIC + MULITBOOT2_ARCHITECTURE + MULITBOOT2_HEADER_LENGTH)

.align 8
multiboot2_header_start:

// Like MB1, put the magic, arch, length, etc.
.long MULTIBOOT2_MAGIC
.long MULITBOOT2_ARCHITECTURE
.long MULITBOOT2_HEADER_LENGTH
.long MULITBOOT2_CHECKSUM

// Documentation: https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html

/* Give a tag describing the entrypoint */
.align 8
multiboot2_entry_tag:
.word 3                         // Type (u16 - entry)
.word 0                         // Flags (u16 - unused)
.long 12                        // Size of the tag
.long _start

/* Then, give a tag requesting a framebuffer */
.align 8
multiboot2_fb_tag:
.word 5                         // Type (u16 - fb)
.word 0                         // Flags (u16 - unused)
.long 20                        // Size of the tag
.long 1920                      // 1024 width
.long 1080                       // 768 height
.long 32                        // 32 bpp (depth)

/* Make our lives easier and make module page aligned */
.align 8
mulitboot2_align_mods:
.word 6                         // Type (u16 - align)
.word 0                         // Flags (u16 - unused)
.long 8                         // Size of the tag

// NOTE:    A relocatable image tag is not used, I'm not keeping track of EIP
//          through all of this bootstrapping AND adjusting ELF addresses in C code.

/* End tag */
.align 8
multiboot2_end_tag:
.word 0
.word 0
.long 8

multiboot2_header_end:


/* ==== GDT ==== */

.align 8

// GDT pointer
gdtr:
    .word gdt_end - gdt_base
    .quad gdt_base


// GDT data
gdt_base:
    /* Null segment */
    .long 0
    .long 0

    /* Code segment - 64-bit kernel mode */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0x20
    .byte 0x00

    /* Data segment - 64-bit kernel mode */
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0x00
    .byte 0x00
gdt_end:


/* ==== GENERIC ==== */

// Reserve a stack in BSS
.section .bss
.align 16
__stack_bottom:
.skip 65536 // Provide 16KiB of stack
.global __stack_top
__stack_top:

/* ==== BOOTSTRAP ==== */
.section .multiboot.text, "a"
.code32

.extern mem_kernelPML

.global _start
.type _start, @function
_start:
    // Setup stack 
    movl $(__stack_top - KERNEL_LOAD_ADDRESS), %esp

    // Check that stack is 16-byte aligned
    and $-16, %esp

    // Clear EBP for backtrace
    xor %ebp, %ebp

    // Save all multiboot variables onto the stack
    // Anything pushed should be popped off the stack later, so in 64-bit
    // realm we can just pop these back off.
    // IMPORTANT: Remember that these are 64-bit pointers, so we have to do 2 pushes per register.
    pushl $0
    pushl %esp  // Stack
    pushl $0
    pushl %eax  // Multiboot magic
    pushl $0
    pushl %ebx  // Multiboot header ptr

    // Skip check on long mode, rather just crash.

.paging_init:
    // We need to now setup our paging structures.
    // This should've already been zeroed out by the loader as part of the ELF file.
    movl $(mem_kernelPML - KERNEL_LOAD_ADDRESS), %edi

    // Set entry #0 of PML4 to point to PDPT[0] with present, writable, and user
    mov $0x1007, %eax
    add %edi, %eax
    mov %eax, (%edi)

    // Set entry #whatever (0xFFFFF00000000000) to point to PDPT[0] with present, writable, user
    // The kernel will clean this up later.
    push %edi
    mov $0x1007, %eax
    add %edi, %eax
    add $0xF00, %edi
    mov %eax, (%edi)
    mov $(mem_kernelPML - KERNEL_LOAD_ADDRESS), %edi
    pop %edi

    // Set entry #0 of PDPT to point to PD[0] with present, writable, and user.
    add $0x1000, %edi
    mov $0x1003, %eax
    add %edi, %eax
    mov %eax, (%edi)

    // Setup 32 2MiB pages to map 64MiB of low memory temporarily (source: ToaruOS).
    // !!!: We don't check if this is supported, and the memory subsystem was not designed with 2MiB (as of Dec 22nd)
    // 0x0000000000000000-0x0000000003ffffff -> 0x000000000000-0x000003ffffff
    add $0x1000, %edi       // Move to next entry
    mov $0x87, %ebx         // Present, writable, usermode, and page size
    mov $512, %ecx           // Loop amount (400 4KiB pages)

.paging_set_entry_loop:
    // Loop through each entry 
    mov %ebx, (%edi)        // Write the entry
    add $0x200000, %ebx     // Add 2MiB
    add $8, %edi            // Next entry
    loop .paging_set_entry_loop

.paging_enable:
    // Set CR3 to point to our PML4
    movl $(mem_kernelPML - KERNEL_LOAD_ADDRESS), %edi
    movl %edi, %cr3

    // Enable PAE, it is required for long mode.
    mov %cr4, %eax
    or $32, %eax
    mov %eax, %cr4

    // Enable EFER, enabling SYSCALL/SYSRET
    movl $0xC0000080, %ecx
    rdmsr
    or $256, %eax
    wrmsr

    // Check PG - bootloader may already have enabled paging
    movl %cr0, %eax
    test $0x80000000, %eax
    // ljmp $0x08, $(.longmode_move_to_kernel - KERNEL_LOAD_ADDRESS)   // bruh

    // Otherwise, enable paging.
    or $0x80000000, %eax
    mov %eax, %cr0

    // Load the GDTR
    lgdt gdtr

    // Leap of faith!
    ljmp $0x08, $(longmode_entry - KERNEL_LOAD_ADDRESS)

.type boot_error, @function
boot_error:
    // Because we requested a Multiboot framebuffer,
    // we can't show errors! Hopefully the bootloader informed the user.

    // Instead, do the next best thing: kill the computer!
    ljmp $0xFFFF, $0x0 // Reset vector

    // Uhh, that didn't work.
    cli
    hlt
    jmp .


/* ==== BOOTSTRAP - 64-bit ==== */

.section .text
.code64
.align 8

.extern arch_main
.type arch_main, @function

// Main longmode entrypoint - setup segments
longmode_entry:
    cli
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

.longmode_jump:
    // Indirect jump!!
    movabsq $longmode_actual, %rax
    jmp *%rax

longmode_actual:
    // If we jump here, then we don't have to setup segments.
    cli

    // Pop into function pointer arguments
    pop %rdi
    pop %rsi
    pop %rdx

    // Adjust stack pointer
    movq $KERNEL_LOAD_ADDRESS, %rax
    addq %rax, %rsp

    call arch_main

.longmode_halt:
    cli
    hlt
    jmp .