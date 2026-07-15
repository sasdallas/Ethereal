/**
 * @file hexahedron/subsystems/audio.c
 * @brief Audio subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/subsystems/audio.h>
#include <kernel/processor_data.h>
#include <kernel/fs/devfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/slab.h>
#include <kernel/fs/poll.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/pmm.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "AUDIO", __VA_ARGS__)

/* Devfs ops */
static int audiodev_ioctl(devfs_node_t *n, unsigned long request, void *argp);
devfs_ops_t audio_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = audiodev_ioctl,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll_events = NULL,
    .poll = NULL,
};

/* Stream ops */
static int audio_close(devfs_node_t *n);
static ssize_t audio_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer, int flags);
static int audio_ioctl(devfs_node_t *n, unsigned long request, void *argp);
devfs_ops_t audio_stream_ops = {  
    .open = NULL,
    .close = audio_close,
    .read = NULL,
    .write_ext = audio_write,
    .ioctl = audio_ioctl,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll_events = NULL,
    .poll = NULL,
};

/* List of audio devices */
audio_device_t *audio_devices = NULL;
int audio_device_idx = 0;
spinlock_t audio_device_lock = SPINLOCK_INITIALIZER;

/* Caches */
slab_cache_t *audio_device_cache = NULL;
slab_cache_t *audio_control_cache = NULL;
slab_cache_t *audio_stream_cache = NULL;

/* Set state */
#define STREAM_SET_STATE(s, _state) ({ int r = (s)->ops->set_state(s, (_state)); if (r == 0) { (s)->state = (_state); } r; })


/**
 * @brief Create a new audio device
 * @param priv The private pointer to use for this device
 * @returns A new audio device, already placed in the list
 */
audio_device_t *audio_createDevice(void *priv) {
    audio_device_t *dev = slab_allocate(audio_device_cache);
    memset(dev, 0, sizeof(audio_device_t));
    MUTEX_INIT(&dev->lock);
    dev->priv = priv;

    spinlock_acquire(&audio_device_lock);
    dev->index = audio_device_idx++;
    dev->next = audio_devices;
    audio_devices = dev;
    spinlock_release(&audio_device_lock);
    
    char name[128];
    snprintf(name, 128, "audio%d", dev->index);
    dev->node = devfs_register(devfs_root, name, VFS_CHARDEVICE, &audio_dev_ops, DEVFS_MAJOR_AUDIO, dev->index, (void*)dev);
    
    return dev;
}

/**
 * @brief Create a new audio control
 * @param device The device to add the control to
 * @param name The name of the control to register
 * @param type The type of control to register
 * @param purpose The purpose of the control
 * @param priv The private pointer
 * @returns Control object
 */
audio_control_t *audio_createControl(audio_device_t *device, char *name, audio_control_type_t type, audio_control_purpose_t purpose, void *priv) {
    audio_control_t *ctrl = slab_allocate(audio_control_cache);
    memset(ctrl, 0, sizeof(audio_control_t));
    ctrl->parent = device;    
    ctrl->type = type;
    ctrl->name = name;
    ctrl->purpose = purpose;
    ctrl->ops.get_value = NULL;
    ctrl->ops.set_value = NULL;
    ctrl->priv = priv;

    mutex_acquire(&device->lock);
    ctrl->next = device->controls;
    ctrl->id = device->ncontrols++;
    device->controls = ctrl;
    mutex_release(&device->lock);
    
    return ctrl;
}

/**
 * @brief Create a new audio stream
 * @param device The device to add the stream to
 * @param ops The stream operations
 * @param priv The private pointer
 * @returns Stream object
 */
audio_stream_t *audio_createStream(audio_device_t *device, audio_stream_ops_t *ops, void *priv) {
    audio_stream_t *stream = slab_allocate(audio_stream_cache);
    memset(stream, 0, sizeof(audio_stream_t));
    SPINLOCK_INIT(&stream->buffer.lock);
    WAIT_QUEUE_INIT(&stream->buffer.waiters);

    stream->parent = device;
    stream->ops = ops;
    stream->priv = priv;
    stream->state = AUDIO_STREAM_STATE_STOPPED;

    mutex_acquire(&device->lock);
    stream->next = device->streams;
    device->streams = stream;
    int id = device->nstreams++;
    mutex_release(&device->lock);

    // TODO maybe use different major?
    char name[128];
    snprintf(name, 128, "audio%ds%d", device->index, id);
    stream->node = devfs_register(devfs_root, name, VFS_CHARDEVICE, &audio_stream_ops, device->index, id, stream);
    
    return stream;
}

/**
 * @brief Initialize buffer
 * @param buffer The buffer to initialize
 * @param sgl The SGL allocated
 */
void audio_initBuffer(audio_buffer_t *buffer, audio_buffer_sgl_t *sgl) {
    // By init buffer we just need to map vaddr
    void *vaddr = vmm_map(NULL, PAGE_SIZE * sgl->size_pages, VM_FLAG_DEFAULT, 0);
    assert(vaddr);

    size_t pgs_per_ent = sgl->size_pages / sgl->num_entries;
    for (unsigned i = 0; i < sgl->num_entries; i++) {
        uintptr_t addr = (uintptr_t)vaddr + ((i * pgs_per_ent) * PAGE_SIZE);
        for (unsigned j = 0; j < pgs_per_ent; j++) {
            arch_mmu_map(NULL, addr + (j * PAGE_SIZE), sgl->entries[i].paddr + (j * PAGE_SIZE), MMU_FLAG_PRESENT | MMU_FLAG_WRITE);
        }
    }

    buffer->sgl = sgl;
    buffer->vaddr = vaddr;
}

/**
 * @brief Audio device ioctl
 */
static int audiodev_ioctl(devfs_node_t *n, unsigned long request, void *argp) {
    audio_device_t *dev = n->priv;
    int retval = 0;

    // Lock the device and process the request
    mutex_acquire(&dev->lock);

    if (request == IO_AUDIO_DEVICE_GET_INFO) {
        audio_device_info_t *info = argp;
        info->ncontrols = dev->ncontrols;
        info->nstreams = dev->nstreams;
    } else if (request == IO_AUDIO_DEVICE_GET_CONTROL) {
        retval = -ENOTSUP;
    } else if (request == IO_AUDIO_DEVICE_GET_VALUE) {
        retval = -ENOTSUP;
    } else if (request == IO_AUDIO_DEVICE_SET_VALUE) {
        retval = -ENOTSUP;
    } else {
        LOG(ERR, "Invalid or unimplemented audio device request 0x%x\n", request);
        retval = -EINVAL;
    }

    mutex_release(&dev->lock);
    return retval;
}

/**
 * @brief Audio stream close
 */
static int audio_close(devfs_node_t *n) {
    audio_stream_t *stream = n->priv;
    LOG(DEBUG, "audio_close\n");
    STREAM_SET_STATE(stream, AUDIO_STREAM_STATE_DRAINING);
    WAIT_QUEUE_CONDITION(&stream->buffer.drainers, stream->buffer.bytes_available == 0);
    return 0;     
}

/**
 * @brief Audio stream write
 */
static ssize_t audio_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer, int flags) {
    audio_stream_t *stream = n->priv;
    audio_buffer_t *buf = &stream->buffer;

    size_t bytes_rem = size;
    size_t bytes_placed = 0;
    while (bytes_rem) {
        if ((flags & O_NONBLOCK) == 0) {
            int r = WAIT_QUEUE_CONDITION(&buf->waiters, buf->bytes_available + buf->frame_size < buf->buffer_size);
            if (r != 0) {
                return r;
            }
        }
            
        spinlock_acquire(&buf->lock);

        size_t avl = buf->buffer_size - buf->bytes_available;
        if (avl < buf->frame_size) {
            // race condition was lost
            spinlock_release(&buf->lock);

            if ((flags & O_NONBLOCK)) {
                return -EWOULDBLOCK;
            }
            
            continue;
        }
        
        if (avl > bytes_rem) avl = bytes_rem;

        // otherwise, calculate how many bytes we can fit
        size_t start_off = buf->app_ptr % buf->buffer_size;
        size_t end_off = (buf->app_ptr + avl) % buf->buffer_size;

        if (start_off < end_off) {
            memcpy(buf->vaddr + start_off, buffer + bytes_placed, avl);
        } else {
            size_t fit = buf->buffer_size - start_off;
            memcpy(buf->vaddr + start_off, buffer + bytes_placed, fit);
            memcpy(buf->vaddr, buffer + bytes_placed + fit, avl - fit);
        }


        bytes_rem -= avl;
        bytes_placed += avl;
        buf->bytes_available += avl;
        buf->app_ptr += avl;

        if (stream->state != AUDIO_STREAM_STATE_PLAYING && buf->bytes_available >= buf->start_thres) {
            LOG(DEBUG, "Starting stream, passed threshold.\n");
            STREAM_SET_STATE(stream, AUDIO_STREAM_STATE_PLAYING);
        }

        spinlock_release(&buf->lock);
    }

    return (ssize_t)bytes_placed;
}

/**
 * @brief Audio stream ioctl
 */
static int audio_ioctl(devfs_node_t *n, unsigned long request, void *argp) {
    audio_stream_t *stream = n->priv;

    if (request == IO_AUDIO_SET_CONFIG) {
        audio_stream_config_t *config = argp;
        LOG(DEBUG, "IO_AUDIO_SET_CONFIG receieved\n");
        LOG(DEBUG, "Configuration: period_size=0x%x periods=0x%x channels=%d sample_rate=%d\n", config->period_size, config->periods, config->channels, config->sample_rate);
    
        // Stop the stream if needed
        spinlock_acquire(&stream->buffer.lock);
        if (stream->state != AUDIO_STREAM_STATE_STOPPED) {
            int r = STREAM_SET_STATE(stream, AUDIO_STREAM_STATE_STOPPED);
            if (r != 0) {
                spinlock_release(&stream->buffer.lock);
                return r;
            }
        }

        // Configure the stream
        int r = stream->ops->configure(stream, config);
        if (r != 0) {
            spinlock_release(&stream->buffer.lock);
            return r;
        }

        // Setup the buffer
        stream->buffer.app_ptr = 0;
        stream->buffer.bytes_available = 0;
        stream->buffer.periods = config->periods;
        stream->buffer.period_size = config->period_size;
        stream->buffer.frame_size = audio_getFrameSize(config);
        stream->buffer.start_thres = config->start_thres;
        stream->buffer.buffer_size = config->periods * (config->period_size * stream->buffer.frame_size);
        if (stream->buffer.buffer_size > stream->buffer.sgl->size_pages * PAGE_SIZE) {
            LOG(ERR, "The buffer size is too low to accomodate %d periods.\n", config->periods);
            spinlock_release(&stream->buffer.lock);
            return -EINVAL;
        }

        // We are now configured
        assert(STREAM_SET_STATE(stream, AUDIO_STREAM_STATE_CONFIGURED) == 0);
        spinlock_release(&stream->buffer.lock);

        return 0;
    } else if (request == IO_AUDIO_REQUEST) {
        int audreq  = *(int*)argp;

        int r = 0;
        audio_stream_state_t state_to_set = 0;
        if (audreq == AUDIO_STREAM_PLAY) {
            if (stream->state != AUDIO_STREAM_STATE_CONFIGURED || stream->state == AUDIO_STREAM_STATE_PAUSED) {
                LOG(INFO, "Cannot start stream while state is %d\n", stream->state);
                r = -EINVAL;
                goto _cleanup;
            }

            state_to_set = AUDIO_STREAM_STATE_PLAYING;
        } else if (audreq == AUDIO_STREAM_STOP) {
            state_to_set = AUDIO_STREAM_STATE_STOPPED;
        } else if (audreq == AUDIO_STREAM_PAUSE) {
            state_to_set = AUDIO_STREAM_STATE_PAUSED;
        } else if (audreq == AUDIO_STREAM_DRAIN) {
            state_to_set = AUDIO_STREAM_STATE_DRAINING;
        } else {
            LOG(ERR, "Unknown audio stream command %d\n", audreq);
            r = -EINVAL;
            goto _cleanup;
        }

        // Set the state
        if (stream->state == state_to_set) return 0;
        r = STREAM_SET_STATE(stream, state_to_set);
        if (r != 0) goto _cleanup;

        if (audreq == AUDIO_STREAM_DRAIN) {
            // We have a bit more work to do
            spinlock_release(&stream->buffer.lock);
            WAIT_QUEUE_CONDITION(&stream->buffer.drainers, stream->buffer.bytes_available == 0);
            return 0;
        }

    _cleanup:
        spinlock_release(&stream->buffer.lock);
        return r;
    }

    return -ENODEV;
}

/**
 * @brief Audio advance stream to next period
 * @param stream The stream to advance
 */
void audio_streamNextPeriod(audio_stream_t *stream) {
    audio_buffer_t *buf = &stream->buffer;
    size_t bytes_finished = buf->period_size * buf->frame_size;
    
    spinlock_acquire(&buf->lock);
    waitqueue_wakeup(&buf->waiters, 1);

    if (bytes_finished > buf->bytes_available) {
        bytes_finished = buf->bytes_available;
    }
    
    buf->bytes_available -= bytes_finished;

    if (!buf->bytes_available) {
        buf->app_ptr = 0;
        if (stream->state == AUDIO_STREAM_STATE_DRAINING) {
            waitqueue_wakeup(&buf->drainers, 1);
        } else {
            LOG(ERR, "Out of content! No bytes available on stream.\n");
        }

        STREAM_SET_STATE(stream, AUDIO_STREAM_STATE_STOPPED);
    }

    spinlock_release(&buf->lock);
}

/**
 * @brief Initialize audio subsystem
 */
static int audio_init() {
    audio_device_cache = slab_createCache("audio device cache", SLAB_CACHE_DEFAULT, sizeof(audio_device_t), 0, NULL, NULL);
    audio_control_cache = slab_createCache("audio control cache", SLAB_CACHE_DEFAULT, sizeof(audio_control_t), 0, NULL, NULL);
    audio_stream_cache = slab_createCache("audio stream cache", SLAB_CACHE_DEFAULT, sizeof(audio_stream_t), 0, NULL, NULL);
    return 0;
}

FS_INIT_ROUTINE(audio, INIT_FLAG_DEFAULT, audio_init);
