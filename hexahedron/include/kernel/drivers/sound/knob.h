/**
 * @file hexahedron/include/kernel/drivers/sound/knob.h
 * @brief Knob on a sound card
 * 
 * Each "knob" represents a control of sort, such as master volume, input gain, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_SOUND_KNOB_H
#define DRIVERS_SOUND_KNOB_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define SOUND_KNOB_MASTER           1       // Master knob, controls all volume
#define SOUND_KNOB_OTHER            255     // Other type of knob that we don't know

/**** TYPES ****/

struct sound_card;
struct sound_knob;

/**
 * @brief Write method for a sound knob
 * @param knob The knob being written to
 * @param value The value to write to the knob
 * @returns 0 on success
 */
typedef int (*sound_knob_write_t)(struct sound_knob *knob, uint32_t value);

/**
 * @brief Read method for a sound knob
 * @param knob The knob being read from
 * @returns The value of the knob
 */
typedef uint32_t (*sound_knob_read_t)(struct sound_knob *knob);

/**
 * @brief Sound knob
 * 
 * Each knob on the card represents a volume wheel of sorts. It's a contro tha accepts a 
 */
typedef struct sound_knob {
    char *name;                             // Name of the knob
    struct sound_card *card;                // Sound card this knob is apart of
    uint8_t type;                           // Type of the knob
    uint32_t knob_id;                       // ID of this knob (auto-generated, don't touch)
    uint32_t dev_knob_id;                   // Device-specific knob ID
    sound_knob_read_t read;                 // Sound knob read method
    sound_knob_write_t write;               // Sound knob write method
} sound_knob_t;

#endif