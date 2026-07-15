/**
 * @file hexahedron/include/kernel/subsystems/audio.h
 * @brief Audio subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_AUDIO_H
#define KERNEL_SUBSYSTEMS_AUDIO_H

/**** INCLUDES ****/
#include <kernel/misc/waitqueue.h>
#include <kernel/misc/mutex.h>
#include <kernel/fs/devfs.h>
#include <ethereal/audio.h>
#include <stddef.h>


/**** TYPES ****/

struct audio_channel;
struct audio_stream;
struct audio_control;

typedef struct audio_stream_ops {
    int (*configure)(struct audio_stream *stream, audio_stream_config_t *config);
    int (*set_state)(struct audio_stream *stream, audio_stream_state_t state);
    uint64_t (*get_position)(struct audio_stream *stream);
} audio_stream_ops_t;

/* Audio device, represents a single sound card */
typedef struct audio_device {
    struct audio_device *next;
    int index;
    devfs_node_t *node;
    mutex_t lock;

    // Controls
    struct audio_control *controls;
    size_t ncontrols;

    // Streams
    struct audio_stream *streams;
    size_t nstreams;

    // Driver-specific
    void *priv;
} audio_device_t;

/* Audio control */
/* Many of these will exist */
typedef struct audio_control {
    struct audio_control *next;
    struct audio_device *parent;

    // General information
    int id;
    char *name;
    audio_control_type_t type;
    audio_control_purpose_t purpose;

    // Sometimes these will differ across types
    struct {
        int (*get_value)(struct audio_control *control, audio_control_value_t *value);
        int (*set_value)(struct audio_control *control, audio_control_value_t *value);
    } ops;

    // Value information, pre-populated by driver
    union {
        struct {
            long min;
            long max;
            long step;
        } integer;

        struct {
            unsigned int nitems;
            unsigned int current_item;
            char **names;
        } enumerated;

        struct {
            size_t length;
        } vendor;
    } info;

    // Driver-specific
    void *priv;
} audio_control_t;

/* Audio buffer SGL */
typedef struct audio_buffer_sgl_ent {
    uintptr_t paddr;
    size_t length;
} audio_buffer_sgl_ent_t;

typedef struct audio_buffer_sgl {
    size_t num_entries;
    size_t size_pages;
    audio_buffer_sgl_ent_t entries[];
} audio_buffer_sgl_t;

/* Audio buffer */
typedef struct audio_buffer {
    // Application pointer in this ringbuffer
    uint64_t app_ptr;

    // Lock and event
    spinlock_t lock;
    wait_queue_t waiters;
    wait_queue_t drainers;

    // Virtual address to make memory copying faster
    void *vaddr;

    // Scatter-gather list
    audio_buffer_sgl_t *sgl;

    // Buffer information
    size_t buffer_size;                 // Buffer size in bytes
    size_t frame_size;                  // Frame size in bytes
    size_t periods;                     // Total periods configured to play
    size_t period_size;                 // Size per period
    size_t bytes_available;             // Available bytes in the buffer
    size_t start_thres;                 // Start threshold
} audio_buffer_t;

/* Audio stream device - for sound */
typedef struct audio_stream {
    struct audio_stream *next;
    struct audio_device *parent;
    audio_stream_state_t state;
    audio_stream_ops_t *ops;
    devfs_node_t *node;

    // Buffer
    audio_buffer_t buffer;

    // Driver-specific
    void *priv;
} audio_stream_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new audio device
 * @param priv The private pointer to use for this device
 * @returns A new audio device, already placed in the list
 */
audio_device_t *audio_createDevice(void *priv);

/**
 * @brief Create a new audio control
 * @param device The device to add the control to
 * @param name The name of the control to register
 * @param type The type of control to register
 * @param purpose The purpose of the control
 * @param priv The private pointer
 * @returns Control object
 */
audio_control_t *audio_createControl(audio_device_t *device, char *name, audio_control_type_t type, audio_control_purpose_t purpose, void *priv);

/**
 * @brief Create a new audio stream
 * @param device The device to add the stream to
 * @param ops The stream operations
 * @param priv The private pointer
 * @returns Stream object
 */
audio_stream_t *audio_createStream(audio_device_t *device, audio_stream_ops_t *ops, void *priv);

/**
 * @brief Initialize buffer
 * @param buffer The buffer to initialize
 * @param sgl The SGL allocated
 */
void audio_initBuffer(audio_buffer_t *buffer, audio_buffer_sgl_t *sgl);

/**
 * @brief Audio advance stream to next period
 * @param stream The stream to advance
 */
void audio_streamNextPeriod(audio_stream_t *stream);

/**
 * @brief Helper to get the frame size
 */
static inline int audio_getFrameSize(audio_stream_config_t *config) {
    int size = 0;
    switch (config->format) {
        case AUDIO_FORMAT_S16_LE: size = 2; break;
        case AUDIO_FORMAT_S24_LE: size = 3; break;
        case AUDIO_FORMAT_F32_LE: case AUDIO_FORMAT_S32_LE: size = 4; break;
        default: assert(0);
    }

    return size * config->channels;
}

#endif
