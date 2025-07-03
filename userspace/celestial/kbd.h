/**
 * @file userspace/celestial/kbd.h
 * @brief kbd.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _CELESTIAL_WM_KBD_H
#define _CELESTIAL_WM_KBD_H

/**** FUNCTIONS ****/

/**
 * @brief Initialize keyboard device
 */
void kbd_init();

/**
 * @brief Update keyboard device
 */
void kbd_update();

#endif