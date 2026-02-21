/**
 * @file hexahedron/fs/tty.c
 * @brief TTY (teletype) and PTY (psuedo-teletype) driver 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/tty.h>
#include <kernel/init.h>
#include <kernel/fs/devfs.h>
#include <kernel/debug.h>
#include <ctype.h>
#include <asm/ioctls.h>
#include <sys/ioctl_ethereal.h>
#include <kernel/task/syscall.h>
#include <kernel/task/signal.h>
#include <kernel/processor_data.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:TTY", __VA_ARGS__)

/* Device filesystem directories */
static devfs_node_t *pts_dir = NULL;

/* Numbers */
static atomic_int pty_num = 1;
static hashmap_t *pty_map;

/* Control helpers */
#define IS_CONTROL(ch) ((ch) < 0x20 || (ch) == 0x7F)
#define TO_CTRL(ch) (('@' + (ch)) % 128)

/* PTY device operations */
static ssize_t tty_read(devfs_node_t *file, loff_t off, size_t size, char *buffer);
static ssize_t tty_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer);
static int tty_ioctl(devfs_node_t *file, unsigned long request, void *argp);
static int tty_poll(devfs_node_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t tty_poll_events(devfs_node_t *n);

static ssize_t pty_master_read(devfs_node_t *file, loff_t off, size_t size, char *buffer);
static ssize_t pty_master_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer);
static int pty_master_ioctl(devfs_node_t *file, unsigned long request, void *argp);
static int pty_master_poll(devfs_node_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t pty_master_poll_events(devfs_node_t *n);

static devfs_ops_t tty_ops = {
    .open = NULL,
    .close = NULL,
    .read = tty_read,
    .write = tty_write,
    .ioctl = tty_ioctl,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll = tty_poll,
    .poll_events = tty_poll_events
};

static devfs_ops_t pty_master_ops = {
    .open = NULL,
    .close = NULL,
    .read = pty_master_read,
    .write = pty_master_write,
    .ioctl = pty_master_ioctl,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll = pty_master_poll,
    .poll_events = pty_master_poll_events,
};

/**
 * @brief tty read
 */
static ssize_t tty_read(devfs_node_t *file, loff_t off, size_t size, char *buffer) {
    tty_t *tty = file->priv;

    if (tty->tios.c_lflag & ICANON || tty->tios.c_cc[VMIN] == 0) {
        return circbuf_read(tty->read_buf, size, (uint8_t*)buffer);
    } else {
        size_t sz_to_read = size;
        if (tty->tios.c_cc[VMIN] < sz_to_read) sz_to_read = tty->tios.c_cc[VMIN];

        for (size_t i = 0; i < sz_to_read; i++) {
            circbuf_read(tty->read_buf, 1, (uint8_t*)buffer+i);
        }

        return sz_to_read;
    }

    return size;
}

/**
 * @brief tty write
 */
static ssize_t tty_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer) {
    tty_t *tty = file->priv;

    // Process the TTY write
    size_t written = 0;
    while (written < size) {
        char ch = buffer[written];

        // Is output processing enabled?
        if (tty->tios.c_oflag & OPOST) {
            // Yeah, handle it.
            if (tty->tios.c_oflag & OLCUC ) ch = toupper(ch);
            if (tty->tios.c_oflag & ONLCR && ch == '\n') {
                tty->write(tty, "\r\n", 2);
                written++;
                continue;
            }

            if (tty->tios.c_oflag & OCRNL && ch == '\r') {
                tty->write(tty, "\r\n", 2);
                written++;
                continue;
            }
        }

        tty->write(tty, &((char*)buffer)[written], 1);
        written++;
    }

    return written;
}

/**
 * @brief flush tty
 */
void tty_flush(tty_t *tty) {
    if (tty->canon_idx) {
        LOG(DEBUG, "tty_flush\n");
        mutex_acquire(&tty->mut);
        circbuf_write(tty->read_buf, tty->canon_idx, (uint8_t*)tty->canon_buffer);
        tty->canon_idx = 0;
        tty->canon_buffer[0] = 0;
        poll_signal(&tty->event, POLLIN);
        mutex_release(&tty->mut);
    }
}

/**
 * @brief tty process input character
 */
void tty_handle(tty_t *tty, char ch) {
    // Handle CRNL shenanigans
    if (ch == '\r' && (tty->tios.c_iflag & IGNCR)) {
        return;
    } else if (ch == '\n' && (tty->tios.c_iflag & INLCR)) {
        ch = '\r';
    } else if (ch == '\r' && (tty->tios.c_iflag & ICRNL)) {
        ch = '\n';
    }

    if (tty->tios.c_lflag & ISIG) {
        // Process signals
        int sig = -1;
        if (ch == tty->tios.c_cc[VINTR]) {
            sig = SIGINT;
        } else if (ch == tty->tios.c_cc[VQUIT]) {
            sig = SIGQUIT;
        } else if (ch == tty->tios.c_cc[VSUSP]) {
            sig = SIGTSTP;
        }

        if (sig != -1) {
            if (tty->fg_proc) signal_sendGroup(tty->fg_proc, sig);
            if (tty->tios.c_lflag & ECHO) {
                char ctrl[2] = { '^', TO_CTRL(ch) };
                tty->write(tty, ctrl, 2);
            }
            return;
        }
    }

    if (tty->tios.c_lflag & ICANON) {
        int flush_tty = 0;
    
        if (ch == tty->tios.c_cc[VERASE]) {
            // Do canonical backspace
            if (tty->canon_idx) {
                tty->canon_idx--;
                char prev = tty->canon_buffer[tty->canon_idx];
                tty->canon_buffer[tty->canon_idx] = 0;
                if ((tty->tios.c_lflag & ECHO) && (tty->tios.c_lflag & ECHOE)) {
                    tty->write(tty, "\010 \010", 3);
                    if (IS_CONTROL(prev)) tty->write(tty, "\010 \010", 3);
                }
            }

            if ((tty->tios.c_lflag & ECHO) && ((tty->tios.c_lflag & ECHOE) == 0)) {
                tty->write(tty, "^", 1);
                char control = TO_CTRL(ch);
                tty->write(tty, &control, 1);
            }
            

            return;
        } else if (ch == tty->tios.c_cc[VEOF]) {
            flush_tty = 1;
        } else if ((tty->tios.c_cc[VEOL] && ch == tty->tios.c_cc[VEOL])) {
            tty_flush(tty);
            return;
        } else {
            // TODO: the rest (VKILL )
        }

        // Store in buffer
        tty->canon_buffer[tty->canon_idx++] = ch;
        if (tty->canon_idx >= 4096) flush_tty = 1;

        if (ch == '\n') flush_tty = 1;

        // Write the character if echoed
        if ((tty->tios.c_lflag & ECHO)) {
            if (IS_CONTROL(ch) && ch != '\n') {
                char ctrl[2] = {'^', TO_CTRL(ch) };
                tty->write(tty, ctrl, 2);
            } else {
                tty->write(tty, &ch, 1);
            }
        }

        // If newline then flush it
        if (flush_tty) {
            tty_flush(tty);
        }
    } else {
        if (tty->tios.c_lflag & ECHO) tty->write(tty, &ch, 1);

        mutex_acquire(&tty->mut);
        circbuf_write(tty->read_buf, 1, (uint8_t*)&ch);
        poll_signal(&tty->event, POLLIN);
        mutex_release(&tty->mut);
    }
}

/**
 * @brief tty poll
 */
static int tty_poll(devfs_node_t *file, poll_waiter_t *waiter, poll_events_t events) {
    tty_t *tty = file->priv;
    return poll_add(waiter, &tty->event, events);
}

/**
 * @brief tty poll events
 */
static poll_events_t tty_poll_events(devfs_node_t *n) {
    tty_t *tty = n->priv;
    return POLLOUT | (circbuf_remaining_read(tty->read_buf));
}

/**
 * @brief tty ioctl internal
 */
static int __tty_ioctl(tty_t *tty, unsigned long request, void *argp) {

    switch (request) {
        case IOCTLTTYIS:
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = 1;
            return 0;

        case IOCTLTTYNAME:
            SYSCALL_VALIDATE_PTR_SIZE(argp, strlen(tty->name) + 8);
            snprintf(argp, strlen(tty->name) + 8, "/device/%s", tty->name); // !!!: bad
            return 0;

        case IOCTLTTYLOGIN:
            assert(0 && "unimpl");
            return 0;

        case TIOCGWINSZ:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(argp, &tty->winsz, sizeof(struct winsize));
            return 0;

        case TIOCSWINSZ:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(&tty->winsz, argp, sizeof(struct winsize));
            // TODO: send sigwinch
            return 0;

        case TIOCSCTTY:
            if (current_cpu->current_process->pid == current_cpu->current_process->pgid) {
                if (tty->control_proc == current_cpu->current_process->pid) return 0; // already controlling
                
                if (tty->control_proc) {
                    // Can't steal control unless we are root and argp is 1
                    if (!((uintptr_t)argp == 1 && PROC_IS_ROOT(current_cpu->current_process))) {
                        return -EPERM;
                    } 
                }

                tty->control_proc = current_cpu->current_process->pid;
                return 0;
            } else {
                return -EPERM; // must be leader
            }

        case TIOCNOTTY:
            assert(0 && "TIOCNOTTY");
            break;

        case TIOCGPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            *(int*)argp = tty->fg_proc;
            return 0;

        case TIOCSPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            tty->fg_proc = *(int*)argp;
            return 0;

        case TCSETS:
        case TCSETSW:
        case TCSETSF: // TODO: parse this differently
            SYSCALL_VALIDATE_PTR(argp);
            struct termios *tios_new = (struct termios *)argp;

            if (((tios_new->c_lflag & ICANON) == 0) && tty->tios.c_lflag & ICANON) {
                tty_flush(tty);
            }

            memcpy(&tty->tios, tios_new, sizeof(struct termios));
            if (tty->fill_tios) tty->fill_tios(tty, &tty->tios);
            return 0;

        case TCGETS:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(argp, &tty->tios, sizeof(struct termios));
            return 0;

        default:
            LOG(WARN, "Unsupported IOCTL: 0x%x\n", request);
            return -ENOSYS;
    }
}

/**
 * @brief tty ioctl
 */
static int tty_ioctl(devfs_node_t *file, unsigned long request, void *argp) {
    tty_t *tty = file->priv;
    return __tty_ioctl(tty, request, argp);
}

/**
 * @brief pty master read
 */
static ssize_t pty_master_read(devfs_node_t *file, loff_t off, size_t size, char *buffer) {
    pty_t *pty = file->priv;
    return circbuf_read(pty->out, size, (uint8_t*)buffer);
}

/**
 * @brief pty master write
 */
static ssize_t pty_master_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer) {
    pty_t *pty = file->priv;
    for (size_t i = 0; i < size; i++) {
        tty_handle(pty->slave, buffer[i]);
    }

    return (ssize_t)size;
}

/**
 * @brief pty master ioctl
 */
static int pty_master_ioctl(devfs_node_t *file, unsigned long request, void *argp) {
    pty_t *pty = file->priv;
    return __tty_ioctl(pty->slave, request, argp);
}


/**
 * @brief pty master poll
 */
static int pty_master_poll(devfs_node_t *n, poll_waiter_t *waiter, poll_events_t events) {
    pty_t *pty = (pty_t*)n->priv;
    return poll_add(waiter, &pty->out_event, events);
}

/**
 * @brief pty master poll events
 */
static poll_events_t pty_master_poll_events(devfs_node_t *n) {
    pty_t *pty = (pty_t*)n->priv;
    poll_events_t ret =  POLLOUT | (circbuf_remaining_read(pty->out) ? POLLIN : 0);
    return ret;
}

/**
 * @brief pty slave write
 */
int pty_slave_write(tty_t *tty, char *buffer, size_t size) {
    pty_t *pty = tty->priv;
    int r = (int)circbuf_write(pty->out, size, (uint8_t*)buffer);
    if (r >= 0) poll_signal(&pty->out_event, POLLIN);
    return r;
}

/**
 * @brief Create a TTY
 * @param name The name of the TTY
 */
tty_t *tty_create(char *name) {
    tty_t *tty = kmalloc(sizeof(tty_t));
    memset(tty, 0, sizeof(tty_t));
    MUTEX_INIT(&tty->mut);
    POLL_EVENT_INIT(&tty->event);
    tty->name = strdup(name);
    tty->read_buf = circbuf_create("tty input buffer", 4096);
    tty->tios.c_iflag = ICRNL | BRKINT | ISIG;
    tty->tios.c_oflag = ONLCR | OPOST;
    tty->tios.c_lflag = ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN;
    tty->tios.c_cflag = CREAD | CS8 | B9600;
    tty->tios.c_cc[VEOF] = 4;
    tty->tios.c_cc[VEOL] = 0;
    tty->tios.c_cc[VERASE] = 0x7f;
    tty->tios.c_cc[VINTR] = 3;
    tty->tios.c_cc[VKILL] = 21;
    tty->tios.c_cc[VMIN] = 1;
    tty->tios.c_cc[VQUIT] = 28;
    tty->tios.c_cc[VSTOP] = 19;
    tty->tios.c_cc[VSUSP] = 26;

    tty->canon_buffer = kmalloc(4096);
    tty->canon_idx = 0;

    tty->winsz.ws_row = 25;
    tty->winsz.ws_col = 80;
    tty->fill_tios = NULL;
    tty->write = NULL;

    if (devfs_register(devfs_root, name, VFS_CHARDEVICE, &tty_ops, DEVFS_MAJOR_TTY, 0, tty) == NULL) {
        // TODO: destroy TTY
        return NULL;
    }

    return tty;
}

/**
 * @brief Create a PTY
 */
int pty_create(pty_t **out, vfs_file_t **master, vfs_file_t **slave) {
    pty_t *pty = kmalloc(sizeof(pty_t));
    memset(pty, 0, sizeof(pty_t));
    POLL_EVENT_INIT(&pty->out_event);

    // get new pty num
    long num = atomic_fetch_add(&pty_num, 1);

    // create the slave
    char tmp[64];
    snprintf(tmp, 64, "pts/%d", num);
    pty->slave = tty_create(tmp);
    pty->slave->write = pty_slave_write;
    pty->slave->priv = pty;
    pty->out = circbuf_create("pty output buffer", 4096);

    // now set parameters
    snprintf(tmp, 64, "/device/pts/%d", num);
    if (slave) {
        int r = vfs_open(tmp, O_RDWR, slave);
        if (r) {
            // TODO: cleanup PTY
            return r;
        }
    }

    // Build the master
    snprintf(tmp, 64, ".ptmaster%d", num);
    if (!devfs_register(devfs_root, tmp, VFS_CHARDEVICE, &pty_master_ops, DEVFS_MAJOR_TTY, num, pty)) {
        // TODO: Cleanup pty
        return -ENOMEM;
    }

    snprintf(tmp, 64, "/device/.ptmaster%d", num);
    if (master) {
        int r = vfs_open(tmp, O_RDWR, master);
        if (r) {
            // TODO: cleanup PTY
            return r;
        }
    }

    hashmap_set(pty_map, (void*)(uintptr_t)num, pty);
    if (out) *out = pty;
    return 0;
}

/**
 * @brief TTY init
 */
static int tty_init() {
    // Create the PTY shenanigans
    pty_map = hashmap_create_int("pty map", 4);
    pts_dir = devfs_createDirectory(devfs_root, "pts");
    return !pts_dir;
}

FS_INIT_ROUTINE(tty, INIT_FLAG_DEFAULT, tty_init, devfs);