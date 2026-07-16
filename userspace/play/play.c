/**
 * @file userspace/play/play.c
 * @brief Simple dump play command that uses Symphony to play a WAV file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <ethereal/symphony.h>

typedef struct {
    char riff_id[4];
    uint32_t riff_size;
    char wave_id[4];
    char fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_id[4];
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

void usage() {
    printf("Usage: play [OPTION] [WAV_FILE]\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("play (Ethereal miniutils) 1.10\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}

void stream_audio(int wav_fd, wav_header_t *wav, symphony_buffer_id_t buffer_id) {
    size_t input_samples_per_block = 1024 * wav->num_channels;
    int16_t *input_staging = malloc(input_samples_per_block * sizeof(int16_t));
    double step_ratio = (double)wav->sample_rate / 48000.0;
    size_t output_samples_per_block = (1024.0 / step_ratio) * wav->num_channels;
    int16_t *output_staging = malloc(output_samples_per_block * sizeof(int16_t));
    
    printf("start stream: (%d Hz, %d channels) resampled to 48000Hz\n", wav->sample_rate, wav->num_channels);

    if (symphony_start(buffer_id) < 0) {
        fprintf(stderr, "play: failed to start playback: %s\n", strerror(errno));
        free(input_staging);
        free(output_staging);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(wav_fd, input_staging, input_samples_per_block * sizeof(int16_t))) > 0) {
        size_t actual_input_samples = bytes_read / sizeof(int16_t);
        size_t actual_input_frames = actual_input_samples / wav->num_channels;
        
        size_t actual_output_frames = (size_t)((double)actual_input_frames / step_ratio);
        if (actual_output_frames == 0) {
            break;
        }

        // we have to resample this garbage since it needs to be 48000Hz
        for (size_t f = 0; f < actual_output_frames; f++) {
            double src_frame_idx = (double)f * step_ratio;
            size_t base_frame = (size_t)src_frame_idx;
            float fraction = (float)(src_frame_idx - (double)base_frame);

            size_t next_frame = base_frame + 1;
            if (next_frame >= actual_input_frames) {
                next_frame = base_frame;
            }

            for (size_t c = 0; c < wav->num_channels; c++) {
                float s1 = (float)input_staging[base_frame * wav->num_channels + c];
                float s2 = (float)input_staging[next_frame * wav->num_channels + c];

                float interpolated = s1 + fraction * (s2 - s1);
                output_staging[f * wav->num_channels + c] = (int16_t)interpolated;
            }
        }

        // now we can send it to the audio server
        symphony_write(buffer_id, output_staging, actual_output_frames * wav->num_channels);
    }

    symphony_drain(buffer_id);
    symphony_stop(buffer_id);

    free(input_staging);
    free(output_staging);
}

int main(int argc, char *argv[]) {
    int opt;
    wav_header_t wav;
    symphony_buffer_id_t buffer_id;

    static struct option long_options[] = {
        { "help",      no_argument, 0, 'h' },
        { "version",   no_argument, 0, 'V' },
        { 0, 0, 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "fhVv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                usage();
            case 'V':
                version();
            default:
                fprintf(stderr, "Usage: %s [-fv] <wav_file>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc - optind < 1) {
        fprintf(stderr, "Usage: %s [-fv] <wav_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int wav_fd = open(argv[optind], O_RDONLY);
    if (wav_fd < 0) {
        perror("play: open source file");
        return EXIT_FAILURE;
    }

    if (read(wav_fd, &wav, sizeof(wav_header_t)) < (ssize_t)sizeof(wav_header_t)) {
        fprintf(stderr, "play: failed to read WAV header\n");
        close(wav_fd);
        return EXIT_FAILURE;
    }

    if (strncmp(wav.riff_id, "RIFF", 4) != 0 || strncmp(wav.wave_id, "WAVE", 4) != 0 || wav.audio_format != 1 ||  wav.bits_per_sample != 16) {
        fprintf(stderr, "play: unsupported format (Must be uncompressed 16-bit PCM)\n");
        close(wav_fd);
        return EXIT_FAILURE;
    }
    

    if (symphony_connect() < 0) {
        fprintf(stderr, "play: connection to Symphony daemon failed\n");
        close(wav_fd);
        return EXIT_FAILURE;
    }

    // create the symphony buffer
    buffer_id = symphony_addBuffer(SYMPHONY_STREAM_ANY, AUDIO_FORMAT_S16_LE, 48000, wav.num_channels, 65536 / sizeof(int16_t));
    if (buffer_id < 0) {
        fprintf(stderr, "play: failed to allocate audio buffer (Error %d)\n", buffer_id);
        close(wav_fd);
        return EXIT_FAILURE;
    }

    // get the shmbuf
    symphony_buffer_t *shm_buf = symphony_getBuffer(buffer_id);
    if (!shm_buf) {
        fprintf(stderr, "play: failed to map shared memory ring buffer\n");
        close(wav_fd);
        return EXIT_FAILURE;
    }

    stream_audio(wav_fd, &wav, buffer_id);

    close(wav_fd);
    return EXIT_SUCCESS;
}
