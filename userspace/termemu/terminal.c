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
#include <ethereal/ansi.h>

/* Graphics context */
static gfx_context_t *ctx = NULL;
static gfx_font_t *terminal_font = NULL;
static gfx_font_t *terminal_font_bold = NULL;

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

/* Colors */
gfx_color_t terminal_fg = GFX_RGB(255, 255, 255);
gfx_color_t terminal_bg = GFX_RGB(0, 0, 0);
ansi_t *terminal_ansi = NULL;

/* Keyboard structure */
keyboard_t *kbd = NULL;

/* Cell macro */
#define CELL(x, y) (&(cell_array[(y)][(x)]))
#define CURSOR CELL(cursor_x, cursor_y)

/* Flush macro */
#define FLUSH() { gfx_render(ctx); gfx_resetClips(ctx); }

/**
 * @brief Redraw a cell 
 * @param x The X coordinate of the cell
 * @param y The Y coordinate of the cell
 */
static void draw_cell(uint16_t x, uint16_t y) {
    term_cell_t *cell = CELL(x,y);

    gfx_color_t fg = (cell->highlighted ? CELL_FG_HIGHLIGHTED : cell->fg);
    gfx_color_t bg = (cell->highlighted ? CELL_BG_HIGHLIGHTED : cell->bg);
    
    gfx_font_t *f = (terminal_ansi->flags & ANSI_FLAG_BOLD) ? terminal_font_bold : terminal_font;

    gfx_rect_t r = { .x = x * CELL_WIDTH, .y = y * CELL_HEIGHT, .width = CELL_WIDTH, .height = CELL_HEIGHT };
    gfx_drawRectangleFilled(ctx, &r, bg);
    gfx_renderCharacter(ctx, f, cell->ch, r.x, r.y + 13, fg);
    
    // TODO: not
    gfx_createClip(ctx, r.x, r.y, r.width, r.height);
    
}

/**
 * @brief Set foreground color in terminal
 * @param fg The foreground to set
 */
void terminal_setfg(gfx_color_t fg) { terminal_fg = fg; }

/**
 * @brief Set background color in terminal
 * @param bg The background to set
 */
void terminal_setbg(gfx_color_t bg) { terminal_bg = bg; }

/**
 * @brief Terminal backspace method
 */
static void terminal_backspace() {
    if (cursor_x == 0) return;

    UNHIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);
    cursor_x--;

    HIGHLIGHT(CURSOR);
    CURSOR->ch = ' ';
    draw_cell(cursor_x, cursor_y);
}

/**
 * @brief Scroll the terminal up
 */
void terminal_scroll() {
    // Starting from here to terminal_height-1, we must adjust contents
    for (int y = 0; y < terminal_height-1; y++) {
        for (int x = 0; x < terminal_width; x++) {
            memcpy(CELL(x, y), CELL(x, y+1), sizeof(term_cell_t));
        }
    }

    for (int x = 0; x < terminal_width; x++) {
        CELL(x, terminal_height-1)->ch = ' ';
        CELL(x, terminal_height-1)->highlighted = 0;
    }

    // vmem
    memcpy(ctx->backbuffer, ctx->backbuffer + sizeof(uint32_t) * GFX_WIDTH(ctx) * CELL_HEIGHT, (GFX_HEIGHT(ctx) - CELL_HEIGHT) * GFX_WIDTH(ctx) * (GFX_BPP(ctx)/8));
    memset(ctx->backbuffer + sizeof(uint32_t) * (GFX_HEIGHT(ctx) - CELL_HEIGHT) * GFX_WIDTH(ctx), 0, GFX_WIDTH(ctx) * CELL_HEIGHT * 4);

    // Flush
    gfx_resetClips(ctx);
    gfx_render(ctx);

    // Now zero out the bottom ones
}

/**
 * @brief Write character to the terminal
 * @param ch The character to write to the terminal
 */
static void terminal_write(char ch) {
    UNHIGHLIGHT(CURSOR);

    // Handle special characters
    if (ch == '\n') {
        CURSOR->ch = ' ';
        CURSOR->fg = terminal_fg;
        CURSOR->bg = terminal_bg;
        draw_cell(cursor_x, cursor_y);

        // Reset Y and X
        cursor_y++;
        cursor_x = 0;

        // Update cursor
        goto _update_cursor;
    } else if (ch == '\r') {
        return;
    }

    // Draw the new character
    CURSOR->ch = ch;
    CURSOR->fg = terminal_fg;
    CURSOR->bg = terminal_bg;
    draw_cell(cursor_x, cursor_y);

    // To the next X value
    cursor_x++;

_update_cursor:

    if (cursor_x >= terminal_width) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= terminal_height) {
        // We need to scroll the terminal
        terminal_scroll();
        cursor_y--;
        cursor_x = 0;
    }

    HIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);
}

/**
 * @brief Create the PTY
 * @param startup_program The startup program to use
 */
void terminal_createPTY(char *startup_program) {
    if (openpty(&pty_master, &pty_slave, NULL, NULL, NULL) < 0) {
        perror("openpty");
        exit(1);
    }

    pid_t cpid = fork();
    if (!cpid) {
        // Spawn startup program
        setsid();

        dup2(pty_slave, STDIN_FILENO);
        dup2(pty_slave, STDOUT_FILENO);
        dup2(pty_slave, STDERR_FILENO);

        ioctl(pty_slave, TIOCSCTTY, 1);
        tcsetpgrp(pty_slave, getpid());

        // Spawn shell
        // TODO: Support argc/argv
        char *argv[] = { startup_program, NULL };
        execvp(startup_program, (const char **)argv);
        exit(1);
    }
}

/**
 * @brief Main method
 */
int main(int argc, char *argv[]) {
    // Initialize the graphics context
    ctx = gfx_createFullscreen(CTX_DEFAULT);
    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);

    // Load the fonts
    terminal_font = gfx_loadFont(ctx, "/usr/share/DejaVuSansMono.ttf");
    gfx_setFontSize(terminal_font, 13);
    terminal_font_bold = gfx_loadFont(ctx, "/usr/share/DejaVuSansMono-Bold.ttf");
    gfx_setFontSize(terminal_font_bold, 13);

    // Create ANSI
    terminal_ansi = ansi_create();
    terminal_ansi->write = terminal_write;
    terminal_ansi->backspace = terminal_backspace;
    terminal_ansi->setfg = terminal_setfg;
    terminal_ansi->setbg = terminal_setbg;

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
            cell_array[i][x].fg = terminal_fg;
            cell_array[i][x].bg = terminal_bg;
            cell_array[i][x].ch = ' ';
        }
    }

    // Draw cursor
    HIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);

    // Create keyboard fd
    keyboard_fd = open("/device/keyboard", O_RDONLY);

    if (keyboard_fd < 0) {
        perror("open");
        return 1;
    }

    // Create keyboard object
    kbd = keyboard_create();

    // Create the PTY
    terminal_createPTY("essence");

    dup2(pty_master, STDIN_FILENO);
    dup2(pty_master, STDOUT_FILENO);

    // Enter main loop
    for (;;) {
        // Get events
        struct pollfd fds[] = { { .fd = keyboard_fd, .events = POLLIN }, { .fd = pty_master, .events = POLLIN}};
        int p = poll(fds, 2, -1);
        if (p < 0) return 1;

        // Keyboard events?
        if (fds[0].revents & POLLIN) {
            key_event_t evp;
            ssize_t r = read(keyboard_fd, &evp, sizeof(key_event_t));
            if (r != sizeof(key_event_t)) continue;

            keyboard_event_t *ev = keyboard_event(kbd, &evp);
            
            if (ev->type == KEYBOARD_EVENT_PRESS && ev->ascii) {
                if (ev->ascii == '\b') ev->ascii = 0x7F;
                char b[] = { ev->ascii };
                write(pty_master, b, 1);
            }
                
            free(ev);
        }

        if (fds[1].revents & POLLIN) {
            // We have data on PTY
            char buf[4096];
            ssize_t r = read(pty_master, buf, 4096);
            if (r) {
                buf[r] = 0;

                for (int i = 0; i < r; i++) {
                    ansi_parse(terminal_ansi, buf[i]);
                }

                FLUSH();
            }
        }
    }


    return 0;
}