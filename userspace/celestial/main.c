/**
 * @file userspace/celestialv2/main.c
 * @brief Celestial main
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
#include <stdio.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <poll.h>

/* primary server */
celestial_t celestial_server = { 0 };

pid_t main_thread = 0;

extern pthread_mutex_t anim_lock;
extern pthread_cond_t anim_cond;
extern wm_window_anim_t *anim_list;

void celestial_shutdown() {
    if (gettid() == main_thread) {
        ipc_shutdown();
        renderer_shutdown();
        exit(0);
    } else {
        kill(main_thread, SIGINT);
        for (;;);
    }
}

void celestial_fatal() {
    // We can only shutdown if we are the main thread
    if (gettid() == main_thread) {
        celestial_shutdown();
    } else {
        TRACE_DEBUG("sending SIGUSR1 to main thread\n");
        kill(getpid(), SIGUSR1);
        for (;;);
    }
}

// Fatal signal handler
void sigusr1_handler(int signum) {
    TRACE_ERROR("SIGUSR1 received, fatal signal\n");
    celestial_fatal();
}

void sigint_handler(int signum) {
    TRACE_DEBUG("Shutdown by SIGINT\n");
    return celestial_shutdown();
}

// Primary loop
void celestial_wm_loop() {
    for (;;) {
        extern void window_processAnimations();
        pthread_mutex_lock(&anim_lock);
        while (anim_list == NULL) {
            pthread_cond_wait(&anim_cond, &anim_lock);;
        }

        window_processAnimations();
        pthread_mutex_unlock(&anim_lock);
        usleep(10000);
    }
}

int main(int argc, char *argv[]) {
    int log_device = open("/device/fbcon", O_RDWR);
    dup2(log_device, STDOUT_FILENO);
    dup2(log_device, STDERR_FILENO);

    TRACE_INFO("celestialv2 1.0.0\n");
    main_thread = gettid();
    memset(SERVER, 0, sizeof(celestial_t));
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGINT, sigint_handler);

    pthread_mutex_init(&SERVER->dmg_lock, NULL);
    pthread_mutex_init(&SERVER->bind_lck, NULL);
    pthread_cond_init(&SERVER->dmg_cond, NULL);
    pthread_mutex_init(&SERVER->window_lock, NULL);
    
    renderer_init();
    input_init();
    ipc_init();

    pid_t cpid = fork();
    if (!cpid) {
        const char *argv[] = { "desktop", NULL }; 
        execvp("desktop", (char *const *)argv);
        TRACE_ERROR("Could not start desktop process: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    celestial_wm_loop();
    return 0;
}

uint64_t celestial_now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
