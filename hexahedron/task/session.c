/**
 * @file hexahedron/task/session.c
 * @brief session
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/fs/tty.h>
#include <kernel/mm/slab.h>
#include <kernel/debug.h>

/* Cache */
static slab_cache_t *pgrp_cache = NULL;
static slab_cache_t *session_cache = NULL;

/* Helpers */
static void session_freeProcessGroup(process_group_t *pgrp);
static void session_freeSession(session_t *session);
#define PGRP_HOLD(pgrp) refcount_inc(&(pgrp)->refs)
#define PGRP_RELEASE(pgrp) ({ if (refcount_dec(&(pgrp)->refs) == 0) session_freeProcessGroup(pgrp); })
#define SESSION_HOLD(session) refcount_inc(&(session)->refs)
#define SESSION_RELEASE(session) ({ if (refcount_dec(&(session)->refs) == 0) session_freeSession(session); })

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SESSION", __VA_ARGS__)

/**
 * @brief Initialize session control
 */
void session_init() {
    pgrp_cache = slab_createCache("pgrp", SLAB_CACHE_DEFAULT, sizeof(process_group_t), 0, NULL, NULL);
    session_cache = slab_createCache("session", SLAB_CACHE_DEFAULT, sizeof(session_t), 0, NULL, NULL);
}

/**
 * @brief Get group for a process, returns referenced
 */
static inline process_group_t *session_getGroup(process_t *proc) {
    spinlock_acquire(&proc->pgrp_lock);
    process_group_t *pgrp = proc->pgrp;
    if (pgrp) PGRP_HOLD(pgrp);
    spinlock_release(&proc->pgrp_lock);
    return pgrp;
}

/**
 * @brief Get the PGID
 */
pid_t session_getpgid(process_t *process) {
    if (process->flags & PROCESS_KERNEL) return 0;
    process_group_t *pgrp = session_getGroup(process);
    pid_t pgid = pgrp->pgid;
    PGRP_RELEASE(pgrp);
    return pgid;
}

/**
 * @brief Get the SID
 */
pid_t session_getsid(process_t *process) {
    if (process->flags & PROCESS_KERNEL) return 0;
    process_group_t *pgrp = session_getGroup(process);
    pid_t sid = pgrp->session->sid;
    PGRP_RELEASE(pgrp);
    return sid;
}

/**
 * @brief Create a new session
 */
static session_t *session_newSession(pid_t sid) {
    session_t *session = slab_allocate(session_cache);
    SPINLOCK_INIT(&session->lock);
    refcount_init(&session->refs, 1);
    DLIST_INIT(&session->groups);
    session->ctty = NULL;
    session->sid = sid;
    return session;
}

/**
 * @brief Create a new group
 */
static process_group_t *session_newGroup(session_t *session, pid_t pgid) {
    process_group_t *pgrp = slab_allocate(pgrp_cache);
    SPINLOCK_INIT(&pgrp->lock);
    refcount_init(&pgrp->refs, 1);
    DLIST_INIT(&pgrp->members);
    pgrp->pgid = pgid;
    pgrp->session = session;
    SESSION_HOLD(session);
    return pgrp;
}

/**
 * @brief Death
 */
static void session_freeProcessGroup(process_group_t *pgrp) {
    SESSION_RELEASE(pgrp->session);
    slab_free(pgrp_cache, pgrp);
}

/**
 * @brief Death
 */
static void session_freeSession(session_t *session) {
    slab_free(session_cache, session);
}

/**
 * @brief Find the process group
 * @todo Make this fast
 */
static process_group_t *session_findGroup(session_t *session, pid_t pgid) {
    DLIST_FOREACH(process_group_t, group, &session->groups, session_entry) {
        if (group->pgid == pgid) return group;
    }
    return NULL;
}

/**
 * @brief Initialize job control on a process
 * @param parent The parent of the process (NULL if init)
 * @param process The process
 */
void session_initProcess(process_t *parent, process_t *process) {
    SPINLOCK_INIT(&process->pgrp_lock);
    process->pgrp = NULL;
    if (parent == NULL) {
        session_t *session = session_newSession(1);
        process_group_t *pgrp = session_newGroup(session, 1);

        DLIST_INSERT_TAIL(&session->groups, pgrp, session_entry);
        PGRP_HOLD(pgrp); // process reference
        DLIST_INSERT_TAIL(&pgrp->members, process, pgrp_entry);
        process->pgrp = pgrp;
        SESSION_RELEASE(session); // the group now owns the session
    } else {
        process_group_t *pgrp = session_getGroup(parent);

        spinlock_acquire(&pgrp->lock);
        DLIST_INSERT_TAIL(&pgrp->members, process, pgrp_entry);
        process->pgrp = pgrp;
        spinlock_release(&pgrp->lock);
    }
}

/**
 * @brief Handle destroying the process
 */
void session_destroyProcess(process_t *process) {
    // this can retry a few times
    for (;;) {
        process_group_t *pgrp = session_getGroup(process);

        session_t *session = pgrp->session;
        bool retired = false;
        bool detached = false;

        spinlock_acquire(&session->lock);
        spinlock_acquire(&pgrp->lock);
        spinlock_acquire(&process->pgrp_lock);

        // detaching from the pgrp itself
        if (process->pgrp == pgrp) {
            detached = true;
            DLIST_REMOVE(&pgrp->members, process_t, process, pgrp_entry);
            process->pgrp = NULL;
            if (!DLIST_FIRST(&pgrp->members)) {
                // we can remove this pgrp entirely
                DLIST_REMOVE(&session->groups, process_group_t, pgrp, session_entry);
                retired = true;
            }
        }

        spinlock_release(&process->pgrp_lock);
        spinlock_release(&pgrp->lock);
        spinlock_release(&session->lock);

        if (retired) PGRP_RELEASE(pgrp);
        if (detached) PGRP_RELEASE(pgrp); // process reference
        PGRP_RELEASE(pgrp);
        if (detached) return;
    }
}

/**
 * @brief Set controlling tty
 * @param tty The TTY to set
 * @param steal see @c TIOCSTTY
 */
int session_claimTTY(tty_t *tty, bool steal) {
    process_t *proc = current_cpu->current_process;
    process_group_t *pgrp = session_getGroup(proc);

    session_t *session = pgrp->session;
    if (session->sid != proc->pid) {
        // not the group leader
        PGRP_RELEASE(pgrp);
        return -EPERM;
    }

    spinlock_acquire(&tty->job_lock);
    session_t *old_session = tty->session;
    SPINLOCK_LOCK_BOTH(&session->lock, old_session ? &old_session->lock : &session->lock);

    process_group_t *old_fg = NULL;
    int error = 0;
    if (session->ctty && session->ctty != tty) {
        error = -EPERM;
    } else if (old_session && old_session != session && (!steal || !PROC_IS_ROOT(proc))) {
        error = -EPERM;
    } else if (old_session != session) {
        // yay we can kill it
        if (old_session && old_session->ctty == tty) {
            old_session->ctty = NULL;
        }
        
        old_fg = tty->foreground;
        SESSION_HOLD(session);
        PGRP_HOLD(pgrp);
        tty->session = session;
        tty->foreground = pgrp;
        session->ctty = tty;
    }

    SPINLOCK_UNLOCK_BOTH(&session->lock, old_session ? &old_session->lock : &session->lock);
    spinlock_release(&tty->job_lock);

    if (error == 0 && old_session != session) {
        if (old_fg && old_session) {
            session_signalGroup(old_fg, SIGHUP);
            session_signalGroup(old_fg, SIGCONT);
        }

        if (old_fg) PGRP_RELEASE(old_fg);
        if (old_session) SESSION_RELEASE(old_session);
    }

    PGRP_RELEASE(pgrp);
    return error;
}

/**
 * @brief Hangup TTY
 * @param tty The TTY to hangup
 */
void session_hangup(tty_t *tty) {
    process_t *proc = current_cpu->current_process;
    process_group_t *caller = session_getGroup(proc);

    process_group_t *fg = NULL;
    session_t *session = NULL;

    spinlock_acquire(&tty->job_lock);

    if (tty->session == caller->session && tty->session->sid == proc->pid) {
        session = tty->session;
        spinlock_acquire(&session->lock);
        if (session->ctty == tty) session->ctty = NULL;
        fg = tty->foreground;
        tty->foreground = NULL;
        tty->session = NULL;
        spinlock_release(&session->lock);
    }

    spinlock_release(&tty->job_lock);

    if (fg) {
        session_signalGroup(fg, SIGHUP);
        session_signalGroup(fg, SIGCONT);
        PGRP_RELEASE(fg);
    }

    if (session) SESSION_RELEASE(session);
    PGRP_RELEASE(caller);
}

/**
 * @brief Signal group of processes
 * @param pgrp The process group to signal
 * @param signal The signal number
 * @note You should have a reference on pgrp on call
 */
int session_signalGroup(process_group_t *pgrp, int signal) {
    spinlock_acquire(&pgrp->lock);
    DLIST_FOREACH(process_t, proc, &pgrp->members, pgrp_entry) {
        signal_send(proc, signal);
    }
    spinlock_release(&pgrp->lock);
    return 0;
}

/**
 * @brief Signal foreground
 * @param tty The TTY to signal the foreground of
 * @param signal The signal number
 */
int session_signalForeground(tty_t *tty, int signal) {
    // take a reference to the foreground
    spinlock_acquire(&tty->job_lock);
    process_group_t *fg = tty->foreground;
    if (fg) PGRP_HOLD(fg);
    spinlock_release(&tty->job_lock);

    if (fg == NULL) {
        return 0;
    }

    int r = session_signalGroup(fg, signal);
    PGRP_RELEASE(fg);
    return r;
}

/**
 * @brief Get foreground (tcgetpgrp)
 * @param proc The process to get the foreground of
 * @param tty The TTY
 */
pid_t session_tcgetpgrp(process_t *proc, tty_t *tty) {
    process_group_t *pgrp = session_getGroup(proc);

    spinlock_acquire(&tty->job_lock);
    pid_t result;
    if (tty->session == pgrp->session && tty->foreground != NULL) {
        result = tty->foreground->pgid;
    } else {
        result = (pid_t)-ENOTTY;
    }
    spinlock_release(&tty->job_lock);
    PGRP_RELEASE(pgrp);
    return result;
}

/**
 * @brief Set foreground (tcsetpgrp)
 * @param proc The process to set the foreground of
 * @param tty The TTY to set the foreground on
 * @param pgid The process group ID
 */
int session_tcsetpgrp(process_t *proc, tty_t *tty, pid_t pgid) {
    process_group_t *pgrp = session_getGroup(proc);
    session_t *session = pgrp->session;

    spinlock_acquire(&tty->job_lock);
    
    if (tty->session != session) {
        spinlock_release(&tty->job_lock);
        PGRP_RELEASE(pgrp);
        return -ENOTTY;
    }

    spinlock_acquire(&session->lock);
    process_group_t *next = session_findGroup(session, pgid);
    if (next) PGRP_HOLD(next);
    spinlock_release(&session->lock);

    if (!next) {
        spinlock_release(&tty->job_lock);
        PGRP_RELEASE(pgrp);
        return -EPERM;
    }

    process_group_t *saved = tty->foreground;
    tty->foreground = next;

    spinlock_release(&tty->job_lock);

    if (saved) PGRP_RELEASE(saved);
    PGRP_RELEASE(pgrp);

    return 0;
}

/**
 * @brief Move target to pgid
 */
static int session_move(process_t *target, process_group_t *old, pid_t pgid) {
    session_t *expected_session = old->session;
    process_group_t *source = old;
    bool source_held = false;

    for (;;) {
        session_t *session = source->session;
        process_group_t *candidate = NULL;
        process_group_t *retired = NULL;

        if (session != expected_session || target->pid == session->sid) {
            if (source_held) PGRP_RELEASE(source);
            return -EPERM;
        }

        // locate the group to set
        spinlock_acquire(&session->lock);
        process_group_t *dest = session_findGroup(session, pgid);
        if (!dest) {
            spinlock_release(&session->lock);
            if (pgid != target->pid) {
                if (source_held) PGRP_RELEASE(source);
                return -EPERM;
            }

            candidate = session_newGroup(session, pgid);

            // !!! this double-checks because another caller could have created the group
            spinlock_acquire(&session->lock);
            dest = session_findGroup(session, pgid);
            if (!dest) dest = candidate;
        }

        SPINLOCK_LOCK_BOTH(&source->lock, &dest->lock);
        spinlock_acquire(&target->pgrp_lock);

        if (target->pgrp != source) {
            spinlock_release(&target->pgrp_lock);
            SPINLOCK_UNLOCK_BOTH(&source->lock, &dest->lock);
            spinlock_release(&session->lock);
            if (candidate) PGRP_RELEASE(candidate);

            process_group_t *next = session_getGroup(target);
            if (source_held) PGRP_RELEASE(source);
            if (!next) return -ESRCH;
            source = next;
            source_held = true;
            continue;
        }

        if (dest != source) {
            if (dest == candidate) {
                // a new pgrp had to be created
                DLIST_INSERT_TAIL(&session->groups, candidate, session_entry);
            }

            PGRP_HOLD(dest);
            DLIST_REMOVE(&source->members, process_t, target, pgrp_entry);
            DLIST_INSERT_TAIL(&dest->members, target, pgrp_entry);
            target->pgrp = dest;

            // are there any members remaining?
            if (!DLIST_FIRST(&source->members)) {
                DLIST_REMOVE(&session->groups, process_group_t, source, session_entry);
                retired = source;
            }
        }

        spinlock_release(&target->pgrp_lock);
        SPINLOCK_UNLOCK_BOTH(&source->lock, &dest->lock);

        spinlock_release(&session->lock);

        if (dest != source) PGRP_RELEASE(source);
        if (retired) PGRP_RELEASE(retired);

        if (candidate && candidate != dest) {
            // this releases the only reference on candidate, causing it to be freed
            PGRP_RELEASE(candidate);
        }

        if (source_held) PGRP_RELEASE(source);
        
        return 0;
    }
}

/**
 * @brief Set pgid
 * @param pid Process ID
 * @param pgid Process group ID
 */
int session_setpgid(pid_t pid, pid_t pgid) {
    process_t *target = pid ? process_getFromPID(pid) : current_cpu->current_process;
    process_t *caller = current_cpu->current_process;
    if (!target || (target != caller && target->parent != caller)) {
        return -ESRCH;
    }
    
    if (!pgid) {
        pgid = target->pid;
    }

    // get both groups
    process_group_t *old = session_getGroup(target);
    process_group_t *caller_group = session_getGroup(caller);

    if (old->session != caller_group->session) {
        PGRP_RELEASE(caller_group);
        PGRP_RELEASE(old);
        return -EPERM; // not even part of the session
    }

    if (target->pid == old->session->sid) {
        PGRP_RELEASE(caller_group);
        PGRP_RELEASE(old);
        return -EPERM; // the session leader
    }

    int r = session_move(target, old, pgid);
    PGRP_RELEASE(caller_group);
    PGRP_RELEASE(old);
    return r;
}

/**
 * @brief Set SID
 */
pid_t session_setsid() {
    process_t *proc = current_cpu->current_process;
    process_group_t *old = session_getGroup(proc);
    if (old->pgid == proc->pid) {
        PGRP_RELEASE(old);
        return -EPERM;
    }

    session_t *session = session_newSession(proc->pid);
    process_group_t *pgrp = session_newGroup(session, proc->pid);

    session_t *old_session = old->session;
    spinlock_acquire(&old_session->lock);
    spinlock_acquire(&old->lock);
    spinlock_acquire(&proc->pgrp_lock);

    bool dead = false;
    int error = 0;
    if (proc->pgrp != old) {
        // lost the race
        assert(0 && "TODO race loss handling");
    } else if (session_findGroup(old_session, proc->pid)) {
        error = -EPERM;
    } else {
        DLIST_REMOVE(&old->members, process_t, proc, pgrp_entry);
        DLIST_INSERT_TAIL(&session->groups, pgrp, session_entry);
        DLIST_INSERT_TAIL(&pgrp->members, proc, pgrp_entry);
        PGRP_HOLD(pgrp); // process reference
        proc->pgrp = pgrp;

        // anything left?
        if (!DLIST_FIRST(&old->members)) {
            DLIST_REMOVE(&old_session->groups, process_group_t, old, session_entry);
            dead = true;
        }
    }

    spinlock_release(&proc->pgrp_lock);
    spinlock_release(&old->lock);
    spinlock_release(&old_session->lock);

    if (error != 0) {
        PGRP_RELEASE(pgrp);
    } else {
        PGRP_RELEASE(old);
    }

    if (dead) {
        PGRP_RELEASE(old); // initial ref
    }

    PGRP_RELEASE(old);
    SESSION_RELEASE(session);
    return error ? error : proc->pid;
}
