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

#include <stdint.h>

volatile uint8_t *uart = (uint8_t *) 0x09000000;

void putchar(char c) {
    *uart = c;
}

void print(const char *s) {
    while(*s != '\0') {
        putchar(*s);
        s++;
    }
}

void bootstub_main(void) {
    print("Hello world!\n");
}