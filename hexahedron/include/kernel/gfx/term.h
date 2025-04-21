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
#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>
#include <errno.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define TERMINAL_DEFAULT_FG     RGB(255, 255, 255)  // White
#define TERMINAL_DEFAULT_BG     RGB(0, 0, 0)        // Black

/**** FUNCTIONS ****/

/**
 * @brief Initialize the terminal system - sets terminal_print as default printf method.
 * @returns 0 on success, -EINVAL on no video driver/font driver.
 * @note The terminal can always be reinitalized. It will clear the screen and reset parameters.
 */
int terminal_init(color_t fg, color_t bg);

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


/**
 * @brief Set the coordinates of the terminal
 */
void terminal_setXY(int x, int y);

/**
 * @brief Get the current X of the terminal
 */
int terminal_getX();

/**
 * @brief Get the current Y of the terminal
 */
int terminal_getY();

/**
 * @brief Get the current fg of the terminal
 */
color_t terminal_getForeground();

/**
 * @brief Get the current bg of the terminal
 */
color_t terminal_getBackground();

/**
 * @brief Get the current width of the terminal
 */
int terminal_getWidth();

/**
 * @brief Get the current height of the terminal
 */
int terminal_getHeight();

#endif