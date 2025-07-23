/**
 * @file userspace/lib/ansi/ansi.c
 * @brief ANSI library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/ansi.h>
#include <ethereal/ansi_defs.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define ANSI_DEFAULT_BUFFER_SIZE        32
#define ANSI_EXPAND() { if (ansi->bufidx + 1 >= ansi->bufsz) { ansi->buf = realloc(ansi->buf, ansi->bufsz*2); ansi->bufsz *= 2; }}
#define ANSI_PUSH(ch) { ansi->buf[ansi->bufidx] = ch; ansi->bufidx++; ANSI_EXPAND(); }
#define ANSI_DEFAULT(a) (a == 9)
#define ANSI_TO_RGB(arg) (ansi_convert(ansi, arg))
#define ANSI_SET_OR_CLEAR(arg, flag) (arg > 9) ? (ansi->flags &= ~(flag)) : (ansi->flags |= (flag))

uint32_t ansi_default_pallete[] = {
    GFX_RGB(0, 0, 0),               // Black
    GFX_RGB(255, 0, 0),             // Red
    GFX_RGB(62, 154, 6),            // Green
    GFX_RGB(0xc4, 0xa0, 0x00),          // Yellow
    GFX_RGB(52, 101, 164),          // Blue
    GFX_RGB(170, 0, 170),           // Purple
    GFX_RGB(0, 170, 170),           // Cyan
    GFX_RGB(0xee, 0xee, 0xec),         // White
    GFX_RGB(85, 85, 85),            // Dark gray
    GFX_RGB(255, 85, 85),           // Light red
    GFX_RGB(85, 255, 85),           // Light green
    GFX_RGB(0xfc, 0xe9, 0xf4),      // Yellow
    GFX_RGB(0x72, 0x9f, 0xcf),      // Light blue
    GFX_RGB(255, 85, 255),          // Light purple
    GFX_RGB(0x34, 0xe2, 0xe2),      // Light cyan
    GFX_RGB(255, 255, 255)          // White
};

/**
 * @brief Create ANSI object
 */
ansi_t *ansi_create() {
    ansi_t *a = malloc(sizeof(ansi_t));
    a->state = ANSI_STATE_NONE;
    a->flags = 0;
    
    a->buf = malloc(ANSI_DEFAULT_BUFFER_SIZE);
    a->bufidx = 0;
    a->bufsz = ANSI_DEFAULT_BUFFER_SIZE;

    a->ansi_pallete = ansi_default_pallete;
    a->ansi_fg = 9;
    a->ansi_bg = 9;
    return a;
}

/**
 * @brief Convert an ANSI ID to a color
 * @param ansi ANSI
 * @param id The ID to convert
 */
uint32_t ansi_convert(ansi_t *ansi, int id) {
    if (id < 16) {
        return ansi->ansi_pallete[id];
    } else if (id <= 231) {
        // In-between
        uint8_t r = (id - 16) / 36 % 6 * 40 + 55;
        uint8_t g = (id - 16) / 6 % 6 * 40 + 55;
        uint8_t b = (id - 16) / 1 % 6 * 40 + 55;
        return GFX_RGB(r, g, b);
    } else if (id <= 255) {
        // Grayscale
        uint8_t gray = (id - 232) * 10 + 8;
        return GFX_RGB(gray, gray, gray);
    } else {
        return GFX_RGB(255, 255, 255);
    }
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
        ansi->buf[ansi->bufidx] = 0;
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

        // Get cursor
        int16_t cx, cy;
        ansi->get_cursor(&cx, &cy);

        // What function do we have?
        switch (ch) {
            case ED:
                // Erase display
                if (ansi->clear) ansi->clear();
                break;

            case SGR:
                for (int i = 0; i < argc; i++) {
                    int arg = strtol(argv[i], NULL, 10);

                    if (!arg) {
                        // Zero signifies a reset on everything
                        ansi->ansi_fg = 15; // TODO: Customize
                        ansi->ansi_bg = 0;
                        ansi->flags = 0;
                    } else if (arg == 1 || arg == 22) {
                        (arg == 22) ? (ansi->flags &= ~(ANSI_FLAG_BOLD | ANSI_FLAG_FAINT)) : (ansi->flags |= ANSI_FLAG_BOLD);
                    } else if (arg == 2) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_FAINT);
                    } else if (arg == 3 || arg == 23) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_ITALIC);
                    } else if (arg == 4 || arg == 24) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_UNDERLINE);
                    } else if (arg == 5 || arg == 25) {
                        if (arg == 5) {
                            if (i < argc) {
                                if (strtol(argv[i-1], NULL, 10) == 38) {
                                    // 38 signals foreground
                                    ansi->ansi_fg = strtol(argv[i+1], NULL, 10);
                                    i++;
                                } else if (strtol(argv[i-1], NULL, 10) == 48) {
                                    // 48 signals background
                                    ansi->ansi_bg = strtol(argv[i+1], NULL, 10);
                                    i++;
                                }
                            } else {
                                ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_BLINKING);
                            }
                        } else {
                            ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_BLINKING);
                        }
                    } else if (arg == 7 || arg == 27) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_INVERSE);
                    } else if (arg == 8 || arg == 28) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_HIDDEN);
                    } else if (arg == 9 || arg == 29) {
                        ANSI_SET_OR_CLEAR(arg, ANSI_FLAG_STRIKETHROUGH);
                    }
                    
                    // Color code parsing
                    if (arg >= 30 && arg < 39) {
                        // Set foreground color code
                        ansi->ansi_fg = arg - 30;
                    } else if (arg >= 40 && arg < 49) {
                        // Set background color code
                        ansi->ansi_bg = arg - 40;
                    } else if (arg >= 90 && arg <= 97) {
                        // Set foreground bright color code
                        ansi->ansi_fg = arg - 82;
                    } else if (arg >= 100 && arg <= 107) {
                        ansi->ansi_bg = arg - 92;
                    } else if (arg == 39) {
                        ansi->ansi_fg = 15; // TODO: customize
                    } else if (arg == 49) {
                        ansi->ansi_bg = 0;
                    }
                }


                // Set colors
                gfx_color_t fg = (ANSI_TO_RGB(ansi->ansi_fg));
                gfx_color_t bg = (ANSI_TO_RGB(ansi->ansi_bg));
                ansi->setfg(fg);
                ansi->setbg(bg);
                break;
            
            case CUU:
                // Cursor up
                int lines_up = 1;
                if (argc) {
                    lines_up = strtol(argv[0], NULL, 10);
                }

                ansi->move_cursor(cx, cy-lines_up);
                break;
    
            case CUD:
                // Cursor down
                int lines_down = 1;
                if (argc) {
                    lines_down = strtol(argv[0], NULL, 10);
                }

                ansi->move_cursor(cx, cy+lines_down);
                break;

            case CUF:
                // Cursor forward
                int cols_fwd = 1;
                if (argc) {
                    cols_fwd = strtol(argv[0], NULL, 10);
                }

                ansi->move_cursor(cx+cols_fwd, cy);
                break;
                
            case CUB:
                // Cursor backwards
                int cols_backward = 1;
                if (argc) {
                    cols_backward = strtol(argv[0], NULL, 10);
                }

                ansi->move_cursor(cx-cols_backward, cy);
                break;

            case CUP:
                if (argc < 2) {
                    ansi->move_cursor(0, 0);
                } else {
                    ansi->move_cursor(strtol(argv[1], NULL, 10), strtol(argv[0], NULL, 10));
                }

                break;

            case EL:
                int op = 0;
                if (argc) op = strtol(argv[0], NULL, 10);
                
                int x = 0, x_end = 0;
                switch (op) {
                    case 0:
                        x = cx;
                        x_end = ansi->screen_width;
                        break;

                    case 1:
                        x = 0;
                        x_end = cx;
                        break;

                    case 2:
                        x = 0;
                        x_end = ansi->screen_width;
                        break;
                }

                for (int i = x; i < x_end; i++) ansi->set_cell(i, cy, ' ');
                break;
            
            case SD:
                int lines = 1;
                if (argc) {
                    lines = strtol(argv[0], NULL, 10);
                }    

                ansi->scroll(lines);
                break;

            default:
                fprintf(stderr, "ANSI: Unrecognized function '%c'\n", ch);
        }

        ansi->bufidx = 0;
        ansi->state = ANSI_STATE_NONE;
    }
}