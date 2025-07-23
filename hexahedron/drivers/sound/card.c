/**
 * @file hexahedron/drivers/sound/card.c
 * @brief Sound card subsystem
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
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>

/* List of sound cards */
list_t *sound_card_list = NULL;

/* Default sound card for /device/audio */
sound_card_t *sound_default_card = NULL;

/* /device/audio */
int sound_open(fs_node_t *node, unsigned int flags);
int sound_close(fs_node_t *node);
ssize_t sound_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer);
ssize_t sound_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer); 

static fs_node_t sound_default_device = {
    .name = "audio",
    .mask = 0666,
    .flags = VFS_BLOCKDEVICE,
    .open = sound_open,
    .close = sound_close,
    .write = sound_write,
    .read = sound_read,
    .dev = NULL,
};

/* Last used device index */
static int sound_last_index = 0;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "SOUND:CARD", __VA_ARGS__)

/**
 * @brief Sound card open method
 * @param node The node to open
 * @param flags Flags opened with
 */
int sound_open(fs_node_t *node, unsigned int flags) {
    return 0;
}

/**
 * @brief Sound card close method
 * @param node The node to close
 */
int sound_close(fs_node_t *node) {
    return 0;
}

/**
 * @brief Sound card node read method
 * @param node The node read from
 * @param off Unused
 * @param size Size of data to write
 * @param buffer Buffer of data to write
 */
ssize_t sound_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    return 0;
}

/**
 * @brief Sound card node write method
 * @param node The node written to
 * @param off Unused
 * @param size Size of data to write
 * @param buffer Buffer of data to write
 */
ssize_t sound_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!CARD(node)) {
        // Try default?
        if (!sound_default_card) return -ENODEV;
        node->dev = (void*)sound_default_card;
    }

    if (size < sizeof(sound_card_play_request_t)) return -EINVAL;
    return (mixer_request(CARD(node), buffer));
}

/**
 * @brief Mount default audio device node
 */
void audio_mount() {
    vfs_mount(&sound_default_device, "/device/audio");
}

/**
 * @brief Create sound card object
 * @param name The name of the card
 * @param sound_format The format of sound in the card
 * @param sample_rate The sample rate of the card
 * @returns A new sound card object
 */
sound_card_t *sound_createCard(char *name, uint8_t sound_format, uint32_t sample_rate) {
    sound_card_t *card = kzalloc(sizeof(sound_card_t));
    card->name = name;
    card->sound_format = sound_format;
    card->sample_rate = sample_rate;
    card->sound_data = list_create("sound data");
    
    card->node = kzalloc(sizeof(fs_node_t));
    snprintf(card->node->name, 256, "audio%d", sound_last_index++);
    card->node->flags = VFS_BLOCKDEVICE;
    card->node->mask = 0666;
    card->node->open = sound_open;
    card->node->close = sound_close;
    card->node->read = sound_read;
    card->node->write = sound_write;
    card->node->dev = (void*)card;

    return card;
}

/**
 * @brief Register the sound card object
 * @param card The card to register
 * @returns 0 on success
 * 
 * Will mount the card to whatever node->name is
 */
int sound_registerCard(sound_card_t *card) {
    // Mount the card
    char mount_path[256+8];
    snprintf(mount_path, 256+8, "/device/%s", card->node->name);
    vfs_mount(card->node, mount_path);

    if (!sound_card_list) sound_card_list = list_create("sound card list");
    list_append(sound_card_list, card);

    // Set card as default if not set already
    if (!sound_default_card) sound_default_card = card;

    return 0;
}

/**
 * @brief Add a knob to a card
 * @param card The card to add a knob to
 * @param name Name of the knob
 * @param type Type of the knob
 * @param read Read method for the knob
 * @param write Write method for the knob
 * @returns Knob object on success or NULL
 */
sound_knob_t *sound_addKnob(sound_card_t *card, char *name, uint8_t type, sound_knob_read_t read, sound_knob_write_t write) {
    return NULL;
}
