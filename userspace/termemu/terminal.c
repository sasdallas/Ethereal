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
#include <sys/wait.h>
#include <sys/time.h>
#include <structs/queue.h>
#include <assert.h>
#include <ft2build.h>

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
static int cursor_hold = 0; // set when cursor reaches EOL.. next character will go to 0 X
static bool cursor_enabled = true;

/* Terminal cell array */
term_cell_t *cell_array = NULL;

/* Scrollback array */
list_t *scrollback_up = NULL;
list_t *scrollback_down = NULL;
int sb_row_count_total = 0;

/* File descriptors */
int pty_master = -1;
int pty_slave = -1;
int keyboard_fd = -1;

/* Die */
bool die = false;

term_cell_t *last_rendered = NULL;

/* Colors */
gfx_color_t terminal_fg = GFX_RGB(255, 255, 255);
gfx_color_t terminal_bg = GFX_RGBA(0, 0, 0, 0xf2);
ansi_t *terminal_ansi = NULL;

/* Keyboard structure */
keyboard_t *kbd = NULL;

/* Window if we launched in graphical mode */
window_t *win = NULL;

/* Drag X/Y */
uint16_t drag_start_x, drag_start_y = 0;
uint16_t drag_last_x, drag_last_y = 0;
int is_dragging = 0;

/* Child PID */
pid_t child_pid;

/* Writer queue */
queue_t writer_queue;
pthread_t writer;
pthread_cond_t writer_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct writer_data { char data[256]; size_t size; };

/* Cell macro */
#define CELL(x, y) (&(cell_array[((y) * terminal_width) + (x)]))
#define CURSOR CELL(cursor_x, cursor_y)

#define HIGHLIGHT_CURSOR() if (cursor_enabled) HIGHLIGHT(CURSOR)
#define UNHIGHLIGHT_CURSOR() if (cursor_enabled) UNHIGHLIGHT(CURSOR)


/* Flush macro */
#define FLUSH()

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
    if (y >= terminal_height) y = terminal_height-1;
    if (x >= terminal_width) x = terminal_width-1;

    UNHIGHLIGHT_CURSOR();
    cursor_x = x;
    cursor_y = y;
    cursor_hold = 0;
    HIGHLIGHT_CURSOR();
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
 * @brief Render cell
 * @param cell The cell to draw
 * @param x X to render the cell at
 * @param y Y to render the cell at
 */
void render_cell(term_cell_t *cell, int x, int y) {
    gfx_color_t fg = (cell->highlighted ? CELL_FG_HIGHLIGHTED : cell->fg);
    gfx_color_t bg = (cell->highlighted ? CELL_BG_HIGHLIGHTED : cell->bg);
    gfx_font_t *f = (cell->bold ? terminal_font_bold : terminal_font);
    gfx_rect_t r = { .x = x * CELL_WIDTH, .y = y * CELL_HEIGHT, .width = CELL_WIDTH, .height = CELL_HEIGHT };
    gfx_drawRectangleFilled(ctx, &r, bg);
    
    // some characters dont render quite right on our fontset, so we draw them in
    if (cell->ch >= 0x2580 && cell->ch <= 0x259F) {
        if (cell->ch == 0x2588) {
            // full block
            gfx_rect_t h = { r.x, r.y, CELL_WIDTH, CELL_HEIGHT };
            gfx_drawRectangleFilled(ctx, &h, fg);
        } else if (cell->ch == 0x2584) {
            // lower half block
            gfx_rect_t h = { r.x, r.y + (CELL_HEIGHT/2), CELL_WIDTH, CELL_HEIGHT/2 };
            gfx_drawRectangleFilled(ctx, &h, fg);
        } else if (cell->ch == 0x2580) {
            // upper half block
            gfx_rect_t h = { r.x, r.y, CELL_WIDTH, CELL_HEIGHT/2 };
            gfx_drawRectangleFilled(ctx, &h, fg);
        } else if (cell->ch >= 0x2589 && cell->ch <= 0x258F) {
            // left partial block
            int wide = (r.width * (8 - (cell->ch - 0x2588))) / 8;
            gfx_rect_t h = { r.x, r.y, wide, r.height};
            gfx_drawRectangleFilled(ctx, &h, fg);
        } else {
            // shade block
            gfx_renderCharacter(ctx, f, cell->ch, r.x, r.y+13, fg);
            last_rendered = cell;
        }
    } else {
        gfx_renderCharacter(ctx, f, cell->ch, r.x, r.y+13, fg);
        last_rendered = cell;
    }
}

/**
 * @brief Re-render the terminal
 */
void terminal_render() {
    for (int y = 0; y < terminal_height; y++) {
        for (int x = 0; x < terminal_width; x++) {
            render_cell(&cell_array[y*terminal_width+x], x, y);
        }
    }

    gfx_render(ctx);
    if (win) celestial_flip(win);
}

/**
 * @brief Redraw a cell 
 * @param x The X coordinate of the cell
 * @param y The Y coordinate of the cell
 */
static inline void draw_cell(uint16_t x, uint16_t y) {
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
void terminal_setbg(gfx_color_t bg) { terminal_bg = (bg & ~0xff000000) | (0xf2 << 24); }

/**
 * @brief Terminal backspace method
 */
static void terminal_backspace() {
    if (cursor_x == 0) return;

    UNHIGHLIGHT_CURSOR();
    cursor_x--;
    cursor_hold = 0;
    HIGHLIGHT_CURSOR();
}

/**
 * @brief Duplicate a row
 */
term_cell_t *terminal_duplicateRow(int y) {
    term_cell_t *out = malloc(sizeof(term_cell_t) * terminal_width);
    memcpy(out, CELL(0, y), sizeof(term_cell_t) * terminal_width);
    return out;
}

void terminal_scrollOne() {
    // Add a new row into the scrollback up
    term_cell_t *sb_row = terminal_duplicateRow(0);
    list_append(scrollback_up, sb_row);
    sb_row_count_total++;

    // Starting from here to terminal_height-1, we must adjust contents
    memmove(&cell_array[0], &cell_array[terminal_width], sizeof(term_cell_t) * terminal_width * (terminal_height-1));
    
    // If we need to go down all of them, do that.
    node_t *n = list_pop(scrollback_down);

    // Before we zero, check if we have stuff already.
    if (n) {
        term_cell_t *down_row = (term_cell_t*)n->value;
        free(n);

        memcpy(CELL(0, terminal_height-1), down_row, sizeof(term_cell_t) * terminal_width);
        // for (int x = 0; x < terminal_width; x++) {
        //     memcpy(CELL(x, terminal_height-1), &down_row[x], sizeof(term_cell_t));
        //     draw_cell(x, terminal_height-1);
        // }

        free(down_row);
    } else  {
        for (int x = 0; x < terminal_width; x++) {
            CELL(x, terminal_height-1)->ch = ' ';
            CELL(x, terminal_height-1)->highlighted = 0;
            CELL(x, terminal_height-1)->fg = terminal_fg;
            CELL(x, terminal_height-1)->bg = terminal_bg;
        }
    }
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
        memcpy(&cell_array[(y+1)*terminal_width], &cell_array[y*terminal_width], sizeof(term_cell_t) * terminal_width);
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

void mark_drag(uint16_t end_x, uint16_t end_y, int highlight);

/**
 * @brief UTF decoder
 */
static int terminal_decodeCodepoint(unsigned char ch, uint32_t *cp_out) {
    static int utf_state = 0;
    static uint32_t utf_codepoint = 0;

    if (utf_state == 0) {
        if (ch < 0x80) {
            *cp_out = (uint32_t)ch;
            return 1;
        } else if ((ch & 0xE0) == 0xC0) {
            utf_codepoint = ch & 0x1F;
            utf_state = 1;
            return 0;
        } else if ((ch & 0xF0) == 0xE0) {
            utf_codepoint = ch & 0x0F;
            utf_state = 2;
            return 0;
        } else if ((ch & 0xF8) == 0xF0) {
            utf_codepoint = ch & 0x07;
            utf_state = 3;
            return 0;
        } else {
            *cp_out = (uint32_t)'?';
            return 1;
        }
    } else {
        if ((ch & 0xC0) == 0x80) {
            utf_codepoint = (utf_codepoint << 6) | (ch & 0x3F);
            utf_state--;
            if (utf_state == 0) {
                
                if (utf_codepoint > 0x10FFFF || (utf_codepoint >= 0xD800 && utf_codepoint <= 0xDFFF)) {
                    *cp_out = '?';
                } else {
                    *cp_out = utf_codepoint;
                }

                return 1;
            } else {
                return 0;
            }
        } else {
            utf_state = 0;
            *cp_out = '?';
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Write character to the terminal
 * @param ch_in The character to write to the terminal
 */
static void terminal_write(char ch_in) {
    uint32_t ch;
    if (terminal_decodeCodepoint(ch_in, &ch) == 0) {
        return; // need more input
    }
    
    if (is_dragging) {
        is_dragging = 0;
        mark_drag(drag_last_x, drag_last_y, 0);
        HIGHLIGHT_CURSOR();
        draw_cell(cursor_x, cursor_y);
    }

    terminal_scroll(-1, 0);
    UNHIGHLIGHT_CURSOR();

    // Handle special characters
    if (ch == '\n') {
        // Reset Y and X
        cursor_y++;
        cursor_x = 0;
        cursor_hold = 0;

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
        cursor_hold = 0;
        cursor_x = 0;
        return;
    }


    if (cursor_hold) {
        cursor_x = 0;
        cursor_y++;
        cursor_hold = 0;

        // make sure it hasn't overflowed yet
        if (cursor_y >= terminal_height) {
            terminal_scroll(1, 0);
            cursor_y--;
            cursor_x = 0;
        }
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
        // clamp
        cursor_x = terminal_width-1;
        cursor_hold = 1;
    }

    if (cursor_y >= terminal_height) {
        // We need to scroll the terminal
        terminal_scroll(1, 0);
        cursor_y--;
        cursor_x = 0;
    }

    if (cursor_x < terminal_width && cursor_y < terminal_height) {
        HIGHLIGHT_CURSOR();
        draw_cell(cursor_x, cursor_y);
    }
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

    child_pid = fork();
    if (!child_pid) {
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
    struct writer_data *data = malloc(sizeof(struct writer_data));
    strncpy(data->data, input, 256);
    data->size = strlen(input);
    assert(data->size < 256);

    pthread_mutex_lock(&writer_mutex);
    queue_push(&writer_queue, data);
    pthread_cond_signal(&writer_cond);
    pthread_mutex_unlock(&writer_mutex);
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
            char b[] = {event->ascii, 0};

            terminal_sendInput(b);
            break;
    }
}

/**
 * @brief Celestial keyboard event handler
 * @param win The window the event happened in
 * @param event_type The type of event
 * @param event The event
 */
void kbd_handler(window_t *win, uint32_t event_type, void *event) {
    celestial_event_key_t *key = (celestial_event_key_t*)event;
    keyboard_event_t *ev = keyboard_event(kbd, &key->ev);
                
    if (ev->type == KEYBOARD_EVENT_PRESS) {
        if (ev->ascii == '\b') ev->ascii = 0x7F;
        terminal_process(ev);
    }
    
    free(ev);
}

/**
 * @brief Celestial mouse enter/exit handler
 * @param win The window the event happened in
 * @param event_type The type of event
 * @param event The event
 */
void mouse_cursor_set(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_MOUSE_ENTER) {
        celestial_setMouseCursor(CELESTIAL_MOUSE_TEXT);
    } else {
        if (event_type == CELESTIAL_EVENT_MOUSE_EXIT) {
            // hack
            is_dragging = 2;
        }

        celestial_setMouseCursor(CELESTIAL_MOUSE_DEFAULT);
    }
}

/**
 * @brief Celestial mouse scroll event handler
 * @param win The window the event happened in
 * @param event_type The type of event
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
 * @brief Mark drag
 */
void mark_drag(uint16_t end_x, uint16_t end_y, int highlight) {
    int direction = 0; // 0 = backwards, 1 = forwards

    if (end_y > drag_start_y || end_y < drag_start_y) {
        // One Y is different
        if (end_y > drag_start_y) direction = 1;
        else direction = 0;
    } else {
        // Both Ys are equal
        direction = (end_x > drag_start_x) ? 1 : 0;
    }

    uint16_t cx = drag_start_x;
    uint16_t cy = drag_start_y;
    while (cx != end_x || cy != end_y) {
        if (highlight) {
            HIGHLIGHT(CELL(cx, cy));
        } else {
            UNHIGHLIGHT(CELL(cx, cy));
        }
        draw_cell(cx, cy);

        if (direction) {
            cx++;
            if (cx >= terminal_width) {
                cx = 0;
                cy++;
            }
        } else {
            if (cx == 0) {
                cx = terminal_width-1;
                cy--;
            } else {
                cx--;
            }
        }

    }
}

/**
 * @brief Celestial mouse drag event handler
 * @param win The window the event happened in
 * @param event_type The type of event
 * @param event The event
 */
void mouse_drag_handler(window_t *win, uint32_t event_type, void *event) {
    if (is_dragging == 2) {
        is_dragging = 0;
        mark_drag(drag_last_x, drag_last_y, 0);
        HIGHLIGHT_CURSOR();
        draw_cell(cursor_x, cursor_y);
        FLUSH();
    }

    if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_DOWN) {
        celestial_event_mouse_button_down_t *down = (celestial_event_mouse_button_down_t *)event;

        drag_start_x = down->x / CELL_WIDTH;
        drag_start_y = down->y / CELL_HEIGHT;
        drag_last_x = drag_start_x;
        drag_last_y = drag_start_y;
        return;;
    } else if (event_type == CELESTIAL_EVENT_MOUSE_BUTTON_UP) {
        is_dragging = 2; // limbo state, next event will kill it
        return;
    }
    
    celestial_event_mouse_drag_t *drag = (celestial_event_mouse_drag_t*)event;
    if (!is_dragging) {
        is_dragging = 1;
    }

    uint16_t drag_x = drag->x / CELL_WIDTH;
    uint16_t drag_y = drag->y / CELL_HEIGHT;
    mark_drag(drag_last_x, drag_last_y, 0);
    mark_drag(drag_x, drag_y, 1);
    drag_last_x = drag_x;
    drag_last_y = drag_y;

    FLUSH();
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
            CELL(x, y)->bg = terminal_bg;
            CELL(x, y)->fg = terminal_fg;
        }
    }

    // Redraw cursor
    CURSOR->highlighted = 1;
    draw_cell(cursor_x, cursor_y);
    
    // Flush scrollback buffers
    list_destroy(scrollback_up, true);
    list_destroy(scrollback_down, true);
    scrollback_up = list_create("term scrollback up");
    scrollback_down = list_create("term scrollback down");
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
}

/**
 * @brief Scroll method
 */
void terminal_scrollAnsi(int lines) {
    fprintf(stderr, "termemu: Cannot scroll lines as this is not implemented\n");
}

/**
 * @brief Set cursor
 */
void terminal_setCursorEnabled(bool enabled) {
    cursor_enabled = enabled;
    if (cursor_enabled) HIGHLIGHT(CURSOR);
    if (!cursor_enabled) UNHIGHLIGHT(CURSOR);
}

/**
 * @brief input thread
 */
void *input_thread(void *p) {
    pthread_mutex_lock(&writer_mutex);
    while (1) {
        while (!queue_empty(&writer_queue)) {
            struct writer_data *dat = queue_pop(&writer_queue);
            if (dat) {
                pthread_mutex_unlock(&writer_mutex);
                write(pty_master, dat->data, dat->size);
                pthread_mutex_lock(&writer_mutex);
                free(dat);
            }
        }
        pthread_cond_wait(&writer_cond, &writer_mutex);
    }
}

/**
 * @brief close handler
 */
static void close_handler(window_t *win, uint32_t event_type, void *event) {
    kill(child_pid, SIGKILL);
    die = true;
}

/**
 * @brief Check for child exit
 */
void check_child_exit() {
    int w = waitpid(child_pid, NULL, WNOHANG);
    if (w != child_pid) return;

    die = true;
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

    // Initialize
    QUEUE_INIT(&writer_queue);

    if (fullscreen) {
        // Initialize the graphics context
        ctx = gfx_createFullscreen(CTX_DEFAULT);
    } else {
        // Initialize the graphics context for celestial
        wid_t wid = celestial_createWindow(0x40, 82*CELL_WIDTH, 30*CELL_HEIGHT);
        win = celestial_getWindow(wid);
        celestial_setTitle(win, "Terminal");
        celestial_setHandler(win, CELESTIAL_EVENT_KEY_EVENT, kbd_handler);
        celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_SCROLL, scroll_handler);
        celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_ENTER, mouse_cursor_set);
        celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_EXIT, mouse_cursor_set);
        celestial_setHandler(win, CELESTIAL_EVENT_UNFOCUSED, mouse_cursor_set);
        // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_DRAG, mouse_drag_handler);
        // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_BUTTON_DOWN, mouse_drag_handler);
        // celestial_setHandler(win, CELESTIAL_EVENT_MOUSE_BUTTON_UP, mouse_drag_handler);
        celestial_setHandler(win, CELESTIAL_EVENT_WINDOW_CLOSE, close_handler);
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
    terminal_ansi->set_cursor = terminal_setCursorEnabled;
    terminal_ansi->screen_width = terminal_width;
    terminal_ansi->screen_height = terminal_height;


    // Create cell array
    cell_array = malloc(terminal_height * terminal_width * sizeof(term_cell_t));
    for (int i = 0; i < terminal_height; i++) {
        for (int x = 0; x < terminal_width; x++) {
            term_cell_t *cell = CELL(x, i);
            cell->fg = terminal_fg;
            cell->bg = terminal_bg;
            cell->ch = ' ';
        }
    }

    // Create lists
    scrollback_up = list_create("term scrollback up");
    scrollback_down = list_create("term scrollback down");

    // Draw cursor
    HIGHLIGHT_CURSOR();
    draw_cell(cursor_x, cursor_y);

    // Create keyboard fd
    if (fullscreen) {
        keyboard_fd = open("/device/keyboard", O_RDONLY);
        if (keyboard_fd < 0) {
            perror("open");
            return 1;
        }
    }
    // Create keyboard object
    kbd = keyboard_create();

    // Create the PTY
    if (argc-optind) {
        terminal_createPTY(argv[optind]);
    } else {
        terminal_createPTY("essence");
    }

    // Spawn input thread
    pthread_create(&writer, NULL, input_thread, NULL);
    
    // Enter main loop
    // The idea for this mechanism of redraw came from my research of ToaruOS
    int tty_last = 0;
    int flip_now = 0;
    while (!die) {
        flip_now = 0;

        // Get events
        int p;
        struct pollfd fds[] = {
            { .fd = fullscreen ? keyboard_fd : celestial_getSocketFile(), .events = POLLIN, .revents = 0 },
            { .fd = pty_master, .events = POLLIN, .revents = 0 }
        };

        p = poll(fds, 2, -1);
        if (p < 0) return 1;
        
        // child?
        check_child_exit();

        // Keyboard events?
        if (fds[0].revents & POLLIN) {
            if (fullscreen) {
                key_event_t evp;
                ssize_t r = read(keyboard_fd, &evp, sizeof(key_event_t));
                if (r != sizeof(key_event_t)) continue;

                keyboard_event_t *ev = keyboard_event(kbd, &evp);
                
                if (ev && ev->type == KEYBOARD_EVENT_PRESS) {
                    if (ev->ascii == '\b') ev->ascii = 0x7F;
                    terminal_process(ev);
                }
                    
                free(ev);
            } else {
                celestial_poll();
            }
        }

        // Check for data on PTY
        if (fds[1].revents & POLLIN) {
            char buf[4097];
            ssize_t r = read(pty_master, buf, 4096);
            if (r > 0) {
                buf[r] = 0;
                
                for (int i = 0; i < r; i++) {
                    ansi_parse(terminal_ansi, buf[i]);
                }
            }

            tty_last = 1;
        } else {
            if (tty_last) flip_now = 1;
            tty_last = 0;
        }

        terminal_render();
    }

    return 0;
}
