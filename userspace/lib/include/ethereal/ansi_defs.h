/**
 * @file userspace/lib/include/ethereal/ansi_defs.h
 * @brief ANSI escape code definitions
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _ANSI_DEFS_H
#define _ANSI_DEFS_H

/**** DEFINITIONS ****/

/* Generic ANSI codes */
#define BEL                     '\a'
#define BS                      '\b'
#define HT                      '\t'
#define LF                      '\n'
#define VT                      '\v'
#define FF                      '\f'
#define CR                      '\r'
#define ESC                     '\033'
#define DEL                     0x7F

/* Functions */
#define CUU                     'A'     // Cursor up
#define CUD                     'B'     // Cursor down
#define CUF                     'C'     // Cursor forward
#define CUB                     'D'     // Cursor backward
#define CNL                     'E'     // Cursor next line
#define CPL                     'F'     // Cursor previous line
#define CHA                     'G'     // Cursor horizontal absolute
#define CUP                     'H'     // Cursor position
#define ED                      'J'     // Erase data
#define EL                      'K'     // Erase in line
#define SU                      'S'     // Scroll up
#define SD                      'T'     // Scroll down
#define HVP                     'f'     // Horizontal and vertical position
#define SGR                     'm'     // Set graphics mode
#define IL                      'L'     // Insert lines
#define DL                      'M'     // Delete lines


#endif