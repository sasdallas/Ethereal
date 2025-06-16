/**
 * @file hexahedron/include/kernel/drivers/sound/audio.h
 * @brief Audio types
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_SOUND_AUDIO_H
#define DRIVERS_SOUND_AUDIO_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* Encoding of audio */
#define SOUND_FORMAT_S16PCM         1   // Signed 16-bit PCM
#define SOUND_FORMAT_S24PCM         2   // Signed 24-bit PCM
#define SOUND_FORMAT_S32PCM         3   // Signed 32-bit PCM
#define SOUND_FORMAT_U8PCM          4   // Unsigned 8-bit PCM

/* Hz */
#define SOUND_RATE_8000HZ           8000
#define SOUND_RATE_11025HZ          11025
#define SOUND_RATE_16000HZ          16000
#define SOUND_RATE_22050HZ          22050
#define SOUND_RATE_32000HZ          32000
#define SOUND_RATE_44100HZ          44100
#define SOUND_RATE_48000HZ          48000
#define SOUND_RATE_88200HZ          88200
#define SOUND_RATE_96000HZ          96000

#endif