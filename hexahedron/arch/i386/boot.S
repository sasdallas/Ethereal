/**
 * @file hexahedron/arch/i386/boot.S
 * @brief Entrypoint for the I386 bootloader
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


.section .multiboot

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
.long __kernel_start
.long __bss_start
.long __kernel_end
.long _start

// Request framebuffer (1024x768x32)
.long 0x00000000
.long 1024
.long 768
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
// Start with an address tag
.align 8
multiboot2_address_tag:
.word 2                         // Type (u16 - address)
.word 0                         // Flags (u16 - unused)
.long 24                        // Size of the tag
.long multiboot2_header_start   // Put the start of the header
.long __kernel_start            // Starting address of the kernel
.long __bss_start               // BSS starting address
.long __kernel_end              // Ending address of the kernel

/* Next, give a tag describing the entrypoint */
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
.long 1024                      // 1024 width
.long 768                       // 768 height
.long 32                        // 32 bpp (depth)

/* Give a tag requesting EGA text support */
.align 8
multiboot2_console_flags_tag:
.word 4                         // Type (u16 - flags)
.word 0                         // Flags (u16 - unused)
.long 12                        // Size of the tag
.long 2                         // Bit 1 means we have EGA text support

/* Make our lives easier and make module page aligned */
.align 8
mulitboot2_align_mods:
.word 6                         // Type (u16 - align)
.word 0                         // Flags (u16 - unused)
.long 8                         // Size of the tag

/* Relocatable header tag */
.align 8
mulitboot_relocatable_tag:
.word 10                        // Type (u16 - relocatable)
.word 0                         // Flags (u16 - unused)
.long 24                        // Size of the tag
.long 0x100000                  // We can accept a range of 0x100000 to 0x200000 (TODO: refine this, just randomly thought of these)
.long 0x200000
.long 4096
.long 1                         // Load image at the lowest possible address


multiboot2_header_end:


/* ==== GENERIC ==== */

// Reserve a stack
.section .bss
.global __stack_top
.align 16
__stack_bottom:
.skip 16384 // Provide 16 KiB of stack
__stack_top:

// The main entrypoint
.section .text
.global _start

.extern arch_main
.extern _init

.type _start, @function
_start:
    // Setup stack
    movl $__stack_top, %esp

    // Call global constructors
    // ??? why no works
    // call _init

    // Clear EBP for backtrace
    xor %ebp, %ebp

    // Call architecture main
    push %esp // Stack
    push %eax // Multiboot magic
    push %ebx // Multiboot header ptr
    call arch_main

    // This shouldn't return. If it does, halt
    cli
    hlt
    jmp .