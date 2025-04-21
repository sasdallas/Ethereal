/**
 * @file hexahedron/fs/pty.c
 * @brief PTY driver
 * 
 * Psuedoteletype and teletype driver for Hexahedron.
 * PTY devices are mounted to /device/pts/ptyXXX
 * TTY devices are mounted to /device/ttyXX
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/pty.h>
#include <kernel/mem/alloc.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <string.h>
#include <termios.h>

/* Last used index for PTY */
static int last_pty_index = 0;

/* Helpers */
#define WRITE_IN(ch) pty->write_in(pty, ch)
#define WRITE_OUTPUT(ch) pty->write_out(pty, ch)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:PTY", __VA_ARGS__)

/**
 * @brief Default PTY read output (for process input)
 */
int pty_writeIn(pty_t *pty, char ch) {
    uint8_t arr[] = { ch };
    circbuf_write(pty->in, 1, arr);
    return 0;
}

/**
 * @brief Default PTY write output (for process output)
 */
int pty_writeOut(pty_t *pty, char ch) {
    uint8_t arr[] = { ch };
    circbuf_write(pty->out, 1, arr);
    return 0;
}

/**
 * @brief Process input character for a specific PTY, taking tios into account
 * @param pty The PTY to process the character for
 * @param c The character to process
 */
void pty_input(pty_t *pty, uint8_t ch) {
    // Process any signals that we need
    if (LFLAG(ISIG)) {
        // Process signals if ch
        if (ch == CC(VINTR)) {
            LOG(ERR, "UNIMPL: Send SIGINTR\n");
            return;
        }

        if (ch == CC(VQUIT)) {
            LOG(ERR, "UNIMPL: Send SIGQUIT\n");
            return;
        } 

        if (ch == CC(VSUSP)) {
            LOG(ERR, "UNIMPL: Send SIGSUSP\n");
            return;
        }
    }

    // Strip the eighth bit if ISTRIP
    if (IFLAG(ISTRIP)) {
        ch &= 0x7F;
    }

    // Handle CR <-> NL
    if (IFLAG(ICRNL) && ch == '\r') {
        // CR -> NL
        ch = '\n';
    } else if (IFLAG(INLCR) && ch == '\n') {
        // NL -> CR
        ch = '\r';
    }

    // If we are running in canonical mode, handle
    if (LFLAG(ICANON)) {
        // VERASE signals a backspace
        if (ch == CC(VERASE)) {
            // Backspace, so handle that
            if (pty->canonical_idx) {
                // Ok, this is kinda hacky but were basically gonna handle this by
                // going back in the buffer (if there's a control character, we will delete it)
                // and if ECHO & ECHOE are set we actually will erase it
                pty->canonical_idx--;
                
                // Check if it is a control character
                int was_control = (pty->canonical_buffer[pty->canonical_idx] < 0x20 || pty->canonical_buffer[pty->canonical_idx] == 0x7F); 
                pty->canonical_buffer[pty->canonical_idx] = 0;
                if (LFLAG(ECHO) && LFLAG(ECHOE)) {
                    WRITE_OUTPUT('\010');
                    WRITE_OUTPUT(' ');
                    WRITE_OUTPUT('\010');

                    if (was_control) {
                        WRITE_OUTPUT('\010');
                        WRITE_OUTPUT(' ');
                        WRITE_OUTPUT('\010');
                    }
                }
            }


            // If ECHOE wasn't set, we should print out the backsapce
            if (LFLAG(ECHO) && !LFLAG(ECHOE)) {
                WRITE_OUTPUT('^');
                WRITE_OUTPUT(('@' + ch) % 128);
            }
            
            return;
        }

        // VEOF
        if (ch == CC(VEOF)) {
            // Dump it right now
            if (pty->canonical_idx) {
                for (int i = 0; i < pty->canonical_idx; i++) WRITE_IN(pty->canonical_buffer[i]);
                pty->canonical_idx = 0;
            } else {
                // uh, idk.
                LOG(ERR, "VEOF on no content. Writing dummy character\n");
                uint8_t data[] = { 0 };
                circbuf_write(pty->in, 1, data);
            }
            
            return;
        }

        // Check overflow
        if ((size_t)pty->canonical_idx >= pty->canonical_bufsz) {
            // problem
            LOG(ERR, "Buffer content overflow\n");
            return;
        }

        // Set the character
        pty->canonical_buffer[pty->canonical_idx] = ch;
        pty->canonical_idx++;

        if (LFLAG(ECHO)) {
            // Write control character
            if ((ch < 0x20 || ch == 0x7F) && ch != '\n') {
                WRITE_OUTPUT('^');
                WRITE_OUTPUT((ch + '@') % 128);
            } else {
                WRITE_OUTPUT(ch);
            }
        }

        if ((CC(VEOL) && ch == CC(VEOL)) || ch == '\n') {
            // If ECHONL is set (and NOT ECHO) we can output the character
            if (!LFLAG(ECHO) && LFLAG(ECHONL)) WRITE_OUTPUT(ch);

            // Dump the buffer
            for (int i = 0; i < pty->canonical_idx; i++) WRITE_IN(pty->canonical_buffer[i]);
            pty->canonical_idx = 0;

            return;
        }

        return;
    } else {
        // Just write the character
        WRITE_IN(ch);
    }
}

/**
 * @brief PTY write method for master
 */
ssize_t pty_writeMaster(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    // Writes to the master are redirected to the slave's stdin
    pty_t *pty = (pty_t*)node->dev;

    for (size_t i = 0; i < size; i++) {
        pty_input(pty, buffer[i]);
    }

    return size;
}

/**
 * @brief PTY write method for slave
 */
ssize_t pty_writeSlave(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    return 0; // UNIMPL
}

/**
 * @brief PTY read method for master
 */
ssize_t pty_readMaster(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    return 0; // UNIMPL
}

/**
 * @brief PTY read method for slave
 */
ssize_t pty_readSlave(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    pty_t *pty = (pty_t*)node->dev;

    ssize_t read = 0;
    uint8_t *b = buffer;
    for (; (size_t)read < size; read++) {
        while (circbuf_read(pty->in, 1, b)) {
            while (pty->in->head == pty->in->tail) arch_pause();
        }

        if (*b == '\n') return read;

        b++;
    }

    return size;
}


/**
 * @brief Initialize the PTY system
 */
void pty_init() {
    // TODO
}

/**
 * @brief Create a new PTY device
 * @param tios Optional presetup termios. Leave as NULL to use defaults
 * @param size TTY window size
 * @param index The index of the TTY. If -1, an index will be auto assigned.
 * @returns PTY or NULL on failure
 */
pty_t *pty_create(struct termios *tios, struct winsize *size, int index) {
    // Allocate a new pty
    pty_t *pty = kmalloc(sizeof(pty_t));
    memset(pty, 0, sizeof(pty_t));

    // Set number
    pty->number = (index == -1) ? last_pty_index : index;
    last_pty_index++;
    
    // Configure circular buffers
    pty->in = circbuf_create("pty in", PTY_BUFFER_SIZE);
    pty->out = circbuf_create("pty out", PTY_BUFFER_SIZE);

    // Configure methods
    pty->write_in = pty_writeIn;
    pty->write_out = pty_writeOut;

    // Configure master device
    // Master should have its writes sent to stdin with reads coming from stdout
    pty->master = kmalloc(sizeof(fs_node_t));
    memset(pty->master, 0, sizeof(fs_node_t));
    snprintf(pty->master->name, 256, "pts%d", pty->number);
    pty->master->flags = VFS_PIPE;
    pty->master->mask = 0666; // TODO
    pty->master->uid = (current_cpu->current_process ? current_cpu->current_process->uid : 0);
    pty->master->gid = (current_cpu->current_process ? current_cpu->current_process->gid : 0);
    pty->master->ctime = pty->master->atime = pty->master->mtime = now();
    pty->master->dev = (void*)pty;

    pty->master->write = pty_writeMaster;
    pty->master->read = pty_readMaster;

    // Configure slave device
    // Slave should have its writes send to stdout with reads coming from stdin
    pty->slave = kmalloc(sizeof(fs_node_t));
    memset(pty->slave, 0, sizeof(fs_node_t));
    snprintf(pty->slave->name, 256, "tty%d", pty->number);
    pty->slave->flags = VFS_CHARDEVICE;
    pty->slave->mask = 0666; // TODO
    pty->slave->uid = (current_cpu->current_process ? current_cpu->current_process->uid : 0);
    pty->slave->gid = (current_cpu->current_process ? current_cpu->current_process->gid : 0);
    pty->slave->ctime = pty->slave->atime = pty->slave->mtime = now();
    pty->slave->dev = (void*)pty;

    pty->slave->write = pty_writeSlave;
    pty->slave->read = pty_readSlave;

    // Configure termios
    if (tios) {
        memcpy(&pty->tios, tios, sizeof(struct termios));
    } else {
        pty->tios.c_cc[VEOF] = 4;           // ^D
        pty->tios.c_cc[VEOL] = 0;           // Unset
        pty->tios.c_cc[VERASE] = 0x7F;      // ^?
        pty->tios.c_cc[VINTR] = 3;          // ^C 
        pty->tios.c_cc[VKILL] = 21;         // ^U
        pty->tios.c_cc[VMIN] = 1;           // Minimum number of characters for noncanonical reads
        pty->tios.c_cc[VQUIT] = 28;         // ^/
        pty->tios.c_cc[VSTOP] = 19;         // ^S
        pty->tios.c_cc[VSUSP] = 26;         // ^Z

        pty->tios.c_iflag = PTY_IFLAG_DEFAULT;
        pty->tios.c_oflag = PTY_OFLAG_DEFAULT;
        pty->tios.c_lflag = PTY_LFLAG_DEFAULT;
        pty->tios.c_cflag = PTY_CFLAG_DEFAULT;
    }

    if (size) {
        memcpy(&pty->size, size, sizeof(struct winsize));
    } else {
        pty->size.ws_row = PTY_WS_ROW_DEFAULT;
        pty->size.ws_col = PTY_WS_COL_DEFAULT;
    }  

    // Allocate a canonical buffer if required
    if (LFLAG(ICANON)) {
        pty->canonical_buffer = kmalloc(PTY_BUFFER_SIZE);
        memset(pty->canonical_buffer, 0, PTY_BUFFER_SIZE);
        pty->canonical_idx = 0;
        pty->canonical_bufsz = PTY_BUFFER_SIZE;
    }

    return pty;
}