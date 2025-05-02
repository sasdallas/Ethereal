/**
 * @file libpolyhedron/arch/i386/setjmp/setjmp.S
 * @brief setjmp and longjmp
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.global setjmp
.type setjmp, @function
setjmp:
    movl 4(%esp), %eax          // Load the pointer to the jmp_buf into %eax
    movl %ebx, 0(%eax)          // Save %ebx
    movl %esi, 4(%eax)          // Save %esi
    movl %edi, 8(%eax)          // Save %edi
    movl %ebp, 12(%eax)         // Save %ebp
    movl %esp, 16(%eax)         // Save %esp
    movl (%esp), %ecx           // Save the return address
    movl %ecx, 20(%eax)         // Store it in the jmp_buf
    xorl %eax, %eax             // Return 0
    ret

.global longjmp
.type longjmp, @function
longjmp:
    movl 4(%esp), %eax          // Load the pointer to the jmp_buf into %eax
    movl 8(%esp), %ecx          // Load the return value into %ecx
    movl 0(%eax), %ebx          // Restore %ebx
    movl 4(%eax), %esi          // Restore %esi
    movl 8(%eax), %edi          // Restore %edi
    movl 12(%eax), %ebp         // Restore %ebp
    movl 16(%eax), %esp         // Restore %esp
    movl 20(%eax), %edx         // Load the saved return address into %edx
    testl %ecx, %ecx            // Check if the return value is 0
    jnz .Lnonzero               // If not zero, jump to .Lnonzero
    incl %ecx                   // Otherwise, set it to 1
.Lnonzero:
    movl %ecx, %eax             // Set the return value in %eax
    jmp *%edx                   // Jump to the saved return address