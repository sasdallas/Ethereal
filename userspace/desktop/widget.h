/**
 * @file userspace/desktop/widget.h
 * @brief Desktop widget
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DESKTOP_WIDGET_H
#define _DESKTOP_WIDGET_H

/**** FUNCTIONS ****/

/**
 * @brief Load default widgets
 */
void widgets_load();

/**
 * @brief Update widgets
 */
void widgets_update();

/**
 * @brief Handle mouse movement
 */
void widget_mouseMovement(unsigned x, unsigned y);

/**
 * @brief Handle mouse exit
 */
void widget_mouseExit();

/**
 * @brief Handle mouse click
 */
void widget_mouseClick(unsigned x, unsigned y);

/**
 * @brief Handle mouse release
 */
void widget_mouseRelease(unsigned x, unsigned y);

#endif