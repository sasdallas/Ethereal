/**
 * @file hexahedron/drivers/sound/mixer.c
 * @brief Hexahedron audio mixer system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/sound/mixer.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SOUND:MIXER", __VA_ARGS__)


/**
 * @brief Initialize the mixer system
 */
void mixer_init() {
    audio_mount();
}

/**
 * @brief Debug function to convert sound type to string
 * @param type The type of sound
 */
static char *mixer_soundTypeToString(uint8_t type) {
    switch (type) {
        case SOUND_FORMAT_S16PCM:
            return "signed 16-bit PCM";
        case SOUND_FORMAT_S24PCM:
            return "signed 24-bit PCM";
        case SOUND_FORMAT_S32PCM:
            return "signed 32-bit PCM";
        case SOUND_FORMAT_U8PCM:
            return "unsigned 8-bit PCM";
        default:
            return "unknown";
    }
}

/**
 * @brief Handle type of sound request
 * @param card The sound card the request was sent to
 * @param buffer The request buffer
 * @returns 0 on success
 */
int mixer_request(sound_card_t *card, void *buffer) {
    sound_card_play_request_t *play_req = (sound_card_play_request_t*)buffer;
    
    // What type of request is it?
    switch (play_req->type) {
        case SOUND_CARD_REQUEST_TYPE_PLAY:
            // PLAY request    
            LOG(INFO, "Play sound request for %s sound at %d sample rate (need to convert to card %s at %d sample rate)\n", mixer_soundTypeToString(play_req->sound_format), play_req->sample_rate, mixer_soundTypeToString(card->sound_format), card->sample_rate);

            // Do we already match the card's target?
            if (play_req->sample_rate != card->sample_rate || play_req->sound_format != card->sound_format) {
                LOG(ERR, "Must convert card data\n");
                return 1;
            }

            spinlock_acquire(&card->sound_data_lock);
            card->start(card);
            sound_card_buffer_data_t *data = kmalloc(sizeof(sound_card_buffer_data_t) + play_req->size);
            data->size = play_req->size;
            memcpy(data->data, play_req->data, play_req->size);
            list_append(card->sound_data, (void*)data);

            spinlock_release(&card->sound_data_lock);
            return 0;

        case SOUND_CARD_REQUEST_TYPE_STOP:
            LOG(INFO, "Stop sound\n");
            return card->stop(card);

        default:
            LOG(ERR, "Unimplemented request: %d\n", play_req->type);
            break;
    }

    return 1;
}

/**
 * @brief Get a buffer of sound data to play until new ones have been processed
 * @param card The card to read data from
 * @returns Sound card buffer data or NULL if absolutely no data is available
 * 
 * If pending data is available in the card conversion queue, it will be syncronously converted.
 * @note Remember to free the returned object when finished
 */
sound_card_buffer_data_t *mixer_buffer(sound_card_t *card) {
    // Does the card have any pending data?
    spinlock_acquire(&card->sound_data_lock);
    if (card->sound_data->length) {
        // Yes, pull data
        node_t *n = list_popleft(card->sound_data);
        sound_card_buffer_data_t *data = (sound_card_buffer_data_t*)n->value;
        kfree(n);
        spinlock_release(&card->sound_data_lock);
        return data;
    } else {
        // TODO: No, can we pull from conversion queue?
    }

    // Nothing available
    spinlock_release(&card->sound_data_lock);
    return NULL;
}