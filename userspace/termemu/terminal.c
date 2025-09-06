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
#include <getopt.h>
#include <ethereal/celestial.h>
#include <sys/ioctl.h>

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

/* Scrollback array */
list_t *scrollback_up = NULL;
list_t *scrollback_down = NULL;
int sb_row_count_total = 0;

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

/* Window if we launched in graphical mode */
window_t *win = NULL;

/* Cell macro */
#define CELL(x, y) (&(cell_array[(y)][(x)]))
#define CURSOR CELL(cursor_x, cursor_y)

/* Flush macro */
#define FLUSH() { gfx_render(ctx); gfx_resetClips(ctx); if (win) { celestial_flip(win); }}

static void draw_cell(uint16_t x, uint16_t y);

/**
 * @brief Help function
 */
void usage() {
    printf("Usage: termemu [-f] [-v]\n");
    printf("Terminal emulator with ANSI support\n\n");
    printf(" -h, --help         Display this help message\n");
    printf(" -f, --fullscreen   Enable fullscreen mode\n");
    printf(" -v, --version      Print the version of termemu\n\n");
    exit(1);
}

/**
 * @brief Version function
 */
void version() {
    printf("termemu version 1.2.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Set cursor position
 * @param x X pos of the cursor
 * @param y Y position of the cursor
 */
void terminal_setCursor(int16_t x, int16_t y) {
    if (y < 0) y = 0;
    if (x < 0) x = 0;
    if (y > terminal_height) y = terminal_height;
    if (x > terminal_width) x = terminal_width;

    UNHIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);
    cursor_x = x;
    cursor_y = y;
    HIGHLIGHT(CURSOR);
    draw_cell(cursor_x, cursor_y);
    FLUSH();
}

/**
 * @brief Get cursor position
 * @param x Output X
 * @param y Output Y
 */
void terminal_getCursor(int16_t *x, int16_t *y) {
    *x = cursor_x;
    *y = cursor_y;
}

/**
 * @brief Redraw a cell 
 * @param x The X coordinate of the cell
 * @param y The Y coordinate of the cell
 */
static void draw_cell(uint16_t x, uint16_t y) {
    term_cell_t *cell = CELL(x,y);

    gfx_color_t fg = (cell->highlighted ? CELL_FG_HIGHLIGHTED : cell->fg);
    gfx_color_t bg = (cell->highlighted ? CELL_BG_HIGHLIGHTED : cell->bg);
    
    gfx_font_t *f = cell->bold ? terminal_font_bold : terminal_font;

    gfx_rect_t r = { .x = x * CELL_WIDTH, .y = y * CELL_HEIGHT, .width = CELL_WIDTH, .height = CELL_HEIGHT };
    gfx_drawRectangleFilled(ctx, &r, bg);
    gfx_renderCharacter(ctx, f, cell->ch, r.x, r.y + 13, fg);
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
    draw_cell(cursor_x, cursor_y);
}

/**
 * @brief Duplicate a row
 */
term_cell_t *terminal_duplicateRow(int y) {
    term_cell_t *out = malloc(sizeof(term_cell_t) * terminal_width);
    memcpy(out, cell_array[y], sizeof(term_cell_t) * terminal_width);
    return out;
}

void terminal_scrollOne() {
    // Add a new row into the scrollback up
    term_cell_t *sb_row = terminal_duplicateRow(0);
    list_append(scrollback_up, sb_row);
    sb_row_count_total++;

    // Starting from here to terminal_height-1, we must adjust contents
    for (int y = 0; y < terminal_height - 1; y++) {
        memcpy(cell_array[y], cell_array[y+1], sizeof(term_cell_t) * terminal_width);
    }

    // vmem
    memcpy(ctx->backbuffer, ctx->backbuffer + GFX_PITCH(ctx) * CELL_HEIGHT, (GFX_HEIGHT(ctx) - CELL_HEIGHT) * GFX_PITCH(ctx));

    // If we need to go down all of them, do that.
    node_t *n = list_pop(scrollback_down);

    // Before we zero, check if we have stuff already.
    if (n) {
        term_cell_t *down_row = (term_cell_t*)n->value;
        free(n);

        // memcpy(CELL(0, terminal_height-1), down_row, sizeof(term_cell_t) * terminal_width);
        for (int x = 0; x < terminal_width; x++) {
            memcpy(CELL(x, terminal_height-1), &down_row[x], sizeof(term_cell_t));
            draw_cell(x, terminal_height-1);
        }

        free(down_row);
    } else  {
        for (int x = 0; x < terminal_width; x++) {
            CELL(x, terminal_height-1)->ch = ' ';
            CELL(x, terminal_height-1)->highlighted = 0;
            CELL(x, terminal_height-1)->fg = terminal_fg;
            CELL(x, terminal_height-1)->bg = terminal_bg;

            draw_cell(x, terminal_height-1);
        }
    }

    // Flush
    gfx_resetClips(ctx);
    gfx_render(ctx);
    if (win) celestial_flip(win);
}

/**
 * @brief Scroll the terminal down
 * @param down How many rows to go down, -1 to go down all of them (plus one)
 * @param strict Do not create new lines
 */
void terminal_scroll(int down, int strict) {
    if (down == -1) {
        // Go down as many as possible
        while (scrollback_down->length) {
            terminal_scrollOne();
        }
    } else {
        for (int i = 0; i < down; i++) {
            if (scrollback_down->length || !strict) { 
                terminal_scrollOne();
            }
        }
    }
}


/**
 * @brief Scroll up once
 */
void terminal_scrollUpOne() {
    node_t *n = list_pop(scrollback_up);
    if (!n) return;

    term_cell_t *new_top_row = (term_cell_t*)n->value;
    free(n);

    // Push the bottom row up
    term_cell_t *bottom_row = terminal_duplicateRow(terminal_height-1);
    list_append(scrollback_down, bottom_row); // NOTE: It would be nicer to use popleft?
    memmove(ctx->backbuffer + GFX_PITCH(ctx) * CELL_HEIGHT, ctx->backbuffer, (GFX_HEIGHT(ctx) - CELL_HEIGHT) * GFX_PITCH(ctx));

    // Shift everything
    for (int y = terminal_height - 2; y >= 0; y--) {
        memcpy(cell_array[y+1], cell_array[y], sizeof(term_cell_t) * terminal_width);
    }

    for (int x = 0; x < terminal_width; x++) {
        memcpy(CELL(x, 0), &new_top_row[x], sizeof(term_cell_t));
        draw_cell(x, 0);
    }

    free(new_top_row);

    // Flush
    gfx_resetClips(ctx);
    gfx_render(ctx);
    if (win) celestial_flip(win);
}

/**
 * @brief Scroll the terminal up
 * @param up How many lines to go up
 */
void terminal_scrollUp(int up) {
    if (up == -1) {
        while (scrollback_up->length) terminal_scrollUpOne();
    } else {
        for (int i = 0; i < up; i++) terminal_scrollUpOne();
    }
}

/**
 * @brief Write character to the terminal
 * @param ch The character to write to the terminal
 */
static void terminal_write(char ch) {
    terminal_scroll(-1, 0);
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
    } else if (ch == '\t') {
        CURSOR->ch = ' ';
        CURSOR->fg = terminal_fg;
        CURSOR->bg = terminal_bg;
        draw_cell(cursor_x, cursor_y);

        cursor_x += (8 - cursor_x % 8);
        
        goto _update_cursor;
    } else if (ch == '\r') {
        return;
    }

    // Draw the new character
    CURSOR->bold = !!(terminal_ansi->flags & ANSI_FLAG_BOLD);
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
        terminal_scroll(1, 0);
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

    // Setup the PTY
    struct winsize size = {
        .ws_col = terminal_width,
        .ws_row = terminal_height,
        .ws_xpixel = 0,
        .ws_ypixel = 0
    };


    ioctl(pty_master, TIOCSWINSZ, &size);

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
        execvp(startup_program, argv);
        exit(1);
    }
}

/**
 * @brief Send input to slave
 * @param input The input to send
 */
static void terminal_sendInput(char *input) {
    write(pty_master, input, strlen(input));
}

/**
 * @brief Process keyboard input
 * @param event The key event
 */
static void terminal_process(keyboard_event_t *event) {
    // TODO: Send keyboard strings?

    // TODO: Some of these are definitely wrong.. PR me if you have a good resource!
    switch (event->scancode) {
        case SCANCODE_LEFT_ARROW:
            if (event->mods & KEYBOARD_MOD_LEFT_SHIFT || event->mods & KEYBOARD_MOD_RIGHT_SHIFT) {
                terminal_sendInput("\033[2D");
            } else if (event->mods & KEYBOARD_MOD_LEFT_CTRL || event->mods & KEYBOARD_MOD_RIGHT_CTRL) {
                terminal_sendInput("\033[5D");
            } else if (event->mods & KEYBOARD_MOD_LEFT_ALT || event->mods & KEYBOARD_MOD_RIGHT_ALT) {
                terminal_sendInput("\033[3D");
            } else {
                terminal_sendInput("\033[D");
            }
            break;

        case SCANCODE_RIGHT_ARROW:
            if (event->mods & KEYBOARD_MOD_LEFT_SHIFT || event->mods & KEYBOARD_MOD_RIGHT_SHIFT) {
                terminal_sendInput("\033[2C");
            } else if (event->mods & KEYBOARD_MOD_LEFT_CTRL || event->mods & KEYBOARD_MOD_RIGHT_CTRL) {
                terminal_sendInput("\033[2C");
            } else if (event->mods & KEYBOARD_MOD_LEFT_ALT || event->mods & KEYBOARD_MOD_RIGHT_ALT) {
                terminal_sendInput("\033[3C");
            } else {
                terminal_sendInput("\033[C");
            }
            break;

        case SCANCODE_UP_ARROW:
            if (event->mods & KEYBOARD_MOD_LEFT_SHIFT || event->mods & KEYBOARD_MOD_RIGHT_SHIFT) {
                terminal_sendInput("\033[2A");
            } else if (event->mods & KEYBOARD_MOD_LEFT_CTRL || event->mods & KEYBOARD_MOD_RIGHT_CTRL) {
                terminal_sendInput("\033[2A");
            } else if (event->mods & KEYBOARD_MOD_LEFT_ALT || event->mods & KEYBOARD_MOD_RIGHT_ALT) {
                terminal_sendInput("\033[3A");
            } else {
                terminal_sendInput("\033[A");
            }
            break;
            
        case SCANCODE_DOWN_ARROW:
            if (event->mods & KEYBOARD_MOD_LEFT_SHIFT || event->mods & KEYBOARD_MOD_RIGHT_SHIFT) {
                terminal_sendInput("\033[2B");
            } else if (event->mods & KEYBOARD_MOD_LEFT_CTRL || event->mods & KEYBOARD_MOD_RIGHT_CTRL) {
                terminal_sendInput("\033[2B");
            } else if (event->mods & KEYBOARD_MOD_LEFT_ALT || event->mods & KEYBOARD_MOD_RIGHT_ALT) {
                terminal_sendInput("\033[3B");
            } else {
                terminal_sendInput("\033[B");
            }
            break;
            
            

        case SCANCODE_PGUP:
            terminal_scrollUp(1);
            break;
        
        case SCANCODE_PGDOWN:
            terminal_scroll(1, 1);
            break;

        default:
            if (!event->ascii) return;
            char b[] = {event->ascii};
            write(pty_master, b, 1);
            break;
    }
}

/**
 * @brief Celestial keyboard event handler
 * @param win The window the event happened in
 * @param event_type The type of evbent
 * @param event The event
 */
void kbd_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_key_t *key = (celestial_event_key_t*)event;
    keyboard_event_t *ev = keyboard_event(kbd, &key->ev);
                
    if (ev->type == KEYBOARD_EVENT_PRESS) {
        if (ev->ascii == '\b') ev->ascii = 0x7F;
        terminal_process(ev);
    }
}

/**
 * @brief Celestial mouse scroll event handler
 * @param win The window the event happened in
 * @param event_type The type of evbent
 * @param event The event
 */
void scroll_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_mouse_scroll_t *scroll = (celestial_event_mouse_scroll_t*)event;
    
    if (scroll->direction == CELESTIAL_MOUSE_SCROLL_DOWN) {
        terminal_scroll(5, 1);
    } else if (scroll->direction == CELESTIAL_MOUSE_SCROLL_UP) {
        terminal_scrollUp(4);
    }
}

/**
 * @brief Clear screen method
 */
void terminal_clear() {
    cursor_x = 0;
    cursor_y = 0;

    // Clear all cells
    for (int32_t y = 0; y < terminal_height; y++) {
        for (int32_t x = 0; x < terminal_width; x++) {
            CELL(x, y)->ch = ' ';
            CELL(x, y)->highlighted = 0;
        }
    }

    // Clear framebuffer
    gfx_resetClips(ctx);
    gfx_clear(ctx, GFX_RGB(0,0,0));

    // Redraw cursor
    CURSOR->highlighted = 1;
    draw_cell(cursor_x, cursor_y);
    
    // Flush scrollback buffers
    list_destroy(scrollback_up, true);
    list_destroy(scrollback_down, true);
    scrollback_up = list_create("term scrollback up");
    scrollback_down = list_create("term scrollback down");

    gfx_render(ctx);
    if (win) celestial_flip(win);
}

/**
 * @brief Set cell method
 */
void terminal_setCell(int16_t x, int16_t y, char ch) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (x >= terminal_width) x = terminal_width-1;
    if (y >= terminal_height) y = terminal_height-1;

    // Change the character
    CELL(x, y)->ch = ch;
    draw_cell(x, y);
    FLUSH();
}

/**
 * @brief Scroll method
 */
void terminal_scrollAnsi(int lines) {
    fprintf(stderr, "termemu: Cannot scroll lines as this is not implemented\n");
}

/**
 * @brief Main method
 */
int main(int argc, char *argv[]) {
    int fullscreen = 0;

    int c;
    int index;
    struct option options[] = {
        { .name = "fullscreen", .has_arg = no_argument, .flag = NULL, .val = 'f' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h'},
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    while ((c = getopt_long(argc, argv, "fvh", (const struct option*)options, &index)) != -1) {
        if (!c && options[index].flag == NULL) {
            c = options[index].val;
        }

        switch (c) {
            case 'f':
                fullscreen = 1;
                break;
            case 'v':
                version();
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }


    if (fullscreen) {
        // Initialize the graphics context
        ctx = gfx_createFullscreen(CTX_DEFAULT);
    } else {
        // Initialize the graphics context for celestial
        wid_t wid = celestial_createWindow(0, 640, 476);
        win = celestial_getWindow(wid);
        celestial_subscribe(win, CELESTIAL_EVENT_KEY_EVENT | CELESTIAL_EVENT_MOUSE_SCROLL);
        celestial_setTitle(win, "Terminal");
        celestial_setHandler(win, CELESTIAL_EVENT_KEY_EVENT, kbd_handler);
        celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_SCROLL, scroll_handler);
        ctx = celestial_getGraphicsContext(win);
    }

    gfx_clear(ctx, GFX_RGB(0,0,0));
    gfx_render(ctx);
    if (win) celestial_flip(win);

    // Load the fonts
    terminal_font = gfx_loadFont(ctx, "/usr/share/DejaVuSansMono.ttf");
    gfx_setFontSize(terminal_font, 13);
    terminal_font_bold = gfx_loadFont(ctx, "/usr/share/DejaVuSansMono-Bold.ttf");
    gfx_setFontSize(terminal_font_bold, 13);


    // Calculate width and height
    terminal_width = GFX_WIDTH(ctx) / CELL_WIDTH;
    terminal_height = GFX_HEIGHT(ctx) / CELL_HEIGHT;

    // Create ANSI
    terminal_ansi = ansi_create();
    terminal_ansi->write = terminal_write;
    terminal_ansi->backspace = terminal_backspace;
    terminal_ansi->setfg = terminal_setfg;
    terminal_ansi->setbg = terminal_setbg;
    terminal_ansi->get_cursor = terminal_getCursor;
    terminal_ansi->move_cursor = terminal_setCursor;
    terminal_ansi->clear = terminal_clear;
    terminal_ansi->set_cell = terminal_setCell;
    terminal_ansi->scroll = terminal_scrollAnsi;
    terminal_ansi->screen_width = terminal_width;
    terminal_ansi->screen_height = terminal_height;


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

    // Create lists
    scrollback_up = list_create("term scrollback up");
    scrollback_down = list_create("term scrollback down");

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

    // dup2(pty_master, STDIN_FILENO);
    // dup2(pty_master, STDOUT_FILENO);

    // Enter main loop
    for (;;) {
        // Get events
        struct pollfd fds[] = { { .fd = keyboard_fd, .events = POLLIN }, { .fd = pty_master, .events = POLLIN }, { .fd = celestial_getSocketFile(), .events = POLLIN }};
        int p = poll(fds, 3, -1);
        if (p < 0) return 1;
        if (!p) continue;

        // Keyboard events?
        if (fds[0].revents & POLLIN && fullscreen) {
            key_event_t evp;
            ssize_t r = read(keyboard_fd, &evp, sizeof(key_event_t));
            if (r != sizeof(key_event_t)) continue;

            keyboard_event_t *ev = keyboard_event(kbd, &evp);
            
            if (ev && ev->type == KEYBOARD_EVENT_PRESS) {
                if (ev->ascii == '\b') ev->ascii = 0x7F;
                terminal_process(ev);
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

        if (fds[2].revents & POLLIN) {
            celestial_poll();
        }
    }


    return 0;
}