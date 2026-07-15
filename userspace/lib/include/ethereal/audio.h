/**
 * @file userspace/lib/include/ethereal/audio.h
 * @brief Ethereal audio API
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _ETHEREAL_AUDIO_H
#define _ETHEREAL_AUDIO_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**** DEFINITIONS ****/

/* Stream requests */
#define IO_AUDIO_SET_CONFIG                 0x2001  // Set configuration of the stream, updates state to CONFIGURED
#define IO_AUDIO_REQUEST                    0x2002  // Request the stream to do something
#define IO_AUDIO_AVAILABLE                  0x2003  // Returns the amount of bytes available in the buffer

/* Device requests */
#define IO_AUDIO_DEVICE_GET_INFO            0x3001  // Get information on the device          
#define IO_AUDIO_DEVICE_GET_CONTROL         0x3002  // Get control information from the device
#define IO_AUDIO_DEVICE_GET_VALUE           0x3003  // Get a value of a specific control
#define IO_AUDIO_DEVICE_SET_VALUE           0x3004  // Set the value of a specific control

/**** TYPES ****/

typedef enum audio_stream_request {
    AUDIO_STREAM_PLAY,
    AUDIO_STREAM_STOP,
    AUDIO_STREAM_PAUSE,
    AUDIO_STREAM_DRAIN
} audio_stream_request_t;

typedef enum audio_stream_state {
    AUDIO_STREAM_STATE_STOPPED,
    AUDIO_STREAM_STATE_CONFIGURED,
    AUDIO_STREAM_STATE_PLAYING,
    AUDIO_STREAM_STATE_DRAINING,
    AUDIO_STREAM_STATE_PAUSED,
} audio_stream_state_t;

typedef enum audio_control_type {
    AUDIO_CONTROL_BOOLEAN,
    AUDIO_CONTROL_INTEGER,
    AUDIO_CONTROL_ENUMERATED,
    AUDIO_CONTROL_VENDOR
} audio_control_type_t;

typedef enum audio_control_purpose {
    AUDIO_PURPOSE_VENDOR,
    AUDIO_PURPOSE_VOLUME,
    AUDIO_PURPOSE_ROUTING,
    AUDIO_PURPOSE_MUTE,
    AUDIO_PURPOSE_EFFECT
} audio_control_purpose_t;


typedef struct audio_control_info {
    int id;
    char name[128]; // TODO: maybe make this a two-pass ioctl
    audio_control_type_t type;
    audio_control_purpose_t purpose;

    union {
        struct {
            long min;
            long max;
            long step;
        } integer;

        struct {
            unsigned int nitems;
            unsigned int item; // the item you want to get
            char name[64];
        } enumerated;

        struct {
            size_t length;
        } vendor;
    } value;
} audio_control_info_t;

typedef struct audio_control_value {
    int id;

    union {
        struct {
            bool value;    
        } boolean;

        struct {
            long value;
        } integer;

        struct {
            unsigned int value;
        } enumerated;

        struct {
            unsigned char *value;
        } vendor;
    } value;
} audio_control_value_t;

typedef enum {
    AUDIO_FORMAT_S16_LE,
    AUDIO_FORMAT_S24_LE,
    AUDIO_FORMAT_S32_LE,
    AUDIO_FORMAT_F32_LE,
} audio_format_t;

typedef struct {
    uint32_t sample_rate;       // sample rate in Hz
    uint16_t channels;          // amount of channels per audio (affects the calculated frame size)
    audio_format_t format;

    uint32_t period_size;       // size per period in frames
    uint32_t periods;           // target periods (you will get an error if this is too high)
    uint32_t start_thres;       // threshold of frames to write before starting
} audio_stream_config_t;

typedef struct audio_device_info {
    int nstreams;
    int ncontrols;
} audio_device_info_t;


#endif
