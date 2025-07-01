/**
 * @file userspace/miniutils/play.c
 * @brief Mini utility that writes WAV files to /device/audio
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <kernel/drivers/sound/mixer.h>

#include <sys/ioctl.h>

int main(int argc, char * argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: play [FILE]\n");
        return 1;
    }

    int dev = open("/device/audio", O_WRONLY);
    int file = open(argv[1], O_RDONLY);
    
    if (dev < 0) {
        fprintf(stderr, "play: /device/audio: %s\n", strerror(errno));
        return 1;
    }

    if (file == -1) {
        fprintf(stderr, "play: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    char buf[0x1000+sizeof(sound_card_play_request_t)];
    sound_card_play_request_t *req = (sound_card_play_request_t*)buf;
    req->sample_rate = 48000;
    req->sound_format = SOUND_FORMAT_S16PCM;
    req->type = SOUND_CARD_REQUEST_TYPE_PLAY;
    int r;

    while ((r = read(file, req->data, 0x1000))) {
        req->size = r;
        write(dev, req, r);
    }
    return 0;
}