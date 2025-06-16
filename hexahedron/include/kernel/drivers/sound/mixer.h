/**
 * @file hexahedron/include/kernel/drivers/sound/mixer.h
 * @brief Sound mixer header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_SOUND_MIXER_H
#define DRIVERS_SOUND_MIXER_H

/**** INCLUDES ****/
#include <kernel/drivers/sound/card.h>

/**** DEFINITIONS ****/

#define SOUND_CARD_REQUEST_TYPE_PLAY        1
#define SOUND_CARD_REQUEST_TYPE_STOP        2

/**** TYPES ****/

/**
 * @brief Sound card write request
 * 
 * Requests for a specific type of audio to play on the sound card. The kernel will handle downsampling
 * and conversion while you just give us the raw audio data.
 */
typedef struct sound_card_play_request {
    uint8_t type;                   // SOUND_CARD_REQUEST_TYPE_PLAY
    uint8_t sound_format;           // SOUND_FORMAT_xx
    uint32_t sample_rate;           // Sampling rate
    size_t size;                    // Size of data
    uint8_t data[];                 // Data to play
} sound_card_play_request_t;


/**
 * @brief Stop request
 * 
 * Immediately stop any audio that is playing
 */
typedef struct sound_card_stop_request {
    uint8_t type;                   // SOUND_CARD_REQUEST_TYPE_STOP
} sound_card_stop_request_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the mixer
 */
void mixer_init();

/**
 * @brief Handle type of sound request
 * @param card The sound card the request was sent to
 * @param buffer The request buffer
 * @returns 0 on success
 */
int mixer_request(sound_card_t *card, void *buffer);

/**
 * @brief Get a buffer of sound data to play until new ones have been processed
 * @param card The card to read data from
 * @returns Sound card buffer data or NULL if absolutely no data is available
 * 
 * If pending data is available in the card conversion queue, it will be syncronously converted.
 * @note Remember to free the returned object when finished
 */
sound_card_buffer_data_t *mixer_buffer(sound_card_t *card);

#endif