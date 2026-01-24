/**
 * @file hexahedron/include/kernel/fs/tty.h
 * @brief PTY/TTY driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_TTY_H
#define KERNEL_FS_TTY_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs_new.h>
#include <termios.h>
#include <structs/circbuf.h>

/**** TYPES ***/

typedef struct tty {
    char *name;                         // Name of the TTY
    mutex_t mut;                        // Locking mutex
    struct termios tios;                // Termios
    struct winsize winsz;               // Window size
    circbuf_t *read_buf;                // TTY read buffer
    char *canon_buffer;                 // For ICANON
    size_t canon_idx;
    poll_event_t event;                 // Poll event to attach

    pid_t control_proc;
    pid_t fg_proc;

    // User-provided functions
    int (*write)(struct tty *tty, char *buffer, size_t size);
    int (*fill_tios)(struct tty *tty, struct termios *tios);

    // Private
    void *priv;
} tty_t;

typedef struct pty {
    tty_t *slave;                       // Slave TTY
    circbuf_t *out;                     // TTY output buffer
    poll_event_t out_event;             // Output event
} pty_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a TTY
 * @param name The name of the TTY
 */
tty_t *tty_create(char *name);

/**
 * @brief Create a PTY
 */
int pty_create(pty_t **out, vfs_file_t **master, vfs_file_t **slave);

#endif