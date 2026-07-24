/**
 * @file userspace/lib/symphony/symphony.c
 * @brief Symphony audio library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <ethereal/symphony.h>
#include <structs/hashmap.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

/* Symphony socket */
static int symphony_socket = -1;

/* Map */
hashmap_t *buffer_map = NULL;

struct symphony_buffer_info {
    symphony_buffer_t *buf;
    size_t samples;
    unsigned short channels;
    audio_format_t format;
};

/**
 * @brief Initialize connection with the Symphony audio library
 * @returns 0 on success
 */
int symphony_connect() {
    if (symphony_socket >= 0) return 0; // already connected
    
    symphony_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (symphony_socket < 0) {
        fprintf(stderr, "libsymphony: socket: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_un tgt = {
        .sun_family = AF_UNIX,
        .sun_path = SYMPHONY_SOCKET_PATH,
    };

    if (connect(symphony_socket, (const struct sockaddr*)&tgt, sizeof(struct sockaddr_un)) < 0) {
        fprintf(stderr, "libsymphony: connect: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * @brief Send full packet
 */
static int symphony_sendFull(void *data, size_t size) {
    return send(symphony_socket, (char*)data, size, 0);
}

/**
 * @brief Receive full packet
 */
static int symphony_recvFull(void *data, size_t size) {
    return recv(symphony_socket, (char*)data, size, 0);
}

/**
 * @brief Send packet with optional data and receive optional response
 * @param type The type to send
 * @param data Optional data
 * @param data_size Optional data size
 * @param resp Optional resp
 * @param resp_size Optional resp size
 * @returns Error code
 */
static int symphony_sendPacket(symphony_packet_type_t type, void *data, size_t data_size, void *resp, size_t resp_size) {
    if (symphony_socket == -1) {
        int r = symphony_connect();
        if (r != 0) return r;
    }

    symphony_header_t hdr = {
        .magic = SYMPHONY_MAGIC,
        .size = data_size,
        .type = type
    };

    ssize_t r = symphony_sendFull(&hdr, sizeof(symphony_header_t));
    if (r != sizeof(symphony_header_t)) {
        return r;
    }

    if (data_size) {
        r = symphony_sendFull(data, data_size);
        if (r != data_size) {
            return r;
        }
    }

    symphony_resp_t sym_resp;
    r = symphony_recvFull(&sym_resp, sizeof(symphony_resp_t));
    if (r != sizeof(symphony_resp_t)) {
        return r;
    }

    if (sym_resp.magic != SYMPHONY_MAGIC_RESP) {
        fprintf(stderr, "libsymphony: bad magic 0x%x\n", sym_resp.magic);
        errno = EBADMSG; // complete wrong error code btw
        return -1;
    }

    if (sym_resp.error != 0) {
        errno = sym_resp.error;
        return -1;
    }

    if (resp_size) {
        r = symphony_recvFull(resp, resp_size);
        if (r != resp_size) {
            return r;
        }
    }

    return 0;
}

/**
 * @brief Get the size of a sample
 * @param format The format
 * @param channels The channels
 */
size_t symphony_getSampleSize(audio_format_t format, uint16_t channels) {
    switch (format) {
        case AUDIO_FORMAT_S16_LE: return 2 * channels;
        case AUDIO_FORMAT_S24_LE: return 3 * channels;
        default: return 4 * channels;
    }
}

/**
 * @brief Get information on the Symphony server
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getInfo(symphony_info_t *info) {
    return symphony_sendPacket(SYMPHONY_PACKET_GET_INFO, NULL, 0, info, sizeof(symphony_info_t));
}

/**
 * @brief Get information on a specific Symphony device
 * @param device The index of the device to get
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getDevice(int device, symphony_device_info_t *info) {
    symphony_packet_get_device_t pkt = { .device_id = device };
    return symphony_sendPacket(SYMPHONY_PACKET_GET_DEVICE, &pkt, sizeof(pkt), info, sizeof(symphony_device_info_t));
}

/**
 * @brief Get information on a specific Symphony stream
 * @param stream The ID of the stream to get
 * @param info The output information buffer
 * @returns 0 on success
 */
int symphony_getStream(unsigned short stream, symphony_stream_info_t *info) {
    symphony_packet_get_stream_t pkt = { .stream_id = stream };
    return symphony_sendPacket(SYMPHONY_PACKET_GET_STREAM, &pkt, sizeof(pkt), info, sizeof(symphony_stream_info_t));
}

/**
 * @brief Create and attach a buffer to a Symphony stream
 * @param stream_id The stream ID to attach the buffer to
 * @param format The audio format of the buffer
 * @param sample_rate The sample rate of the buffer
 * @param channels The amount of channels in the buffer
 * @param size The size of the buffer, in samples
 * @returns Buffer ID handle on success, less than 0 on error
 */
symphony_buffer_id_t symphony_addBuffer(uint16_t stream_id, audio_format_t format, uint32_t sample_rate, uint16_t channels, size_t size) {
    // First create the buffer
    size_t sample_size = symphony_getSampleSize(format, channels);
    size_t buflen = (size * sample_size) + sizeof(symphony_buffer_t);
    int buf_fd = shared_new(buflen, SHARED_DEFAULT);
    if (buf_fd < 0) {
        fprintf(stderr, "libsymphony: shared_new: %s\n", strerror(errno));
        return -1;
    }

    if (!buffer_map) {
        buffer_map = hashmap_create_int("symphony buffer map", 5);
    }


    symphony_buffer_t *buf = mmap(NULL, buflen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, buf_fd, 0);
    if (buf == MAP_FAILED) {
        fprintf(stderr, "libsymphony: mmap: %s\n", strerror(errno));
        return -1;
    }

    buf->head = 0;
    buf->tail = 0;

    // TODO: move this to server
    if (stream_id == SYMPHONY_STREAM_ANY) {
        char *e = getenv("SYMPHONY_DEFAULT_STREAM");
        if (e) {
            stream_id = strtol(e, NULL, 16);
        } else {
            stream_id = SYMPHONY_STREAM(0,0);
        }
    }

    symphony_packet_add_buffer_t pkt = {
        .stream_id = stream_id,
        .channels = channels,
        .sample_rate = sample_rate,
        .format = format,
        .buflen = buflen,
        .bufkey = shared_key(buf_fd),
    };

    symphony_buffer_id_t id;
    int r = symphony_sendPacket(SYMPHONY_PACKET_ADD_BUFFER, &pkt, sizeof(pkt), &id, sizeof(id));
    if (r != 0) {
        return r;
    }

    struct symphony_buffer_info *i = malloc(sizeof(struct symphony_buffer_info));
    i->buf = buf;
    i->samples = size;
    i->format = format;
    i->channels = channels;

    hashmap_set(buffer_map, (void*)(uintptr_t)id, i);
    return id;
}

/**
 * @brief Get buffer pointer
 * @param id The buffer ID to get
 * @returns Pointer to Symphony buffer object or NULL
 */
symphony_buffer_t *symphony_getBuffer(symphony_buffer_id_t id) {
    if (!buffer_map) return NULL;
    struct symphony_buffer_info *info = hashmap_get(buffer_map, (void*)(uintptr_t)id);
    if (!info) return NULL;
    return info->buf;
}

/**
 * @brief Start playback
 * @param id The buffer ID to start playback on
 * @returns 0 on success
 */
int symphony_start(symphony_buffer_id_t id) {
    symphony_packet_start_t pkt = { .buffer_id = id };
    return symphony_sendPacket(SYMPHONY_PACKET_START, &pkt, sizeof(pkt), NULL, 0);
}

/**
 * @brief Stop playback
 * @param id The buffer ID to stop playback on
 * @returns 0 on success
 */
int symphony_stop(symphony_buffer_id_t id) {
    symphony_packet_stop_t pkt = { .buffer_id = id };
    return symphony_sendPacket(SYMPHONY_PACKET_STOP, &pkt, sizeof(pkt), NULL, 0);
}


/**
 * @brief Drain playback
 * @param id The buffer ID to drain
 * @returns 0 on success
 */
int symphony_drain(symphony_buffer_id_t id) {
    struct symphony_buffer_info *i = hashmap_get(buffer_map, (void*)(uintptr_t)id);
    if (!i) return -EINVAL;

    symphony_buffer_t *buffer = i->buf;

    for (;;) {
        uint64_t head = __atomic_load_n(&buffer->head, __ATOMIC_ACQUIRE);
        uint64_t tail = __atomic_load_n(&buffer->tail, __ATOMIC_ACQUIRE);
        if (head >= tail) break;
        usleep(10000);
    }

    return 0;
}

/**
 * @brief Write samples
 * @param buffer The buffer to write the samples to
 * @param samples The samples to write
 * @param num The amount of samples to write
 * @warning This will block if the audio server isnt updating fast enough
 */
void symphony_write(symphony_buffer_id_t bufid, void *samples, size_t num) {
    int16_t *src = (int16_t *)samples;
    size_t written = 0;

    // TODO: this loop must be improved
    struct symphony_buffer_info *i = hashmap_get(buffer_map, (void*)(uintptr_t)bufid);
    if (!i) return;

    symphony_buffer_t *buffer = i->buf;
    size_t max_samples =  i->samples * i->channels;

    while (written < num) {
        uint64_t head = __atomic_load_n(&buffer->head, __ATOMIC_ACQUIRE);
        uint64_t tail = __atomic_load_n(&buffer->tail, __ATOMIC_ACQUIRE);

        size_t used_space = tail - head;
        size_t free_space = max_samples - used_space;

        if (free_space == 0) {
            // TODO: use futex
            usleep(2000); 
            continue;
        }

        size_t chunk = num - written;
        if (chunk > free_space) {
            chunk = free_space;
        }

        for (size_t i = 0; i < chunk; i++) {
            buffer->samples[(tail + i) % max_samples] = src[written + i];
        }

        tail += chunk;
        written += chunk;

        __atomic_store_n(&buffer->tail, tail, __ATOMIC_RELEASE);
    }
}
