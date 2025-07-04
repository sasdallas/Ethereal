/**
 * @file userspace/termemu/terminal.c
 * @brief Terminal emulator for Ethereal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "terminal.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ethereal/keyboard.h>
#include <pty.h>
#include <poll.h>
#include <graphics/gfx.h>

/* Graphics context */
static gfx_context_t *ctx = NULL;
static gfx_font_t *terminal_font = NULL;

/* Font information */
const int16_t terminal_font_scaling = 13;
const int16_t terminal_char_offset = 13;

/* Width and height in cells */
static int16_t terminal_width = 0;
static int16_t terminal_height = 0;

/* Cursor data */
static int16_t cursor_x = 0;
static int16_t cursor_y = 0;

/* Terminal cell array */
term_cell_t **cell_array = NULL;

/* File descriptors */
int pty_master = -1;
int pty_slave = -1;
int keyboard_fd = -1;

/* Keyboard structure */
keyboard_t *kbd = NULL;

/* Cell macro */
#define CELL(x, y) (&(cell_array[(y)][(x)]))

#define CURSOR CELL(cursor_x, cursor_y)

/**
 * @brief Redraw a cell 
 * @param x The X coordinate of the cell
 * @param y The Y coordinate of the cell
 */
static void draw_cell(uint16_t x, uint16_t y) {
    term_cell_t *cell = CELL(x,y);
    gfx_rect_t r = { .x = x * CELL_WIDTH, .y = y * CELL_HEIGHT, .width = CELL_WIDTH, .height = CELL_HEIGHT };
    gfx_drawRectangleFilled(ctx, &r, cell->bg);
    gfx_renderCharacter(ctx, terminal_font, cell->ch, r.x + 1, r.y + 13, cell->fg);
    
    // TODO: not
    gfx_createClip(ctx, r.x, r.y, r.width, r.height);
    gfx_render(ctx);
    gfx_resetClips(ctx);
}

/**
 * @brief Write character to the terminal
 * @param ch The character to write to the terminal
 */
static void terminal_write(char ch) {
    UNHIGHLIGHT(CURSOR);
    CURSOR->ch = ch;
    draw_cell(cursor_x, cursor_y);

    if (ch == '\n') {
        cursor_y++;
        cursor_x = 0;
        return;
    }

    cursor_x++;

_update_cursor:
    HIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);
}

int main(int argc, char *argv[]) {
    // Initialize the graphics context
    ctx = gfx_createFullscreen(CTX_DEFAULT);
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);

    // Load the font
    terminal_font = gfx_loadFont(ctx, "/usr/share/DejaVuSansMono.ttf");
    gfx_setFontSize(terminal_font, 13);

    // Calculate width and height
    terminal_width = GFX_WIDTH(ctx) / CELL_WIDTH;
    terminal_height = GFX_HEIGHT(ctx) / CELL_HEIGHT;

    // Create cell array
    cell_array = malloc(terminal_height * sizeof(term_cell_t*));
    for (int i = 0; i < terminal_height; i++) {
        cell_array[i] = malloc(sizeof(term_cell_t) * terminal_width);
        memset(cell_array[i], 0, sizeof(term_cell_t) * terminal_width);

        // Build the initial cell system
        for (int x = 0; x < terminal_width; x++) {
            cell_array[i][x].fg = CELL_FG_UNHIGHLIGHTED;
            cell_array[i][x].bg = CELL_BG_UNHIGHLIGHTED;
            cell_array[i][x].ch = ' ';
        }
    }

    // Draw cursor
    HIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);

    // Create keyboard fd
    keyboard_fd = open("/device/keyboard", O_RDONLY);
    kbd = keyboard_create();

    struct pollfd fds[] = { { .fd = keyboard_fd, .events = POLLIN }};

    for (;;) {
        int p = poll(fds, 1, -1);
        if (p < 0) return 1;

        if (fds[0].revents & POLLIN) {
            key_event_t evp;
            ssize_t r = read(keyboard_fd, &evp, sizeof(key_event_t));
            if (r != sizeof(key_event_t)) continue;

            keyboard_event_t *ev = keyboard_event(kbd, &evp);
            
            if (ev->type == KEYBOARD_EVENT_PRESS && ev->ascii) {
                if (ev->ascii == '\b') ev->ascii = 0x7F;
                terminal_write(ev->ascii);
                
            }
                
            free(ev);
        }
    }


    return 0;
}