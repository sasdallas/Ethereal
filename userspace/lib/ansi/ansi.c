/**
 * @file userspace/lib/ansi/ansi.c
 * @brief ANSI escape code parser library
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/ansi.h>
#include <ethereal/ansi_defs.h>
#include <ethereal/ansi_pallete.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define ANSI_DEFAULT_BUFFER_SIZE        32
#define ANSI_EXPAND() { if (ansi->bufidx + 1 >= ansi->bufsz) { ansi->buf = realloc(ansi->buf, ansi->bufsz*2); ansi->bufsz *= 2; }}
#define ANSI_PUSH(ch) { ansi->buf[ansi->bufidx] = ch; ansi->bufidx++; ANSI_EXPAND(); }
#define ANSI_DEFAULT(a) (a == 9)
#define ANSI_TO_RGB(arg) ((ansi->ansi_pallete[arg]))

/**
 * @brief Create ANSI object
 */
ansi_t *ansi_create() {
    ansi_t *a = malloc(sizeof(ansi_t));
    a->state = ANSI_STATE_NONE;
    
    a->buf = malloc(ANSI_DEFAULT_BUFFER_SIZE);
    a->bufidx = 0;
    a->bufsz = ANSI_DEFAULT_BUFFER_SIZE;

    a->ansi_pallete = term_colors;
    a->ansi_fg = 9;
    a->ansi_bg = 9;
    return a;
}

/**
 * @brief Parse a character of input
 * @param ansi The current ANSI state
 * @param ch The character to parse
 */
void ansi_parse(ansi_t *ansi, uint8_t ch) {
    if (ansi->state == ANSI_STATE_NONE) {
        // We are in no state, check if ch wants us to enter a state
        switch (ch) {
            case ESC:
                ansi->state = ANSI_STATE_ESCAPE;
                break;
            case DEL:
            case BS:
                ansi->backspace();
                break;
            default:
                ansi->write(ch);
                break;
        }
    } else if (ansi->state == ANSI_STATE_ESCAPE) {
        // We are in escape state, we probably just want to push this.
        // Make sure that ch was a number
        if (ch == '[') {
            // Discard these characters
        } else if (ch >= 'A' && ch <= 'z') {
            // Looks like we found a function
            ansi->state = ANSI_STATE_FUNCTION;
        } else {
            // Nothing of note
            ANSI_PUSH(ch);
        }
    }

    // ESCAPE code parsing might transition us to FUNCTION
    if (ansi->state == ANSI_STATE_FUNCTION) {
        // We need to start parsing into arguments
        char *argv[32] = { 0 };
        int argc = 0;
        
        // Tokenize
        char *save;
        char *pch = strtok_r(ansi->buf, ";", &save);
        while (pch) {
            argv[argc] = pch;
            argc++;
            pch = strtok_r(NULL, ";", &save);
        }

        // What function do we have?
        switch (ch) {
            case SGR:
                for (int i = 0; i < argc; i++) {
                    int arg = strtol(argv[i], NULL, 10);

                    if (!arg) {
                        // Zero signifies a reset on everything
                        ansi->ansi_fg = 9;
                        ansi->ansi_bg = 9;
                    } else if (arg >= 30 && arg <= 39) {
                        // Set foreground color code
                        ansi->ansi_fg = arg - 30;
                    } else if (arg >= 40 && arg <= 49) {
                        // Set background color code
                        ansi->ansi_bg = arg - 30;
                    }
                }


                // Set colors
                gfx_color_t fg = (ANSI_DEFAULT(ansi->ansi_fg) ? GFX_RGB(255, 255, 255) : ANSI_TO_RGB(ansi->ansi_fg));
                gfx_color_t bg = (ANSI_DEFAULT(ansi->ansi_bg) ? GFX_RGB(0, 0, 0) : ANSI_TO_RGB(ansi->ansi_bg));
                ansi->setfg(fg);
                ansi->setbg(bg);
        }

        ansi->bufidx = 0;
        ansi->state = ANSI_STATE_NONE;
    }
}