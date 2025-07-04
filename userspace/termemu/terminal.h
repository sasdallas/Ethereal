/**
 * @file userspace/termemu/terminal.h
 * @brief terminal.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _TERMINAL_H
#define _TERMINAL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <graphics/gfx.h>

/**** DEFINITIONS ****/

/* Cell width and height */
#define CELL_WIDTH          8
#define CELL_HEIGHT         17

/* Cell foreground/background */
#define CELL_FG_UNHIGHLIGHTED   GFX_RGB(255, 255, 255)
#define CELL_FG_HIGHLIGHTED     GFX_RGB(0, 0, 0)
#define CELL_BG_UNHIGHLIGHTED   GFX_RGB(0, 0, 0)
#define CELL_BG_HIGHLIGHTED     GFX_RGB(255, 255, 255)

/**** TYPES ****/

typedef struct term_cell {
    char ch;                // The character in the cell
    uint8_t highlighted;    // Cell is highlighted
    gfx_color_t fg;         // Foreground
    gfx_color_t bg;         // Background
} term_cell_t;

/**** MACROS ****/

#define HIGHLIGHT(cell) { cell->highlighted = 1; }
#define UNHIGHLIGHT(cell) { cell->highlighted = 0; }

#endif