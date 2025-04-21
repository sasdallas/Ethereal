/**
 * @file hexahedron/misc/term.c
 * @brief Terminal driver for Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/gfx/term.h>
#include <kernel/debug.h>
#include <kernel/gfx/gfx.h>
#include <stddef.h>
#include <string.h>

/* GCC */
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

/* Width / Height */
int terminal_width = 0;
int terminal_height = 0;

/* X and Y */
int terminal_x = 0;
int terminal_y = 0;

/* Colors */
color_t terminal_fg = TERMINAL_DEFAULT_FG;
color_t terminal_bg = TERMINAL_DEFAULT_BG;

/* Hacked in ANSI escape codes */
static int ansi_escape_code = 0; // 0 = no code, 1 = received a \033, 2 = received a [. Resets to 0 on m, meaning that yes, you can break this by not specifying m.
static int ansi_color_code = 0; 

/**
 * @brief Initialize the terminal system - sets terminal_print as default printf method.
 * @returns 0 on success, -EINVAL on no video driver/font driver.
 * @note The terminal can always be reinitalized. It will clear the screen and reset parameters.
 */
int terminal_init(color_t fg, color_t bg) {
    // Get video information
    video_driver_t *driver = video_getDriver();
    if (!driver) return -EINVAL; // No video driver available

    // Get font data
    int fontHeight = font_getHeight();
    int fontWidth = font_getWidth();
    if (!fontHeight || !fontWidth) return -EINVAL; // No font loaded

    // Setup terminal variables
    terminal_width = driver->screenWidth / fontWidth;
    terminal_height = driver->screenHeight / fontHeight;
    terminal_fg = fg;
    terminal_bg = bg;

    // Reset terminal X and terminal Y
    terminal_x = terminal_y = 0;

    // Clear screen
    terminal_clear(terminal_fg, terminal_bg);

    // Done!
    return 0;
}

/**
 * @brief Clear terminal screen
 * @param fg The foreground of the terminal
 * @param bg The background of the terminal
 */
void terminal_clear(color_t fg, color_t bg) {
    terminal_fg = fg;
    terminal_bg = bg;

    video_clearScreen(terminal_bg);
    gfx_drawLogo(COLOR_WHITE);

    terminal_x = 0;
    terminal_y = 0;
}


/**
 * @brief Scroll the terminal
 */
void terminal_scroll() {
    // Get pointer to video memory
    uint8_t *vmem = video_getFramebuffer();

    // Get driver
    video_driver_t *driver = video_getDriver();

    gfx_drawLogo(COLOR_BLACK); // Black out logo

    memmove(vmem, vmem + sizeof(uint32_t) * driver->screenWidth * font_getHeight(), (driver->screenHeight - font_getHeight()) * driver->screenWidth * 4);
    memset(vmem + sizeof(uint32_t) * (driver->screenHeight - font_getHeight()) * driver->screenWidth, 0, driver->screenWidth * font_getHeight() * 4);

    gfx_drawLogo(COLOR_WHITE);  // Redraw logo

    terminal_y--;   

    video_updateScreen();
}

/**
 * @brief Handle a backspace in the terminal
 */
static void terminal_backspace() {
    if (terminal_x <= 0) return; 
    terminal_x--;
    terminal_putchar(' ');
    terminal_x--;
}

/**
 * @brief Handle parsing ANSI color code
 */
static void terminal_parseANSI() {
    if (!ansi_color_code) {
        // Reset to default
        terminal_fg = TERMINAL_DEFAULT_FG;
        terminal_bg = TERMINAL_DEFAULT_BG;
        goto _reset;
    }

    int bg = (ansi_color_code >= 40) ? 1 : 0; // Whether this is setting bg or fg
    
    // Subtract necessary
    ansi_color_code -= (bg) ? 40 : 30;
    if (ansi_color_code > 7 || ansi_color_code < 0) goto _reset;

    // Switch statement (ugly but whatever)
    color_t color;
    switch (ansi_color_code) {
        case 0:
            color = COLOR_BLACK;
            break;
        case 1:
            color = COLOR_RED;
            break;
        case 2:
            color = COLOR_GREEN;
            break;
        case 3:
            color = COLOR_YELLOW;
            break;
        case 4:
            color = COLOR_BLUE;
            break;
        case 5:
            color = COLOR_PURPLE;
            break;
        case 6:
            color = COLOR_CYAN;
            break;
        default:
            color = COLOR_WHITE;
            break;
    }

    // Set color
    if (bg) terminal_bg = color;
    else terminal_fg = color;

_reset:
    // Reset all values
    ansi_color_code = 0;
    ansi_escape_code = 0;
}

/**
 * @brief Put character method
 */
int terminal_putchar(int c) {
    if (!terminal_width || !terminal_height) return 0;

    // Handle the character
    switch (c) {
        case '\n':
            // Newline
            terminal_x = 0;
            terminal_y++;

            // Flush
            video_updateScreen();
            break;
    
        case '\b':
            // Backspace
            terminal_backspace();
            break;
        
        case '\0':
            // Null character
            break;
        
        case '\t':
            // Tab
            for (int i = 0; i < 4; i++) terminal_putchar(' ');
            break;
        
        case '\r':
            // Return carriage
            terminal_x = 0;
            break;

        case '\033':
            // ANSI escape sequence. This means that the next character should be a [
            ansi_escape_code = 1;
            break;

        case 'm':
            // Final part of the ANSI escape sequence.
            if (ansi_escape_code == 2) {
                // This means that our color codes are setup in ansi_color_code.
                // Parse and handle (which will reset avlues as well).
                terminal_parseANSI();
                break;
            }

            // Fall through to default case.

        case '[':
            // Are we handling an ANSI escape sequence?
            if (ansi_escape_code) {
                ansi_escape_code = 2;
                break;
            }
            
            // Fall through to default case.
        
        case ';':
            // Are we handling an ANSI escape sequence? (it can be \033[1;XXm too)
            if (ansi_escape_code == 2) {
                // Reset color code & break;
                ansi_color_code = 0;
                break;
            }

            // Fall through to default case.

        default:
            if (ansi_escape_code == 1) {
                // No '[' was received, reset.
                ansi_escape_code = 0; 
            } else if (ansi_escape_code == 2) {
                // Tack on to ANSI color code.
                ansi_color_code = (ansi_color_code * 10) + (c - '0');
                break;
            }

            // Normal character
            font_putCharacter(c, terminal_x, terminal_y, terminal_fg, terminal_bg);
            terminal_x++;
            break;
    }

    if (terminal_x >= terminal_width) {
        // End of the screen width
        terminal_y++;
        terminal_x = 0;
    }

    // Clear the screen if the height is too big
    if (terminal_y >= terminal_height) {
        terminal_scroll();
    }

    return 0;
}

/**
 * @brief Put character method (printf-conforming)
 */
int terminal_print(void *user, char c) {
    return terminal_putchar(c);
}

/**
 * @brief Set the coordinates of the terminal
 */
void terminal_setXY(int x, int y) {
    if (x >= terminal_width) return;
    if (y >= terminal_height) return;
    terminal_x = x;
    terminal_y = y;
}


/**** GETTER FUNCTIONS ****/

/**
 * @brief Get the current X of the terminal
 */
int terminal_getX() {
    return terminal_x;
}

/**
 * @brief Get the current Y of the terminal
 */
int terminal_getY() {
    return terminal_y;
}

/**
 * @brief Get the current fg of the terminal
 */
color_t terminal_getForeground() {
    return terminal_fg;
}

/**
 * @brief Get the current bg of the terminal
 */
color_t terminal_getBackground() {
    return terminal_bg;
}

/**
 * @brief Get the current width of the terminal
 */
int terminal_getWidth() {
    return terminal_width;
}

/**
 * @brief Get the current height of the terminal
 */
int terminal_getHeight() {
    return terminal_height;
}