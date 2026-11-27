/**
 * @file hexahedron/include/kernel/misc/term.h
 * @brief Terminal driver for Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_GFX_TERMINAL_H
#define KERNEL_GFX_TERMINAL_H

/**** INCLUDES ****/
#include <kernel/gfx/color.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the terminal
 */
void terminal_init();

/**
 * @brief Put character method
 */
int terminal_putchar(int c);

/**
 * @brief Put character method (printf-conforming)
 */
int terminal_print(void *user, char c);

/**
 * @brief Clear terminal screen
 * @param fg The foreground of the terminal
 * @param bg The background of the terminal
 */
void terminal_clear(color_t fg, color_t bg);

#endif
