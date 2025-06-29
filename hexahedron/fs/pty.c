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
#include <kernel/task/syscall.h>
#include <kernel/debug.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <string.h>
#include <termios.h>

/* Last used index for PTY */
static int last_pty_index = 0;

/* Helpers */
#define CTRL(ch) (('@' + ch) % 128)
#define IS_CTRL(ch) (ch < 0x20 || ch == 0x7F)

#define WRITE_IN(ch) pty->write_in(pty, ch)
#define WRITE_OUTPUT(ch) pty->write_out(pty, ch)
#define WRITE_CONTROL(ch) { WRITE_OUTPUT('^'); WRITE_OUTPUT(CTRL(ch)); }
#define WRITE_BKSP() { WRITE_OUTPUT('\010'); WRITE_OUTPUT(' '); WRITE_OUTPUT('\010'); }

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
                int was_control = IS_CTRL(pty->canonical_buffer[pty->canonical_idx]); 
                pty->canonical_buffer[pty->canonical_idx] = 0;
                if (LFLAG(ECHO) && LFLAG(ECHOE)) {
                    WRITE_BKSP();
                    if (was_control) WRITE_BKSP();
                }
            }


            // If ECHOE wasn't set, we should print out the backspace
            if (LFLAG(ECHO) && !LFLAG(ECHOE)) WRITE_CONTROL(ch);
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
            // Sourced from old reduceOS
            if (IS_CTRL(ch) && ch != '\n') {
                WRITE_CONTROL(ch);
            } else {
                WRITE_OUTPUT(ch);
            }
        }

        if ((CC(VEOL) && ch == CC(VEOL)) || ch == '\n') {
            // If ECHONL is set (and NOT ECHO) we can output the character
            if (!LFLAG(ECHO) && LFLAG(ECHONL)) WRITE_OUTPUT(ch);

            // Dump the buffer
            for (int i = 0; i < pty->canonical_idx; i++) WRITE_IN(pty->canonical_buffer[i]);
            
            // Reset
            pty->canonical_idx = 0;
            pty->canonical_buffer[pty->canonical_idx] = 0;

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

    // Flush input because we wrote to slave input
    if (size && pty->flush_in) pty->flush_in(pty);

    // !!!: Also flush output because of ECHO
    if (size && pty->flush_out) pty->flush_out(pty);

    return size;
}

/**
 * @brief PTY write method for slave
 */
ssize_t pty_writeSlave(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    // Writes to the slave are redirected to the master's stdin
    pty_t *pty = (pty_t*)node->dev;

    for (size_t i = 0; i < size; i++) {
        char ch = buffer[i];

        // Handle the character

        // OPOST
        if (OFLAG(OPOST)) {
            // TODO: Implementation defined
        }

        // ONLCR
        if (OFLAG(ONLCR) && ch == '\n') {
            WRITE_OUTPUT('\r');
            WRITE_OUTPUT('\n');
            continue;
        } 

        // OCRNL
        if (OFLAG(OCRNL) && ch == '\r') {
            WRITE_OUTPUT('\r');
            WRITE_OUTPUT('\n');
            continue;
        }

        // TODO: ONLRET?

        // OLCUC
        if (OFLAG(OLCUC)) {
            // Map lowercase to uppercase
            WRITE_OUTPUT(toupper(ch));
            continue;
        }

        WRITE_OUTPUT(ch);
    }

    if (size && pty->flush_out) pty->flush_out(pty);
 
    return size;
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

    // Check for canonical mode
    if (LFLAG(ICANON) || CC(VMIN) == 0) {
        return circbuf_read(pty->in, size, buffer);
    } else {
        // We need to follow VMIN
        size_t sz_to_read = CC(VMIN);
        if (size < sz_to_read) sz_to_read = size;
        for (size_t i = 0; i < sz_to_read; i++) {
            circbuf_read(pty->in, 1, buffer+i);
        }

        return sz_to_read;
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
 * @brief IOCTL method for a PTY
 * @param node The node to do the ioctl on
 * @param request The IOCTL request
 * @param argp The argument
 */
int pty_ioctl(fs_node_t *node, unsigned long request, void *argp) {
    pty_t *pty = (pty_t*)node->dev;

    switch (request) {
        case IOCTLTTYIS:
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = 1;
            return 0;

        case IOCTLTTYNAME:
            SYSCALL_VALIDATE_PTR(argp);
            pty->name(pty, argp);
            return 0;

        case IOCTLTTYLOGIN:
            // Set the user ID of the user
            SYSCALL_VALIDATE_PTR(argp);
            if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;
            pty->slave->uid = *(uid_t*)argp;
            pty->master->uid = *(uid_t*)argp;
            return 0;

        case TIOCGWINSZ:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(argp, &pty->size, sizeof(struct winsize));
            return 0;

        case TIOCSWINSZ:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(&pty->size, argp, sizeof(struct winsize));
            if (pty->fg_proc) signal_sendGroup(pty->fg_proc, SIGWINCH);
            return 0;

        case TIOCGLCKTRMIOS:
        case TIOCSLCKTRMIOS:
            LOG(WARN, "TIOCGLCKTERMIOS/TIOCSLCKTERMIOS not implemented\n");
            return -EINVAL;

        case TIOCCBRK:
        case TIOCSBRK:
            LOG(WARN, "TIOCCBRK/TIOCSBRK is unimplemented\n");
            return -ENOTSUP;
        
        case TIOCINQ:
            // idk how to do this
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = circbuf_remaining_read(pty->in);
            return 0;

        case TIOCSERGETLSR:
            return -ENOTSUP;

        case TCFLSH:
            if ((uintptr_t)argp & TCIFLUSH) {
                // Flush pending input
                spinlock_acquire(pty->in->lock);
                pty->in->tail = pty->in->head;
                spinlock_release(pty->in->lock);
            }
            
            if ((uintptr_t)argp & TCOFLUSH) {
                // Flush pending output
                spinlock_acquire(pty->out->lock);
                pty->out->tail = pty->out->head;
                spinlock_release(pty->out->lock);
            }

            return 0;

        case TIOCSTI:
            SYSCALL_VALIDATE_PTR(argp);
            WRITE_OUTPUT(*(char*)argp);
            return 0;
        
        case TIOCCONS:
            // TODO
            LOG(DEBUG, "WARNING: Need to redirect /device/tty0\n");
            return -ENOTSUP;

        case TIOCSCTTY:
            // Set controlling process of this TTY
            if (PROC_IS_LEADER(current_cpu->current_process) && pty->control_proc == current_cpu->current_process->pid) {
                return 0; // Already the controlling process
            } 

            if (!PROC_IS_LEADER(current_cpu->current_process)) return -EPERM;

            // We can't steal control away from a TTY already with a session
            if (pty->control_proc) {
                // Unless we're root and argp is 1
                if (!((uintptr_t)argp == 1 && PROC_IS_ROOT(current_cpu->current_process))) {
                    return -EPERM;
                }
            }

            pty->control_proc = current_cpu->current_process->pid;
            return 0;

        case TIOCNOTTY:
            // Release TTY
            if (pty->control_proc != current_cpu->current_process->pid) return -EPERM;

            // We need to send SIGHUP and SIGCONT to the foreground process group
            signal_sendGroup(pty->fg_proc, SIGHUP);
            signal_sendGroup(pty->fg_proc, SIGCONT);

            pty->control_proc = 0;
            return 0;

        case TIOCGPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = pty->fg_proc;
            return 0;

        case TIOCSPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            pty->fg_proc = *(int*)argp;
            return 0;

        /* TODO: TIOCGSID */

        case TCGETS:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(argp, &pty->tios, sizeof(struct termios));
            return 0;

        case TCSETS:
        case TCSETSW:
            SYSCALL_VALIDATE_PTR(argp);

            // Are we switching out of ICANON?
            if (!(((struct termios*)argp)->c_lflag & ICANON) && pty->tios.c_lflag & ICANON) {
                // Yes, dump input buffer
                for (int i = 0; i < pty->canonical_idx; i++) WRITE_IN(pty->canonical_buffer[i]);
                
                // Reset
                pty->canonical_idx = 0;
                pty->canonical_buffer[pty->canonical_idx] = 0;
                kfree(pty->canonical_buffer);
            }

            // Are we switching *into* canonical?
            if ((((struct termios*)argp)->c_lflag & ICANON) && !(pty->tios.c_lflag & ICANON)) {
                pty->canonical_buffer = kmalloc(PTY_BUFFER_SIZE);
                memset(pty->canonical_buffer, 0, PTY_BUFFER_SIZE);
                pty->canonical_idx = 0;
                pty->canonical_bufsz = PTY_BUFFER_SIZE;
            }

            // TODO: Anything else to flush?
            memcpy(&pty->tios, argp, sizeof(struct termios));
            return 0;

        case TCSETSF:
            // Flush pending input
            spinlock_acquire(pty->in->lock);
            pty->in->tail = pty->in->head;
            spinlock_release(pty->in->lock);

            // TODO: Anything else to flush?
            memcpy(&pty->tios, argp, sizeof(struct termios));
            return 0;

        case TIOCOUTQ:
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = circbuf_remaining_read(pty->out);
            return 0;    
        
        default:
            LOG(ERR, "Unrecognized TTY ioctl: 0x%x\n", request);
            return -EINVAL;
    }
}

/**
 * @brief PTY name
 * @param pty The PTY to use to fill the name in
 * @param name The name pointer
 */
void pty_name(pty_t *pty, char *name) {
    *name = 0;
    snprintf(name, 256, "/device/pts/%zd", pty->number);
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
    pty->name = pty_name;

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
    pty->master->ioctl = pty_ioctl;
    
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
    pty->slave->ioctl = pty_ioctl;

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