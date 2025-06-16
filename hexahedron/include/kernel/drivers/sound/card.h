/**
 * @file hexahedron/include/kernel/drivers/sound/card.h
 * @brief Sound card system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_SOUND_CARD_H
#define DRIVERS_SOUND_CARD_H

/**** INCLUDES ****/
#include <kernel/drivers/sound/knob.h>
#include <kernel/drivers/sound/audio.h>
#include <kernel/misc/spinlock.h>
#include <kernel/fs/vfs.h>
#include <structs/list.h>

/**** TYPES ****/

/**
 * @brief Sound card buffer data
 */
typedef struct sound_card_buffer_data {
    size_t size;                // Size of data buffer
    uint8_t data[];             // Data of the samples
} sound_card_buffer_data_t;

struct sound_card;

/**
 * @brief *Asyncronously* begin playing sound and processing entries in sound_data
 * @param card The card being started
 * @returns 0 on success
 */
typedef int (*sound_card_start_t)(struct sound_card *card);

/**
 * @brief Stop the sound card sound
 * @param card The card being stopped
 * @returns 0 on success
 */
typedef int (*sound_card_stop_t)(struct sound_card *card);

/**
 * @brief Sound card write method
 * @param card The card being written to
 * @param samples Amount of samples
 */
typedef struct sound_card {
    // GENERAL
    char *name;                 // Name of the sound card
    fs_node_t *node;            // Filesystem node
    list_t *knob_list;          // List of knobs in the sound card

    // SOUND DATA
    uint8_t sound_format;       // Format of sound accepted by the card
    uint32_t sample_rate;       // Sample rate of the card

    // REQUESTS
    list_t *sound_data;         // Fully converted requests to play sound data
    spinlock_t sound_data_lock; // Lock for sound data

    // FUNCTIONS
    sound_card_start_t start;   // Start function
    sound_card_stop_t stop;     // Stop function

    // DRIVER
    void *dev;                  // Driver-specific
} sound_card_t;

/**** MACROS ****/

#define CARD(node) ((sound_card_t*)node->dev)

/**** FUNCTIONS ****/

/**
 * @brief Mount default audio device node
 */
void audio_mount();

/**
 * @brief Create sound card object
 * @param name The name of the card
 * @param sound_format The format of sound in the card
 * @param sample_rate The sample rate of the card
 * @returns A new sound card object
 */
sound_card_t *sound_createCard(char *name, uint8_t sound_format, uint32_t sample_rate);

/**
 * @brief Register the sound card object
 * @param card The card to register
 * @returns 0 on success
 * 
 * Will mount the card to whatever node->name is
 */
int sound_registerCard(sound_card_t *card);

/**
 * @brief Add a knob to a card
 * @param card The card to add a knob to
 * @param name Name of the knob
 * @param type Type of the knob
 * @param read Read method for the knob
 * @param write Write method for the knob
 * @returns Knob object on success or NULL
 */
sound_knob_t *sound_addKnob(sound_card_t *card, char *name, uint8_t type, sound_knob_read_t read, sound_knob_write_t write);

#endif