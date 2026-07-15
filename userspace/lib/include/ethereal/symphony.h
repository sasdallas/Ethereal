/**
 * @file userspace/lib/include/ethereal/symphony.h
 * @brief Library for Ethereal's sound server (Symphony)
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _ETHEREAL_SYMPHONY_H
#define _ETHEREAL_SYMPHONY_H

/**** INCLUDES ****/
#include <ethereal/shared.h>
#include <ethereal/audio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**** DEFINITIONS ****/

#define SYMPHONY_SOCKET_PATH        "/comm/symphony"
#define SYMPHONY_MAGIC              0xAAAABBBB
#define SYMPHONY_MAGIC_RESP         0xCCCCDDDD

/**** TYPES ****/

typedef enum symphony_packet_type {
    SYMPHONY_PACKET_GET_INFO,
    SYMPHONY_PACKET_GET_DEVICE,
    SYMPHONY_PACKET_GET_STREAM,
    SYMPHONY_PACKET_ADD_BUFFER,
    SYMPHONY_PACKET_START,
    SYMPHONY_PACKET_STOP
} symphony_packet_type_t;

typedef struct symphony_header {
    uint32_t magic;
    symphony_packet_type_t type;
    size_t size;
} symphony_header_t;

typedef struct symphony_resp {
    uint32_t magic;
    symphony_packet_type_t type;
    int error;
} symphony_resp_t;

/* Packets */

typedef struct symphony_packet_add_buffer {
    unsigned short stream_id; // SYMPHONY_STREAM macro
    unsigned short channels;
    unsigned int sample_rate;
    audio_format_t format;

    size_t buflen;
    key_t bufkey;
} symphony_packet_add_buffer_t;

typedef struct symphony_packet_start {
    int buffer_id;
} symphony_packet_start_t;

typedef struct symphony_packet_stop {
    int buffer_id;
} symphony_packet_stop_t;

typedef struct symphony_packet_get_device {
    unsigned char device_id;
} symphony_packet_get_device_t;

typedef struct symphony_packet_get_stream {
    unsigned short stream_id; // SYMPHONY_STREAM macro
} symphony_packet_get_stream_t;

/* Responses */

typedef struct symphony_device_info {
    int device_streams;
} symphony_device_info_t;

typedef struct symphony_info {
    // symphony server information
    int ver_major;
    int ver_minor;
    int ver_lower;

    // number of available devices
    int num_devices;
} symphony_info_t;

typedef struct symphony_stream_info {
    audio_stream_config_t config;
} symphony_stream_info_t;

typedef struct symphony_buffer {
    uint64_t head;
    uint64_t tail;
    int16_t samples[];
} symphony_buffer_t;

typedef int symphony_buffer_id_t;

/**** MACROS ****/

#define SYMPHONY_STREAM(device_id, idx) ((unsigned short)((((device_id) & 0xFF) << 8) | ((idx) & 0xff)))
#define SYMPHONY_STREAM_ANY 0xFFFF

/**** FUNCTIONS ****/

/**
 * @brief Initialize connection with the Symphony audio library
 * @returns 0 on success
 */
int symphony_connect();

/**
 * @brief Get information on the Symphony server
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getInfo(symphony_info_t *info);

/**
 * @brief Get information on a specific Symphony device
 * @param device The index of the device to get
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getDevice(int device, symphony_device_info_t *info);

/**
 * @brief Get information on a specific Symphony stream
 * @param stream The ID of the stream to get
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getStream(unsigned short stream, symphony_stream_info_t *info);

/**
 * @brief Create and attach a buffer to a Symphony stream
 * @param stream_id The stream ID to attach the buffer to
 * @param format The audio format of the buffer
 * @param sample_rate The sample rate of the buffer
 * @param channels The amount of channels in the buffer
 * @param size The size of the buffer, in samples
 * @returns Buffer ID handle on success, less than 0 on error
 */
symphony_buffer_id_t symphony_addBuffer(uint16_t stream_id, audio_format_t format, uint32_t sample_rate, uint16_t channels, size_t size);

/**
 * @brief Get buffer pointer
 * @param id The buffer ID to get
 * @returns Pointer to Symphony buffer object or NULL
 */
symphony_buffer_t *symphony_getBuffer(symphony_buffer_id_t id);

/**
 * @brief Start playback
 * @param id The buffer ID to start playback on
 * @returns 0 on success
 */
int symphony_start(symphony_buffer_id_t id);

/**
 * @brief Stop playback
 * @param id The buffer ID to stop playback on
 * @returns 0 on success
 */
int symphony_stop(symphony_buffer_id_t id);

/**
 * @brief Drain playback
 * @param id The buffer ID to drain
 * @returns 0 on success
 */
int symphony_drain(symphony_buffer_id_t id);

/**
 * @brief Write samples
 * @param buffer The buffer to write the samples to
 * @param samples The samples to write
 * @param num The amount of samples to write
 * @warning This will block if the audio server isnt updating fast enough
 */
void symphony_write(symphony_buffer_id_t buffer, void *samples, size_t num);

#endif
