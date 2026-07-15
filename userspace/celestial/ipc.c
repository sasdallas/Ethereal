/**
 * @file userspace/celestialv2/ipc.c
 * @brief Handles the IPC
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#define _GNU_SOURCE
#include <unistd.h>

#include "celestial.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <signal.h>

#ifdef BUILDING_LINUX
#define SOCKET_PATH "/tmp/wndsrv"
#else
#define SOCKET_PATH "/comm/wndsrv"
#endif

void ipc_addClient(int fd) {
    wm_client_t *client = malloc(sizeof(wm_client_t));
    client->window_head = NULL;
    client->client_fd = fd;
    client->window_count = 0;
    client->dead = false;

    // TODO: make this atomic
    pthread_mutex_lock(&SERVER->client_lock);
    client->next = SERVER->client_list;
    SERVER->client_list = client;
    SERVER->client_count++;
    pthread_mutex_unlock(&SERVER->client_lock);

    // Wakeup the thread
    char buf[1] = { 'a' };
    write(SERVER->ipc_wake_pipe[1], buf, 1);
}

void ipc_removeClient(int fd) {
    TRACE_DEBUG("Removing client %d as it is dead...\n", fd);
    pthread_mutex_lock(&SERVER->client_lock);

    wm_client_t **prev = &SERVER->client_list;
    wm_client_t *c = SERVER->client_list;

    while (c) {
        if (c->client_fd == fd) {
            *prev = c->next;
            c->next = NULL;
            c->client_fd = -1;
            __atomic_store_n(&c->dead, true, __ATOMIC_SEQ_CST);

            break;
        }

        prev = &c->next;
        c = c->next;
    }

    if (c) SERVER->client_count--;
    pthread_mutex_unlock(&SERVER->client_lock);

    if (!c) {
        TRACE_DEBUG("Client %d was already removed\n", fd);
        return;
    }

    close(fd);

    wm_window_t *win = c->window_head;
    while (win) {
        wm_window_t *next = win->next_client;
        window_close(win);
        win = next;
    }

    ipc_releaseClient(c);
}

void ipc_releaseClient(wm_client_t *client) {
    if (!client || !__atomic_load_n(&client->dead, __ATOMIC_SEQ_CST)) {
        return;
    }

    if (__atomic_load_n(&client->window_count, __ATOMIC_SEQ_CST) == 0) {
        free(client);
    }
}

void ipc_handle(int fd) {
    // Handle IPC for fd
    char data[4096];
    ssize_t ret = recv(fd, data, 4096, 0);
    if (ret < 0) {
        if (errno == ECONNRESET || errno == EBADF || errno == ENOTSOCK) {
            TRACE_WARN("Connection reset with socket %d, removing dead client.\n", fd);
            ipc_removeClient(fd);
            return;
        } else {
            FATAL("Error receiving data from socket: %s\n", strerror(errno));
        }
    }

    if (!ret) {
        ipc_removeClient(fd);
        return;
    }

    // find client for fd
    // TODO: unsafe
    wm_client_t *target = SERVER->client_list;
    while (target) {
        if (target->client_fd == fd) break;
        target = target->next;
    }
    
    if (!target) {
        TRACE_DEBUG("Ignoring IPC on removed client %d\n", fd);
        return;
    }

    return request_handle(target, data, ret);
}

void *ipc_acceptor_thread(void *arg) {
    TRACE_DEBUG("IPC_accept thread TID: %d\n", gettid());
    for (;;) {
        TRACE_DEBUG("Accepting IPC connections now...\n");
        int fd = accept(SERVER->sock_fd, NULL, NULL);
        if (fd < 0) {
            FATAL("accept: %s\n", strerror(errno));
            return NULL;   
        }

        TRACE_DEBUG("Got new client on fd %d\n", fd);
        ipc_addClient(fd);
    }
}

void *ipc_thread(void *arg) {
    TRACE_DEBUG("IPC thread TID: %d\n", gettid());
    for (;;) {
        pthread_mutex_lock(&SERVER->client_lock);
        int nfds = SERVER->client_count + 1;
        struct pollfd fds[nfds];

        int idx = 0;
        wm_client_t *cli = SERVER->client_list;
        while (cli) {
            fds[idx].fd = cli->client_fd;
            fds[idx].events = POLLIN  | POLLHUP;
            fds[idx].revents = 0;
            idx++;

            cli = cli->next;
        }

        pthread_mutex_unlock(&SERVER->client_lock);

        fds[idx].fd = SERVER->ipc_wake_pipe[0];
        fds[idx].events = POLLIN;
        fds[idx].revents = 0;

        int e = poll(fds, nfds, -1);
        if (e < 0) {
            FATAL("poll: %s\n", strerror(errno));
        }

        if (fds[idx].revents & POLLIN) {
            // Wake pipe had information and woke us up
            // Drain the pipe
            TRACE_DEBUG("Flushing buffer\n");
            char buffer;
            while (read(SERVER->ipc_wake_pipe[0], &buffer, 1) > 0);

            if (e == 1) continue;
        }

        for (int i = 0; i < nfds-1; i++) {
            if (fds[i].revents & POLLHUP) {
                ipc_removeClient(fds[i].fd);
            } else if (fds[i].revents & POLLIN) {
                ipc_handle(fds[i].fd); 
            }
        }
    }
}

int ipc_init() {
    pthread_mutex_init(&SERVER->client_lock, NULL);
    
    // Create the socket
    unlink(SOCKET_PATH);
    SERVER->sock_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (SERVER->sock_fd < 0) {
        FATAL("Error creating window socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
        .sun_path = SOCKET_PATH
    };

    if (bind(SERVER->sock_fd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        FATAL("Error binding to " SOCKET_PATH ": %s\n", strerror(errno));
        return 1;
    }

    if (listen(SERVER->sock_fd, 5) < 0) {
        FATAL("listen: %s\n", strerror(errno));
        return 1;
    }

    TRACE_INFO("Bound to " SOCKET_PATH "\n");

    if (pipe(SERVER->ipc_wake_pipe) < 0) {
        FATAL("Error creating IPC wake pipe: %s\n", strerror(errno));
        return 1;
    }

    if (fcntl(SERVER->ipc_wake_pipe[0], F_SETFL, O_NONBLOCK) < 0) {
        FATAL("Error setting IPC pipe as nonblocking: %s\n", strerror(errno));
        return 1;
    }

    if (fcntl(SERVER->ipc_wake_pipe[1], F_SETFL, O_NONBLOCK) < 0) {
        FATAL("Error setting IPC pipe as nonblocking: %s\n", strerror(errno));
        return 1;
    }

    if (pthread_create(&SERVER->ipc_acceptor_thread, NULL, ipc_acceptor_thread, NULL) < 0) {
        FATAL("Error creating IPC acceptor thread: %s\n", strerror(errno));
        return 1;
    }

    if (pthread_create(&SERVER->ipc_thread, NULL, ipc_thread, NULL) < 0) {
        FATAL("Error creating IPC thread: %s\n", strerror(errno));
        return 1;
    }

    TRACE_DEBUG("IPC initialized\n");
    return 0;
}

void ipc_shutdown() {
    TRACE_INFO("Shutting down IPC...\n");
    if (SERVER->ipc_thread) {
        pthread_cancel(SERVER->ipc_thread);
    }

    if (SERVER->ipc_acceptor_thread) {
        pthread_cancel(SERVER->ipc_acceptor_thread);
    }

    if (SERVER->ipc_wake_pipe[0]) {
        close(SERVER->ipc_wake_pipe[0]);
        close(SERVER->ipc_wake_pipe[1]);
    }
    
    if (SERVER->sock_fd) close(SERVER->sock_fd);
    unlink(SOCKET_PATH);
    
    TRACE_DEBUG("IPC was shutdown successfully\n");
}
