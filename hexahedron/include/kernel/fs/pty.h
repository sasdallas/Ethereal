/**
 * @file hexahedron/include/kernel/fs/pty.h
 * @brief Psuedoteletype driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_PTY_H
#define KERNEL_FS_PTY_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <termios.h>
#include <structs/circbuf.h>

/**** DEFINITIONS ****/

#define PTY_BUFFER_SIZE     4096
#define PTY_DIRECTORY       "/device/pts/"
#define TTY_DIRECTORY       "/device/tty/"

/* Defaults */
#define PTY_IFLAG_DEFAULT   ICRNL | BRKINT  // Map CR to newline on input and SIGINT on break
#define PTY_OFLAG_DEFAULT   ONLCR | OPOST   // Map NL to CRNL
#define PTY_LFLAG_DEFAULT   ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN
#define PTY_CFLAG_DEFAULT   CREAD | CS8 | B38400

/* PTY size default */
#define PTY_WS_ROW_DEFAULT  25
#define PTY_WS_COL_DEFAULT  80

/**** MACROS ****/

#define CFLAG(flag) ((pty->tios.c_cflag & flag) == flag)
#define LFLAG(flag) ((pty->tios.c_lflag & flag) == flag)
#define OFLAG(flag) ((pty->tios.c_oflag & flag) == flag)
#define IFLAG(flag) ((pty->tios.c_iflag & flag) == flag)
#define CC(idx) (pty->tios.c_cc[idx])

/**** TYPES ****/

struct pty;

/**
 * @brief PTY write output method
 * @param pty The PTY to write out to
 * @param ch The character to write out
 * @returns 0 on success
 */
typedef int (*pty_write_t)(struct pty *pty, char ch);

/**
 * @brief PTY structure
 */
typedef struct pty {
    int number;                 // PTY number (/device/pts/XX)
    struct termios tios;        // Termios
    struct winsize size;        // Widnow size

    fs_node_t *master;          // Master device
    fs_node_t *slave;           // Slave device
    
    // Canonical buffer
    char *canonical_buffer;     // Canonical buffer
    int canonical_idx;          // Index in canonical buffer
    size_t canonical_bufsz;     // Canonical buffer size

    // Buffers
    circbuf_t *in;              // Input buffer
    circbuf_t *out;             // Output buffer

    // Functions
    pty_write_t write_in;       // Input write method (stdin)
    pty_write_t write_out;      // Output write method (stdout)

    void *_impl;                // Implementation-defined
} pty_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the PTY system
 */
void pty_init();

/**
 * @brief Create a new PTY device
 * @param tios Optional presetup termios. Leave as NULL to use defaults
 * @param size TTY window size
 * @param index The index of the TTY. If -1, an index will be auto assigned.
 * @returns PTY or NULL on failure
 */
pty_t *pty_create(struct termios *tios, struct winsize *size, int index);

#endif