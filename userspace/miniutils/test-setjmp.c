/**
 * @file userspace/miniutils/test-setjmp.c
 * @brief setjmp/longjmp testing
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <setjmp.h>

jmp_buf buf;

void test() {
    printf("longjmp\n");

    longjmp(buf, 1);

    printf("what\n");
}

int main(int argc, char **argv) {
    if (setjmp(buf)) {
        printf("back\n");
    } else {
        printf("setjmp\n");
        test();
    }

    return 0;
}