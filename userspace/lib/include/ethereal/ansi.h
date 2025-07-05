/**
 * @file userspace/lib/include/ethereal/ansi.h
 * @brief ANSI parser
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _ANSI_H
#define _ANSI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

/* States */
#define ANSI_STATE_NONE         0   // Nothing
#define ANSI_STATE_ESCAPE       1   // \x1b
#define ANSI_STATE_FUNCTION     2   // Function

/* Flags */
#define ANSI_FLAG_BOLD          0x01
#define ANSI_FLAG_FAINT         0x02
#define ANSI_FLAG_ITALIC        0x04
#define ANSI_FLAG_UNDERLINE     0x08
#define ANSI_FLAG_BLINKING      0x10
#define ANSI_FLAG_INVERSE       0x20
#define ANSI_FLAG_HIDDEN        0x40
#define ANSI_FLAG_STRIKETHROUGH 0x80

/**** TYPES ****/

/**
 * @brief Set color method
 * @param color The color to set (depends)
 */
typedef void (*ansi_set_color_t)(gfx_color_t color);

/**
 * @brief Write character method
 * @param ch The character to write
 */
typedef void (*ansi_write_t)(char ch);

/**
 * @brief Backspace method
 */
typedef void (*ansi_backspace_t)();

/**
 * @brief Move cursor method
 * @param cur_x The cursor X position to move to
 * @param cur_y The cursor Y position to move to
 */
typedef void (*ansi_move_cursor_t)(int16_t cur_x, int16_t cur_y);

/**
 * @brief Get cursor position
 * @param x Output cursor X position
 * @param y Output cursor Y position
 */
typedef void (*ansi_get_cursor_t)(int16_t *x, int16_t *y);

/**
 * @brief Clear screen
 */
typedef void (*ansi_clear_t)();

typedef struct ansi {
    int state;                          // Current state of the ANSI state machine
    int flags;                          // ANSI flags

    char *buf;                          // ANSI buffer
    size_t bufsz;                       // Buffer size
    size_t bufidx;                      // Buffer index

    int ansi_fg;                        // ANSI foreground code (0-255)
    int ansi_bg;                        // ANSI background code (0 - 255)

    uint32_t *ansi_pallete;             // ANSI pallete

    ansi_write_t write;                 // Write method
    ansi_set_color_t setfg;             // Set foreground
    ansi_set_color_t setbg;             // Set background
    ansi_backspace_t backspace;         // Backspace
    ansi_move_cursor_t move_cursor;     // Move cursor
    ansi_get_cursor_t get_cursor;       // Get cursor
    ansi_clear_t clear;                 // Clear screen
} ansi_t;

/**** FUNCTIONS ****/

/**
 * @brief Create ANSI object
 */
ansi_t *ansi_create();

/**
 * @brief Parse a character of input
 * @param ansi The current ANSI state
 * @param ch The character to parse
 */
void ansi_parse(ansi_t *ansi, uint8_t ch);

#endif