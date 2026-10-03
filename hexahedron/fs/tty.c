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
#include <kernel/task/process.h>

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
static int tty_open(devfs_node_t *file, unsigned long flags);
static ssize_t tty_read(devfs_node_t *file, loff_t off, size_t size, char *buffer);
static ssize_t tty_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer);
static int tty_ioctl(devfs_node_t *file, unsigned long request, void *argp);
static int tty_poll(devfs_node_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t tty_poll_events(devfs_node_t *n);

static ssize_t pty_master_read(devfs_node_t *file, loff_t off, size_t size, char *buffer, int flags);
static ssize_t pty_master_write(devfs_node_t *file, loff_t off, size_t size, const char *buffer);
static int pty_master_ioctl(devfs_node_t *file, unsigned long request, void *argp);
static int pty_master_poll(devfs_node_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t pty_master_poll_events(devfs_node_t *n);

static devfs_ops_t tty_ops = {
    .open = tty_open,
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
    .read_ext = pty_master_read,
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
 * @brief Helper to read from a TTY's buffer
 */
static ssize_t tty_readBuffer(tty_t *tty, char *buffer, size_t size) {
    mutex_acquire(&tty->mut);
    while (ringbuffer_remaining_read(tty->read_buf) == 0) {
        if (tty->is_nonblocking) {
            mutex_release(&tty->mut);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &tty->event, POLLIN);
        mutex_release(&tty->mut);

        int r = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);
        if (r) return r;

        mutex_acquire(&tty->mut);
    }

    ssize_t r = ringbuffer_read(tty->read_buf, buffer, size);
    if (r > 0) poll_signal(&tty->event, POLLOUT);

    mutex_release(&tty->mut);
    return r;
}

/**
 * @brief Helper to write to TTY buffer
 */
static ssize_t tty_writeBuffer(tty_t *tty, char *buffer, size_t size) {
    size_t written = 0;

    mutex_acquire(&tty->mut);
    while (written < size) {
        if (ringbuffer_remaining_write(tty->read_buf) == 0) {
            poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
            poll_add(w, &tty->event, POLLOUT);
            mutex_release(&tty->mut);

            int r = poll_wait(w, -1);
            poll_exit(w);
            poll_destroyWaiter(w);
            if (r) return written ? (ssize_t)written : r;

            mutex_acquire(&tty->mut);
            continue;
        }

        ssize_t r = ringbuffer_write(tty->read_buf, buffer + written, size - written);
        written += r;
        if (r) poll_signal(&tty->event, POLLIN);
    }

    mutex_release(&tty->mut);
    return written;
}

/**
 * @brief tty open
 */
static int tty_open(devfs_node_t *file, unsigned long flags) {
    process_t *proc = current_cpu->current_process;
    if (!(flags & O_NOCTTY) && proc && !(proc->flags & PROCESS_KERNEL)) {
        session_claimTTY(file->priv, false);
    }
    return 0;
}

/**
 * @brief tty read
 */
static ssize_t tty_read(devfs_node_t *file, loff_t off, size_t size, char *buffer) {
    if (!size) return 0;
    tty_t *tty = file->priv;

    if (tty->tios.c_lflag & ICANON || tty->tios.c_cc[VMIN] == 0) {
        return tty_readBuffer(tty, buffer, size);
    } else {
        size_t sz_to_read = size;
        if (tty->tios.c_cc[VMIN] < sz_to_read) {
            sz_to_read = tty->tios.c_cc[VMIN];
        }

        for (size_t i = 0; i < sz_to_read; i++) {
            ssize_t r = tty_readBuffer(tty, buffer+i, 1);
            if (r < 0) return r;
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

    int e;
    size_t written = 0;
    while (written < size) {
        char ch = buffer[written];

        // Is output processing enabled?
        if (tty->tios.c_oflag & OPOST) {
            if (tty->tios.c_oflag & OLCUC) ch = toupper(ch);
            
            if ((tty->tios.c_oflag & ONLCR) && ch == '\n') {
                e = tty->write(tty, "\r\n", 2);
                if (e < 0) return e;
                written++;
                continue;
            }

            if ((tty->tios.c_oflag & OCRNL) && ch == '\r') {
                e = tty->write(tty, "\n", 1);
                if (e < 0) return e;
                written++;
                continue;
            }
        }

        e = tty->write(tty, &ch, 1);
        if (e < 0) return e;
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
        tty_writeBuffer(tty, tty->canon_buffer, tty->canon_idx);
        mutex_acquire(&tty->mut);
        tty->canon_idx = 0;
        tty->canon_buffer[0] = 0;
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
            if (tty->tios.c_lflag & ECHO) {
                char ctrl[2] = { '^', TO_CTRL(ch) };
                tty->write(tty, ctrl, 2);
            }
            session_signalForeground(tty, sig);
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

        tty_writeBuffer(tty, &ch, 1);
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
    mutex_acquire(&tty->mut);
    poll_events_t ret = POLLOUT | (ringbuffer_remaining_read(tty->read_buf) ? POLLIN : 0);
    mutex_release(&tty->mut);
    return ret;
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
            SYSCALL_VALIDATE_PTR_SIZE(argp, strlen(tty->name) + 9);
            snprintf(argp, strlen(tty->name) + 9, "/device/%s", tty->name); // !!!: bad
            return 0;

        case IOCTLTTYLOGIN:
            // set the uids
            SYSCALL_VALIDATE_PTR(argp);
            if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;
            if (tty->is_pty) {
                pty_t *pty = tty->priv;
                pty->slave->node->attr.uid = *(uid_t*)argp;
                pty->master_node->attr.uid = *(uid_t*)argp;
            }

            return 0;

        case TIOCGWINSZ:
            SYSCALL_VALIDATE_PTR(argp);
            memcpy(argp, &tty->winsz, sizeof(struct winsize));
            return 0;

        case TIOCSWINSZ: {
            SYSCALL_VALIDATE_PTR_SIZE(argp, sizeof(struct winsize));
            struct winsize size = *(struct winsize *)argp;
            mutex_acquire(&tty->mut);
            bool changed = memcmp(&tty->winsz, &size, sizeof(size)) != 0;
            tty->winsz = size;
            mutex_release(&tty->mut);
            if (changed) session_signalForeground(tty, SIGWINCH);
            return 0;
        }

        case TIOCSCTTY:
            return session_claimTTY(tty, (uintptr_t)argp == 1);

        case TIOCNOTTY:
            if (session_tcgetpgrp(current_cpu->current_process, tty) < 0) return -ENOTTY;
            if (session_getsid(current_cpu->current_process) != current_cpu->current_process->pid) return -EPERM;
            session_hangup(tty);
            return 0;

        case TIOCGPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            pid_t pgid = session_tcgetpgrp(current_cpu->current_process, tty);
            if (pgid < 0) return pgid;
            *(pid_t*)argp = pgid;
            return 0;

        case TIOCSPGRP:
            SYSCALL_VALIDATE_PTR(argp);
            return session_tcsetpgrp(current_cpu->current_process, tty, *(pid_t*)argp);

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

        // TODO: Support F_SETFL nonblocking as well
        case FIONBIO:
            SYSCALL_VALIDATE_PTR(argp);
            if (*(char*)argp) {
                tty->is_nonblocking = true;
            } else {
                tty->is_nonblocking = false;
            }
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
static ssize_t pty_master_read(devfs_node_t *file, loff_t off, size_t size, char *buffer, int flags) {
    if (size == 0) return 0;
    pty_t *pty = file->priv;

    mutex_acquire(&pty->mut);

    bool nonblocking = (flags & O_NONBLOCK) || pty->is_nonblocking;
    while (ringbuffer_remaining_read(pty->out) == 0) {
        if (nonblocking) {
            mutex_release(&pty->mut);
            return -EWOULDBLOCK;
        }

        poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
        poll_add(w, &pty->out_event, POLLIN);
        mutex_release(&pty->mut);

        int r = poll_wait(w, -1);
        poll_exit(w);
        poll_destroyWaiter(w);
        if (r) return r;

        mutex_acquire(&pty->mut);
    }

    ssize_t r = ringbuffer_read(pty->out, buffer, size);
    if (r > 0) poll_signal(&pty->out_event, POLLOUT);
    mutex_release(&pty->mut);
    return r;
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

    // TODO: Support F_SETFL O_NONBLOCK (more of a devfs thing though)
    if (request == FIONBIO) {
        SYSCALL_VALIDATE_PTR(argp);
        char a = *(char*)argp;

        pty->is_nonblocking = !!a;
        return 0;
    }

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
    mutex_acquire(&pty->mut);
    poll_events_t ret =  POLLOUT | (ringbuffer_remaining_read(pty->out) ? POLLIN : 0);
    mutex_release(&pty->mut);
    return ret;
}

/**
 * @brief pty slave write
 */
int pty_slave_write(tty_t *tty, char *buffer, size_t size) {
    pty_t *pty = tty->priv;

    size_t written = 0;

    mutex_acquire(&pty->mut);
    while (written < size) {
        if (ringbuffer_remaining_write(pty->out) == 0) {
            poll_waiter_t *w = poll_createWaiter(current_cpu->current_thread, 1);
            poll_add(w, &pty->out_event, POLLOUT);
            mutex_release(&pty->mut);

            int r = poll_wait(w, -1);
            poll_exit(w);
            poll_destroyWaiter(w);
            if (r) return written ? (ssize_t)written : r;

            mutex_acquire(&pty->mut);
            continue;
        }

        ssize_t r = ringbuffer_write(pty->out, buffer + written, size - written);
        written += r;
        if (r) poll_signal(&pty->out_event, POLLIN);
    }

    mutex_release(&pty->mut);
    return written;
}

/**
 * @brief Create a TTY
 * @param name The name of the TTY
 */
tty_t *tty_create(char *name) {
    tty_t *tty = kmalloc(sizeof(tty_t));
    memset(tty, 0, sizeof(tty_t));
    MUTEX_INIT(&tty->mut);
    SPINLOCK_INIT(&tty->job_lock);
    POLL_EVENT_INIT(&tty->event);
    tty->name = strdup(name);
    tty->read_buf = ringbuffer_create(4096);
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

    tty->is_pty = false;

    if ((tty->node = devfs_register(devfs_root, name, VFS_CHARDEVICE, &tty_ops, DEVFS_MAJOR_TTY, 0, tty)) == NULL) {
        // TODO: destroy TTY
        return NULL;
    }

    if (current_cpu->current_process) {
        tty->node->attr.uid = current_cpu->current_process->cred.uid;
        tty->node->attr.gid = current_cpu->current_process->cred.gid;
    }

    return tty;
}

/**
 * @brief Create a PTY
 */
int pty_create(pty_t **out, vfs_file_t **master, vfs_file_t **slave) {
    pty_t *pty = kmalloc(sizeof(pty_t));
    memset(pty, 0, sizeof(pty_t));
    MUTEX_INIT(&pty->mut);
    POLL_EVENT_INIT(&pty->out_event);

    // get new pty num
    long num = atomic_fetch_add(&pty_num, 1);

    // create the slave
    char tmp[64];
    snprintf(tmp, 64, "pts/%d", num);
    pty->slave = tty_create(tmp);
    pty->slave->write = pty_slave_write;
    pty->slave->priv = pty;
    pty->slave->is_pty = true;
    pty->out = ringbuffer_create(4096);

    // now set parameters
    snprintf(tmp, 64, "/device/pts/%d", num);
    if (slave) {
        int r = vfs_open(tmp, O_RDWR | O_NOCTTY, slave);
        if (r) {
            // TODO: cleanup PTY
            return r;
        }
    }

    // Build the master
    snprintf(tmp, 64, ".ptmaster%d", num);
    if ((pty->master_node = devfs_register(devfs_root, tmp, VFS_CHARDEVICE, &pty_master_ops, DEVFS_MAJOR_TTY, num, pty)) == NULL) {
        // TODO: Cleanup pty
        return -ENOMEM;
    }

    if (current_cpu->current_process) {
        pty->master_node->attr.uid = current_cpu->current_process->cred.uid;
        pty->master_node->attr.gid = current_cpu->current_process->cred.gid;
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
