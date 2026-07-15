/**
 * @file userspace/symphonyd/symphonyd.c
 * @brief Ethereal audio server
 * 
 * @todo Resampling, better mixing, allow clients to set their volume
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "symphonyd.h"
#include <ethereal/symphony.h>
#include <structs/hashmap.h>
#include <ethereal/shared.h>
#include <ethereal/audio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>

#define APP_NAME "symphonyd"
#include <ethereal/log.h>

/* Versioning info */
#define SYMPHONY_VERSION_MAJOR 1
#define SYMPHONY_VERSION_MINOR 0
#define SYMPHONY_VERSION_LOWER 0

/* Symphony socket */
static int server_socket = -1;

/* Client/buffer list */
server_client_t *client_list = NULL;
server_buffer_t *buffer_list = NULL;
pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t buffer_list_lock = PTHREAD_MUTEX_INITIALIZER;

/* Device list */
list_t devices = LIST_INITIALIZER;

/* Quick maps */
hashmap_t *buffer_map = NULL;
hashmap_t *stream_map = NULL; // keyed via SYMPHONY_STREAM

/* IDs */
static int buffer_last_id = 0;
static int device_last_id = 0;

// get device
server_device_t *device_find(int device_id) {
    // the device list will never change for now
    foreach(n, &devices) {
        server_device_t *dev = n->value;
        if (dev->id == device_id) {
            return dev;
        }
    }

    return NULL;
}

// get stream
server_stream_t *stream_find(unsigned short stream) {
    return hashmap_get(stream_map, (void*)(uintptr_t)stream);
}

// stream manager thread
void *stream_thread(void *arg) {
    server_stream_t *stream = arg;
    bool was_playing = false;

    // pre-compute
    size_t sample_size = 0;
    size_t period_size = 0;
    if (stream->config.format == AUDIO_FORMAT_S16_LE) {
        sample_size = 2 * stream->config.channels;
    } else if (stream->config.format == AUDIO_FORMAT_S24_LE) {
        sample_size = 3 * stream->config.channels;
    } else {
        sample_size = 4 * stream->config.channels;
    }

    period_size = sample_size * stream->config.period_size;
    TRACE_DEBUG("sample_size = %d, period_size = %d\n", sample_size, period_size);

    // Main processor loop
    for (;;) {
        pthread_mutex_lock(&stream->lock);

    _retry:
        while (stream->buffers.length == 0) {
            pthread_cond_wait(&stream->avail, &stream->lock);
        } 
        
        bool anything = false;
        memset(stream->samples, 0, sizeof(stream->samples));
        size_t max_gotten = 0;
        foreach(n, &stream->buffers) {
            server_buffer_t *buf = n->value;
            if (buf->playing == false) continue;
            anything = true;

            // Yes we got something, gather samples.
            uint64_t head = __atomic_load_n(&buf->buffer->head, __ATOMIC_ACQUIRE);
            uint64_t tail = __atomic_load_n(&buf->buffer->tail, __ATOMIC_ACQUIRE);

            // TODO: Collect these differently, since format may be independent
            size_t avail = tail - head;
            size_t max_samples = (buf->buffer_size - sizeof(symphony_buffer_t)) / sizeof(int16_t);

            if (avail > 2048) {
                avail = 2048;
            }

            // The magic happens here
            for (unsigned i = 0; i < avail; i++) {
                float sample = (float)buf->buffer->samples[(i+head) % max_samples];
                sample = sample * buf->client->volume;

                stream->samples[i] += sample;
                
            }

            __atomic_add_fetch(&buf->buffer->head, avail, __ATOMIC_RELEASE);

            if (avail > max_gotten) {
                max_gotten = avail;
            }
        }

        // Did we get anything?
        if (!anything) {
            if (was_playing) {
                // No and the stream is still going, drain it
                audio_stream_request_t req = AUDIO_STREAM_DRAIN;
                ioctl(stream->fd, IO_AUDIO_REQUEST, &req);
            }

            was_playing = false;

            // TODO: This is really ugly, but avail is only signalled on buffer start.
            pthread_cond_wait(&stream->avail, &stream->lock);
            goto _retry;
        } else if (!was_playing) {
            // Yes, and the stream was stopped. Reconfigure it.
            ioctl(stream->fd, IO_AUDIO_SET_CONFIG, &stream->config);
            was_playing = true;
        }

        pthread_mutex_unlock(&stream->lock);

        if (!max_gotten) {
            usleep(65000);
            continue;
        }

        // Time to clamp
        for (unsigned i = 0; i < max_gotten; i++) {
            int32_t sample = stream->samples[i];
            if (sample > 32767) sample = 32767;
            if (sample < -32767) sample = -32767;
            stream->samples_out[i] = sample;
        }

        // TRACE_DEBUG("Writing %d samples.\n", max_gotten);
        write(stream->fd, stream->samples_out, max_gotten * sizeof(int16_t));
    }
}

// configure a stream
int stream_configure(server_stream_t *stream, audio_stream_config_t *config) {
    int r = ioctl(stream->fd, IO_AUDIO_SET_CONFIG, config);
    if (r != 0) {
        TRACE_WARN("Stream configuration @ %dHz %dchan %dperiod %dperiodsz fmt %d failed\n", config->sample_rate, config->channels, config->periods, config->period_size, config->format);
        TRACE_WARN("Error: %s\n", strerror(errno));
        return 1;
    }


    TRACE_INFO("Stream configuration @ %dHz %dchan %dperiod %dperiodsz fmt %d successful\n", config->sample_rate, config->channels, config->periods, config->period_size, config->format);
    memcpy(&stream->config, config, sizeof(audio_stream_config_t));
    return 0;
}

// init stream
int stream_init(server_stream_t *stream) {
    audio_stream_config_t config = {
        .channels = 2,
        .format = AUDIO_FORMAT_S16_LE,
        .periods = 2,
        .period_size = 1024,
        .sample_rate = 48000,
        .start_thres = 10
    };

    if (stream_configure(stream, &config) != 0) {
        TRACE_ERROR("Failed to use default configuration and probing not implemented\n");
        return 1;
    }

    pthread_create(&stream->thread, NULL, stream_thread, stream);
    return 0;
}

// remove a client
void client_remove(server_client_t *cli) {
    pthread_mutex_lock(&client_list_lock);
    if (cli->prev) cli->prev->next = cli->next;
    if (cli->next) cli->next->prev = cli->prev;
    if (cli == client_list) client_list = cli->next;
    pthread_mutex_unlock(&client_list_lock);

    pthread_mutex_lock(&buffer_list_lock);
    server_buffer_t *b = buffer_list;
    server_buffer_t *prev = NULL;
    while (b) {
        server_buffer_t *nxt = b->next;
        if (b->client == cli) {
            if (prev) prev->next = nxt;
            else buffer_list = nxt;

            pthread_mutex_lock(&b->stream->lock);
            list_delete(&b->stream->buffers, &b->node);
            pthread_mutex_unlock(&b->stream->lock);

            close(b->shmfd);
            munmap(b->buffer, b->buffer_size);
            free(b);
        }

        b = nxt;
    }
    pthread_mutex_unlock(&buffer_list_lock);

    close(cli->fd);
    free(cli);
}

// send response code
void client_sendResponse(server_client_t *cli, symphony_header_t *hdr, int error) {
    symphony_resp_t resp = {
        .magic = SYMPHONY_MAGIC_RESP,
        .type = hdr->type,
        .error = error
    };

    send(cli->fd, &resp, sizeof(symphony_resp_t), 0);
}

// send resp data
void client_sendResponseData(server_client_t *cli, void *data, size_t len) {
    send(cli->fd, data, len, 0);
}

// client receive full
ssize_t client_recvFull(server_client_t *cli, void *data, size_t size) {
    size_t rem = size;
    size_t i = 0;
    while (rem) {
        ssize_t r = recv(cli->fd, (char*)data + i, rem, 0);
        if (r <= 0) return r;
        
        rem -= r;
        i += r;
    }

    return i;
}

// primary client thread
void *client_thread(void *arg) {
    server_client_t *cli = arg;

    for (;;) {
        struct pollfd fd = { .fd = cli->fd, .events = POLLIN, .revents = 0 };
        ssize_t r = poll(&fd, 1, -1);
        if (r < 0) {
            TRACE_ERROR("poll failed %s\n", strerror(errno));
            client_remove(cli);
            pthread_exit(NULL);
        }

        if (fd.revents & POLLHUP) {
            TRACE_INFO("Client %d disconnected\n", cli->fd);
            client_remove(cli);
            pthread_exit(NULL);
        }

        symphony_header_t hdr;
        r = client_recvFull(cli, &hdr, sizeof(symphony_header_t));
        
        if (r != sizeof(symphony_header_t)) {
            TRACE_ERROR("Client %d encountered error %s (%d)\n", cli->fd, strerror(errno), r);
            client_remove(cli);
            pthread_exit(NULL);
        }

        if (hdr.magic != SYMPHONY_MAGIC) {
            TRACE_ERROR("Client sent invalid packet (magic 0x%x)\n", hdr.magic);
            continue;
        }

        if (hdr.size >= 256) {
            TRACE_ERROR("Client sent too big of a packet (%d)\n", hdr.size);
            continue;
        }

        TRACE_DEBUG("Received packet %d from client %d\n", hdr.type, cli->fd);

        // yes this is stupid
        void *buffer = hdr.size ? malloc(hdr.size) : NULL;
        if (hdr.size != 0) {
            memset(buffer, 0xab, hdr.size);
            TRACE_DEBUG("receiving %d bytes\n", hdr.size);
            r = client_recvFull(cli, buffer, hdr.size);
            if (r != hdr.size) {
                TRACE_ERROR("Client %d encountered error %s\n", cli->fd, strerror(errno));
                free(buffer);
                client_remove(cli);
                pthread_exit(NULL);
            }
        }

        if (hdr.type == SYMPHONY_PACKET_GET_INFO) {
            symphony_info_t info = {
                .ver_major = SYMPHONY_VERSION_MAJOR,
                .ver_minor = SYMPHONY_VERSION_MINOR,
                .ver_lower = SYMPHONY_VERSION_LOWER,
                .num_devices = device_last_id
            };

            client_sendResponse(cli, &hdr, 0);
            client_sendResponseData(cli, &info, sizeof(symphony_info_t));
        } else if (hdr.type == SYMPHONY_PACKET_GET_DEVICE) {
            symphony_packet_get_device_t *pkt = buffer;
            server_device_t *dev = device_find(pkt->device_id);

            if (dev == NULL) {
                client_sendResponse(cli, &hdr, ENOENT);
            } else {
                symphony_device_info_t info = {
                    .device_streams = dev->streams.length
                };

                client_sendResponse(cli, &hdr, 0);
                client_sendResponseData(cli, &info, sizeof(symphony_device_info_t));
            }
        } else if (hdr.type == SYMPHONY_PACKET_GET_STREAM) {
            symphony_packet_get_stream_t *pkt = buffer;
            server_stream_t *stream = stream_find(pkt->stream_id);
            if (stream == NULL) {
                client_sendResponse(cli, &hdr, ENOENT);
            } else {
                symphony_stream_info_t info = {
                    .config = stream->config,
                };

                client_sendResponse(cli, &hdr, 0);
                client_sendResponseData(cli, &info, sizeof(symphony_stream_info_t));
            }
        } else if (hdr.type == SYMPHONY_PACKET_ADD_BUFFER) {
            symphony_packet_add_buffer_t *pkt = buffer;

            TRACE_DEBUG("SYMPHONY_PACKET_ADD_BUFFER\n");
            TRACE_DEBUG("Channels=%d SampleRate=%d Format=%d\n", pkt->channels, pkt->sample_rate, pkt->format);
            TRACE_DEBUG("StreamId=0x%x BufKey=%d\n", pkt->stream_id, pkt->bufkey);

            // try to find the stream to attach to
            server_stream_t *stream = stream_find(pkt->stream_id);
            if (stream == NULL) {
                TRACE_ERROR("no such stream for stream 0x%x\n", pkt->stream_id);
                client_sendResponse(cli, &hdr, ENOENT);
                continue;
            }

            // open the shared memory
            int shmfd = shared_open(pkt->bufkey);
            if (shmfd < 0) {
                TRACE_ERROR("shared_open failed with error %s\n", strerror(errno));
                client_sendResponse(cli, &hdr, errno);
                continue;
            }

            server_buffer_t *buf = malloc(sizeof(server_buffer_t));
            NODE_INIT(&buf->node, buf);
            buf->client = cli;
            buf->shmfd = shmfd;
            buf->buffer_size = pkt->buflen;
            buf->playing = false;
            buf->id = __atomic_fetch_add(&buffer_last_id, 1, __ATOMIC_RELAXED);
            buf->format = pkt->format;
            buf->channels = pkt->channels;
            buf->sample_rate = pkt->sample_rate;
            buf->stream = stream;

            // TODO: MULTI FORMAT SUPPORT WITH RESAMPLING!!
            if (buf->channels != 2 && buf->sample_rate != 48000 && buf->format != AUDIO_FORMAT_S16_LE) {
                TRACE_ERROR("Buffer is using unimplemented parameters\n");
                assert(0);
            }

            // Map the shm buffer
            buf->buffer = mmap(NULL, pkt->buflen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, shmfd, 0);
            if (buf->buffer == MAP_FAILED) {
                TRACE_ERROR("mmap failed with error %s\n", strerror(errno));
                client_sendResponse(cli, &hdr, errno);
                continue;
            }

            pthread_mutex_lock(&buffer_list_lock);
            buf->next = buffer_list;
            buffer_list = buf;
            pthread_mutex_unlock(&buffer_list_lock);

            pthread_mutex_lock(&stream->lock);
            list_append_node(&stream->buffers, &buf->node);
            pthread_mutex_unlock(&stream->lock);

            // Add to map
            hashmap_set(buffer_map, (void*)(uintptr_t)buf->id, buf);
            
            // Don't signal avail just yet! The buffer is not ready.

            // Send the client response
            client_sendResponse(cli, &hdr, 0);
            client_sendResponseData(cli, &buf->id, sizeof(int));
        } else if (hdr.type == SYMPHONY_PACKET_START) {
            symphony_packet_start_t *pkt = buffer;

            server_buffer_t *buf = hashmap_get(buffer_map, (void*)(uintptr_t)pkt->buffer_id);
            if (buf == NULL) {
                client_sendResponse(cli, &hdr, ENOENT);
                continue;
            }

            pthread_mutex_lock(&buf->stream->lock);
            buf->playing = true;
            pthread_cond_signal(&buf->stream->avail);
            pthread_mutex_unlock(&buf->stream->lock);

            client_sendResponse(cli, &hdr, 0);
        } else if (hdr.type == SYMPHONY_PACKET_STOP) {
            symphony_packet_stop_t *pkt = buffer;

            server_buffer_t *buf = hashmap_get(buffer_map, (void*)(uintptr_t)pkt->buffer_id);
            if (buf == NULL) {
                client_sendResponse(cli, &hdr, ENOENT);
                continue;
            }

            pthread_mutex_lock(&buf->stream->lock);
            buf->playing = false;
            pthread_mutex_unlock(&buf->stream->lock);

            client_sendResponse(cli, &hdr, 0);
        } else {
            TRACE_ERROR("Unknown or unimplemented request 0x%x\n", hdr.type);
            client_sendResponse(cli, &hdr, EINVAL);
        }

        if (buffer) free(buffer);
    }
}

// probe for devices in /device/
int probe_devices() {
    DIR *dev = opendir("/device/");
    if (!dev) {
        TRACE_ERROR("opendir: %s\n", strerror(errno));
        return 1;
    }

    if (chdir("/device/") < 0) {
        TRACE_ERROR("chdir: %s\n", strerror(errno));
        return 1;
    }

    // pass 1
    struct dirent *ent;
    while ((ent = readdir(dev)) != NULL) {
        if (strstr(ent->d_name, "audio") != ent->d_name) {
            continue;
        }

        if (device_last_id >= 255) {
            TRACE_ERROR("More than 255 audio devices detected. Stopping. Some audio devices will not be used.\n");
            break;
        }

        int device_idx = 0;
        int stream_idx = 0;
        int r = sscanf(ent->d_name, "audio%ds%d", &device_idx, &stream_idx);
        
        if (r == 2) {
            // We found a stream, ignore it for now so we can initialize all audio devices
        } else if (r == 1) {
            TRACE_INFO("Found new audio device %d\n", device_idx);

            server_device_t *dev = malloc(sizeof(server_device_t));
            dev->id = device_last_id++;
            NODE_INIT(&dev->node, dev);
            LIST_INIT(&dev->streams);

            list_append_node(&devices, &dev->node);
        } else if (r < 0) {
            TRACE_ERROR("sscanf failed: %s\n", strerror(errno));
        }
    }

    // Round two, find all the streams
    rewinddir(dev);

    while ((ent = readdir(dev)) != NULL) {
        if (strstr(ent->d_name, "audio") != ent->d_name) {
            continue;
        }

        int device_idx = 0;
        int stream_idx = 0;
        int r = sscanf(ent->d_name, "audio%ds%d", &device_idx, &stream_idx);

        if (r == 2) {
            TRACE_INFO("Found stream %d for device %d\n", stream_idx, device_idx);

            server_device_t *dev = device_find(device_idx);
            if (dev == NULL) {
                TRACE_ERROR("Where is the device for this stream?\n");
                continue;
            }

            server_stream_t *stream = malloc(sizeof(server_stream_t));
            NODE_INIT(&stream->node, stream);
            LIST_INIT(&stream->buffers);
            pthread_mutex_init(&stream->lock, NULL);
            pthread_cond_init(&stream->avail, NULL);
            
            stream->id = dev->streams.length;

            // Open the stream
            stream->fd = open(ent->d_name, O_WRONLY);
            if (stream->fd < 0) {
                TRACE_ERROR("%s: open failed with error %s\n", ent->d_name, strerror(errno));
                return 1;
            }

            // Initialize the stream
            if (stream_init(stream) != 0) {
                TRACE_ERROR("Initializing stream %d for device %d failed.\n", stream_idx, device_idx);
                close(stream->fd);
                free(stream);
                continue;
            }

            // Place it in the list
            list_append_node(&dev->streams, &stream->node);
            hashmap_set(stream_map, (void*)(uintptr_t)SYMPHONY_STREAM(device_idx, stream_idx), stream);
        } else if (r == 1) {
            // Device
        } else {
            TRACE_WARN("sscanf returned %d for device %s\n", r, ent->d_name);
        }
    }

    closedir(dev);
    return 0;
}

int main(int argc, char *argv[]) {
    TRACE_INFO("symphonyd v%d.%d.%d\n", SYMPHONY_VERSION_MAJOR, SYMPHONY_VERSION_MINOR, SYMPHONY_VERSION_LOWER);

    // Prepare socket
    server_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_socket < 0) {
        TRACE_ERROR("Failed to create server socket: %s\n", strerror(errno));
        return 1;
    } 

    // Bind to path
    struct sockaddr_un bind_addr = {
        .sun_family = AF_UNIX,
        .sun_path = SYMPHONY_SOCKET_PATH
    };

    if (bind(server_socket, (const struct sockaddr*)&bind_addr, sizeof(struct sockaddr_un)) < 0) {
        TRACE_ERROR("Failed to bind socket: %s\n", strerror(errno));
        return 1;
    }

    if (listen(server_socket, 10) < 0) {
        TRACE_ERROR("Failed to listen socket: %s\n", strerror(errno));
        return 1;
    }
 
    // Initialize structures
    buffer_map = hashmap_create_int("buffer map", 10);
    stream_map = hashmap_create_int("stream map", 10);

    // Probe
    if (probe_devices()) {
        TRACE_ERROR("Failed to probe devices\n");
        return 1;
    }

    // Enter main thread
    for (;;) {
        // TODO Not spin up a thread per client. Do Celestial-styled and spin up an accepter thread and IPC thread 
        int fd = accept(server_socket, NULL, NULL);
        if (fd < 0) {
            TRACE_ERROR("accept failed: %s\n", strerror(errno));
            return 1;
        }

        TRACE_DEBUG("Got new client %d\n", fd);

        server_client_t *cli = malloc(sizeof(server_client_t));
        cli->fd = fd;
        cli->next = NULL;
        cli->prev = NULL;
        cli->volume = 0.5f;
        
        pthread_mutex_lock(&client_list_lock);
        cli->next = client_list;
        client_list = cli;
        pthread_mutex_unlock(&client_list_lock);

        pthread_create(&cli->thread, NULL, client_thread, cli);
    }

    return 0;
}

