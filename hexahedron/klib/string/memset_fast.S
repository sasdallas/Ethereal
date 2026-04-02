
/**
 * @file hexahedron/klib/string/memset_fast.S
 * @brief fast memset
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifdef __ARCH_X86_64__

# optimized memset

.global memset
.type  memset, @function
memset:
    mov     %rdi, %rax          # save return value

    cmp     $32, %rdx
    jb      .small_memset

    # first, we will convert the fill byte into a qword
    movzbl  %sil, %ecx
    mov     %rcx, %r8
    shl     $8,  %r8
    or      %r8, %rcx
    mov     %rcx, %r8
    shl     $16, %r8
    or      %r8, %rcx
    mov     %rcx, %r8
    shl     $32, %r8
    or      %r8, %rcx
    mov     %rcx, %rax

	# calculate the amount of qwords and fill with rep stosq
    mov     %rdx, %rcx
    shr     $3, %rcx             # qword count
    rep stosq

	# fill the remaining bytes
    mov     %rdx, %rcx
    and     $7, %rcx             # remaining bytes
    rep stosb

    ret

.small_memset:
    test    %rdx, %rdx
    jz      .done_memset

.loop:
    mov     %sil, (%rdi)
    inc     %rdi
    dec     %rdx
    jnz     .loop

.done_memset:
    ret
.size memset, .-memset
#endif