/**
 * @file userspace/symphonyd/symphonyd.h
 * @brief Symphony header
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _SYMPHONYD_H
#define _SYMPHONYD_H

/**** INCLUDES ****/
#include <ethereal/symphony.h>
#include <ethereal/shared.h>
#include <structs/list.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/**** TYPES ****/

typedef struct server_client {
    struct server_client *next;
    struct server_client *prev;
    pthread_t thread;
    
    int fd;
    float volume;
} server_client_t;

struct server_stream;

typedef struct server_buffer {
    node_t node;
    struct server_buffer *next;
    int id;
    
    server_client_t *client;
    struct server_stream *stream;

    // Generic information
    int shmfd;
    bool playing;
    

    // Buffer information
    unsigned short channels;
    unsigned int sample_rate;
    audio_format_t format;

    // Ringbuffer
    size_t buffer_size;
    symphony_buffer_t *buffer;
} server_buffer_t;

typedef struct server_device {
    node_t node;
    int id;
    
    list_t streams;
} server_device_t;

typedef struct server_stream {
    node_t node;
    int id;
    int fd;

    // Default stream information for GET_STREAM
    audio_stream_config_t config;

    // Samples
    // enough for 2 channel, signed 16-bit PCM audio @ 1024 frames per period
    // TODO: Multi-format support
    float samples[2048];
    int16_t samples_out[4096];

    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t avail;
    list_t buffers;
} server_stream_t;

#endif
