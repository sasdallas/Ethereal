/**
 * @file drivers/audio/hda/hda.c
 * @brief Intel HD Audio
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "hda.h"
#include <kernel/subsystems/audio.h>
#include <kernel/subsystems/irq.h>
#include <kernel/loader/driver.h>
#include <kernel/drivers/pci.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <string.h>

/* Audio stream ops */
static uint64_t hda_get_position(audio_stream_t *s);
static int hda_set_state(audio_stream_t *s, audio_stream_state_t state);
static int hda_configure(audio_stream_t *s, audio_stream_config_t *config);
audio_stream_ops_t stream_ops = {
    .set_state = hda_set_state,
    .get_position = hda_get_position,
    .configure = hda_configure
};

/* Some helpers */
static int hda_volume_get_value(audio_control_t *c, audio_control_value_t *val);
static int hda_volume_set_value(audio_control_t *c, audio_control_value_t *val);
char *default_device_names[] = {
    "Line Out",
    "Speakers",
    "HP Out",
    "CD",
    "SPDIF Out",
    "Digital Out"
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:HDA", __VA_ARGS__)

/**
 * @brief HDA irq
 */
static int hda_irq(irq_t *irq, void *context) {
    hda_t *hda = context;

    if (hda->regs->rirbsts) {
        hda->regs->rirbsts = hda->regs->rirbsts;
        waitqueue_wakeup(&hda->cmd_queue, 1);    
    }

    if (hda->regs->intsts) {
        uint32_t sts = hda->regs->intsts;

        for (int i = 0; i < 30; i++) {
            if ((sts & (1 << i)) == 0) {
                continue;
            }

            if (hda->streams[i] == NULL) {
                continue;
            }

            uint32_t sdsts = hda->streams[i]->regs->sdsts;
            hda->streams[i]->regs->sdsts = sdsts & ((1 << 2) | (1 << 4) | (1 << 3));
            if (sdsts & (1 << 2)) {
                audio_streamNextPeriod(hda->streams[i]->s);
            }
        }
    }

    return IRQ_HANDLED;
}

/**
 * @brief HDA reset
 */
static int hda_reset(hda_t *hda) {
    LOG(DEBUG, "Performing controller reset.\n");
    if ((hda->regs->gctl & HDA_GCTL_CRST)) {
        hda->regs->gctl &= ~(HDA_GCTL_CRST);
        while ((hda->regs->gctl & HDA_GCTL_CRST)) {
            arch_pause();
        }
    } 

    // Sleep a bit
    clock_sleep(100);

    // Controller is in reset state
    hda->regs->gctl |= HDA_GCTL_CRST;


    int i = 5000;
    while ((hda->regs->gctl & HDA_GCTL_CRST) == 0) {
        if (!i) {
            LOG(ERR, "Timed out when waiting for CRST to set.\n");
            return 1;
        }

        clock_sleep(100);
        i -= 100;
    }


    clock_sleep(100);

    return 0;
}

/**
 * @brief HDA init buffers
 */
static int hda_initBuffers(hda_t *hda) {
    uintptr_t buffer_region = dma_map(PAGE_SIZE);
    uintptr_t phys = arch_mmu_physical(NULL, buffer_region);

    // Disable both DMA regions
    hda->regs->corbctl &= ~(HDA_CORBCTL_CORBRUN);
    hda->regs->rirbctl &= ~(HDA_RIRBCTL_RIRBRUN);

    // Prepare the CORB
    // TODO check CORBSZCAP
    hda->regs->corblbase = phys & 0xFFFFFFFF;
    hda->regs->corbubase = (phys >> 32) & 0xFFFFFFFF;
    hda->regs->corbsize = (hda->regs->corbsize & 0xFC) | 0x2;
    hda->regs->corbrp |= 0x8000;
    hda->regs->corbwp &= ~0xFF;

    // Wait for the CORBRPRST to set
    for (int i = 0; i < 200; i++) {
        if (hda->regs->corbrp & 0x8000) break;
        clock_sleep(10);
    }

    if ((hda->regs->corbrp & 0x8000) == 0) {
        // TODO cleanup
        LOG(ERR, "CORBPRST was never set by hardware\n");
        return 1;
    }

    // Good now we can clear it
    hda->regs->corbrp &= ~0x8000;
    for (int i = 0; i < 200; i++) {
        if ((hda->regs->corbrp & 0x8000) == 0) break;
        clock_sleep(10);
    }
    if (hda->regs->corbrp & 0x8000) {
        LOG(ERR, "CORBRPRST never cleared by hardware\n");
        return 1;
    }

    LOG(DEBUG, "CORB initialized: %p\n", phys);

    // Prepare the RIRB
    hda->regs->rirbctl &= ~(HDA_RIRBCTL_RIRBRUN);
    hda->regs->rirblbase = (phys + 1024) & 0xFFFFFFFF;
    hda->regs->rirbubase = ((phys + 1024) >> 32) & 0xFFFFFFFF;
    hda->regs->rirbsize = (hda->regs->rirbsize & 0xFC) | 0x2;
    hda->regs->rirbwp |= 0x8000;
    hda->regs->rintcnt = (hda->regs->rintcnt & 0xFF00) | 1;


    LOG(DEBUG, "RIRB initialized: %p\n", phys + 1024);

    // Enable both DMA engines
    hda->regs->corbctl |= (HDA_CORBCTL_CORBRUN | 1); // enable the error interrupts too
    hda->regs->rirbctl |= (HDA_RIRBCTL_RIRBRUN | 1);

    // Setup structures
    hda->corb.vaddr = (uint32_t*)buffer_region;
    hda->corb.index = 1;
    hda->corb.sz = 256;

    hda->rirb.vaddr = (uint64_t*)(buffer_region + 1024);
    hda->rirb.index = 1;
    hda->rirb.sz = 256;

    return 0;
}

/**
 * @brief HDA send command internal
 */
static int hda_sendCommandInternal(hda_t *hda, uint32_t corb_ent, uint32_t *out) {
    // TODO multiple improvements to this
    mutex_acquire(&hda->cmd_lock);   
    
    // attach to queue
    wait_queue_node_t n;
    waitqueue_add(&hda->cmd_queue, &n);

    // claim a slot
    hda->corb.vaddr[hda->corb.index] = corb_ent;
    hda->regs->corbwp = (hda->regs->corbwp & 0xFF00) | hda->corb.index;
    hda->corb.index = (hda->corb.index + 1) % hda->corb.sz;

    mutex_release(&hda->cmd_lock);

    // TODO allow timeouts on this
    int w = waitqueue_wait(&hda->cmd_queue, &n, -1);
    waitqueue_remove(&hda->cmd_queue, &n);

    if (w != 0) {
        return w;
    }

    mutex_acquire(&hda->cmd_lock);
    uint64_t rirb = hda->rirb.vaddr[hda->rirb.index];
    hda->rirb.index = (hda->rirb.index + 1) % hda->rirb.sz;
    mutex_release(&hda->cmd_lock);

    *out = (uint32_t)((rirb) & 0xFFFFFFFF);
    return 0;
}

/**
 * @brief HDA send command
 */
static int hda_sendCommand(hda_t *hda, uint8_t codec, uint8_t node_id, uint16_t command, uint8_t data, uint32_t *out) {
    uint32_t corb_ent = HDA_CORB(codec, node_id, command, data);
    return hda_sendCommandInternal(hda, corb_ent, out);
}

/**
 * @brief HDA send big command
 */
static int hda_sendCommandLong(hda_t *hda, uint8_t codec, uint8_t nid, uint8_t verb4, uint16_t payload16, uint32_t *out) {
    uint32_t corb_ent = (uint32_t)((codec << 28) | (nid << 20) | ((verb4 & 0xF) << 16) | payload16);
    return hda_sendCommandInternal(hda, corb_ent, out);
}

/**
 * @brief Get parameter
 */
static inline uint32_t hda_getParameter(hda_t *hda, uint8_t codec, uint8_t nid, uint8_t param) {
    uint32_t out;

    // TODO: error handling
    assert(hda_sendCommand(hda, codec, nid, HDA_GET_PARAMETERS, param, &out) == 0);
    return out;
}   

/**
 * @brief Find widget by NID
 */
static hda_widget_t *hda_findByNID(hda_fg_t *fg, uint8_t nid) {
    hda_widget_t *w = fg->widgets;
    while (w) {
        if (w->nid == nid) {
            return w;
        }

        w = w->next;
    }

    return NULL;
}

/**
 * @brief Create stream controls
 */
static void hda_createStreamControls(hda_t *hda, hda_stream_t *stream, hda_fg_t *fg, uint8_t *path, int path_len) {
    // TODO this is pretty stupid
    char name[64];
    
    // Get configuration defaults
    uint32_t cfg;
    assert(hda_sendCommand(hda, stream->codec, stream->pin_nid, HDA_GET_CONFIG_DEFAULT, 0, &cfg) == 0);

    char *dev_name = NULL;
    if (HDA_CONFIG_DEF_DEVICE(cfg) > 5) {
        LOG(WARN, "Unknown configuration device 0x%x\n", HDA_CONFIG_DEF_DEVICE(cfg));
        dev_name = "Unknown";
    } else {
        dev_name = default_device_names[HDA_CONFIG_DEF_DEVICE(cfg)];
    }

    for (int i = path_len-1; i >= 0; i--) {
        hda_widget_t *w = hda_findByNID(fg, path[i]);
        if (!w) continue;

        if (w->caps & (1 << 2)) {
            // Output amp
            hda_control_t *ct = kmalloc(sizeof(hda_control_t));
            ct->hda = hda;
            ct->stream = stream;
            ct->w = w;
            
            // TODO can detect things like headphones out and make this name betetr
            snprintf(name, 64, "%s Playback Volume", dev_name);

            // Create the audio control and prepare it
            audio_control_t *ctrl = audio_createControl(hda->dev, name, AUDIO_CONTROL_INTEGER, AUDIO_PURPOSE_VOLUME, ct);
            ctrl->ops.get_value = hda_volume_get_value;
            ctrl->ops.set_value = hda_volume_set_value;

            // Fill the integer scale
            uint32_t caps = hda_getParameter(hda, stream->codec, w->nid, HDA_PARAM_OUTPUT_AMP_CAPS);           
            ctrl->info.integer.min = 0;
            ctrl->info.integer.max = (caps >> 8) & 0x7f;
            ctrl->info.integer.step = 1; // TODO: maybe

            break;
        }
    }

    // TODO: Other controls like mute/unmute
} 

/**
 * @brief Create an output stream
 */
static int hda_createOutputStream(hda_t *hda, uint8_t codec, hda_fg_t *fg, uint8_t *path, int path_len) {
    // Allocate an output stream descriptor
    int stream_id = -1;
    for (unsigned i = hda->input_streams; i < hda->input_streams + hda->output_streams; i++) {
        if (hda->streams[i] == NULL) {
            stream_id = i;
            break;
        }
    }

    if (stream_id == -1) {
        LOG(WARN, "No streams available to handle DAC!\n");
        return 0;
    } 

    LOG(INFO, "Allocated stream ID %d\n", stream_id);

    hda_stream_t *stream = kzalloc(sizeof(hda_stream_t));
    hda->streams[stream_id] = stream;
    stream->parent = hda;
    stream->index = (stream_id - hda->input_streams) + 1;
    stream->regs = (volatile hda_stream_regs_t*)((uintptr_t)hda->regs + 0x80 + (stream_id * 0x20));
    stream->codec = codec;
    stream->dac_nid = path[path_len-1];
    stream->pin_nid = path[0];

    // Reset the stream descriptor
    stream->regs->sdctl[0] = 0;
    stream->regs->sdctl[0] |= (1 << 0);
    while ((stream->regs->sdctl[0] & (1 << 0)) == 0) {
        LOG(DEBUG, "Waiting for SRST to set.\n");
        arch_pause();
    }

    stream->regs->sdctl[0] &= ~(1 << 0);
    while ((stream->regs->sdctl[0] & (1 << 0))) {
        LOG(DEBUG, "Waiting for SRST to clear.\n");
        arch_pause();
    }

    LOG(INFO, "Stream reset ok\n");

    // TODO THIS IS ALL MAGIC VALUES AND HAS NO ERROR CHECKING!!!!!!

    // Set the format temporarily
    uint32_t fmt = (1 << 4) | (1 << 0);
    stream->regs->sdfmt = fmt;

    // Begin creation of the BDL
    size_t nent = (PAGE_SIZE/sizeof(hda_bdl_entry_t));
    audio_buffer_sgl_t *sgl = kmalloc(sizeof(audio_buffer_sgl_t) + (sizeof(audio_buffer_sgl_ent_t) * nent));
    hda_bdl_entry_t *bdl_page = (hda_bdl_entry_t*)dma_map(PAGE_SIZE); 
    stream->bdl = bdl_page;

    sgl->num_entries = nent;
    sgl->size_pages = nent;

    size_t buffer_sz = nent * PAGE_SIZE; 
    for (unsigned i = 0; i < PAGE_SIZE / sizeof(hda_bdl_entry_t); i++) {
        bdl_page[i].address = pmm_allocatePage(ZONE_DEFAULT);
        bdl_page[i].length = PAGE_SIZE;
        bdl_page[i].ioc = 1;

        sgl->entries[i].paddr = bdl_page[i].address;
        sgl->entries[i].length = PAGE_SIZE;
    }

    // Prepare the BDL page memory
    uintptr_t bdl_page_phys = arch_mmu_physical(NULL, (uintptr_t)bdl_page);
    stream->regs->sdbdpl = (bdl_page_phys & 0xFFFFFFFF);
    stream->regs->sdbdpu = ((bdl_page_phys >> 32) & 0xFFFFFFFF);
    stream->regs->sdlcbl = buffer_sz;
    stream->regs->sdlvi = 1;

    // Enable some stuff
    stream->regs->sdctl[0] = (1 << 2) | (1 << 4);
    stream->regs->sdctl[2] = (stream->index << 4);

    // Bind DAC to streamtag
    uint32_t resp;
    hda_sendCommand(hda, codec, path[path_len-1], 0x706, (stream->index << 4) | 0, &resp);
    hda_sendCommand(hda, codec, path[path_len-1], 0x200, fmt, &resp);

    // Construct path
    if (path_len > 2) {
        for (int i = 0; i < path_len-1; i++) {
            hda_widget_t *c = hda_findByNID(fg, path[i]);
            if (c && (c->type == HDA_WIDGET_TYPE_SELECTOR || c->type == HDA_WIDGET_TYPE_MIXER)) {
                uint8_t next = path[i+1];
                for (unsigned conn = 0; conn < c->nconn; conn++) {
                    if (c->conntbl[conn] == next) {
                        hda_sendCommand(hda, codec, c->nid, 0x701, conn, &resp);
                        break;
                    }
                }
            }
        }
    }

    // Unmute everything and setup the amps
    // TODO: Less magics
    for (int i = 0; i < path_len; i++) {
        uint8_t nid = path[i];
        hda_widget_t *w = hda_findByNID(fg, nid);
        if (!w) continue;

        if (w->caps & (1 << 2)) {
            uint16_t unmute_out = HDA_AMP_SET_OUTPUT | HDA_AMP_SET_LEFT | HDA_AMP_SET_RIGHT | 0x7F; 
            hda_sendCommandLong(hda, codec, nid, 0x3, unmute_out, &resp);
        }

        if ((w->caps & (1 << 1)) && (i < path_len - 1)) {
            uint8_t next_nid = path[i + 1];
            int conn_index = -1;

            for (unsigned c = 0; c < w->nconn; c++) {
                if (w->conntbl[c] == next_nid) {
                    conn_index = c;
                    break;
                }
            }

            if (conn_index != -1) {
                uint16_t unmute_in = HDA_AMP_SET_INPUT | HDA_AMP_SET_LEFT | HDA_AMP_SET_RIGHT | (conn_index << 8) | 0x7F;
                hda_sendCommandLong(hda, codec, nid, 0x3, unmute_in, &resp);
            }
        }
    }

    hda_sendCommand(hda, codec, path[0], 0x707, (1 << 6), &resp);

    // EAPD
    hda_sendCommand(hda, codec, path[0], 0x70C, 0x2, &resp);

    // Power on DAC and pin
    hda_sendCommand(hda, codec, path[0], 0x705, 0x0, &resp);
    hda_sendCommand(hda, codec, path[path_len-1], 0x705, 0x0, &resp);

    // Create output stream
    stream->s = audio_createStream(hda->dev, &stream_ops, stream);

    // Create the SGL
    audio_initBuffer(&stream->s->buffer, sgl);

    // Create all controls
    hda_createStreamControls(hda, stream, fg, path, path_len);
    return 0;
}


/**
 * @brief HDA get format
 */
static uint16_t hda_calculateFormat(audio_stream_config_t *cfg) {
uint16_t fmt = 0;

    fmt |= (cfg->channels - 1) & 0x0F;

    switch (cfg->format) {
        case AUDIO_FORMAT_S16_LE:
            fmt |= (1 << 4);
            break;
        case AUDIO_FORMAT_S24_LE:
            fmt |= (3 << 4);
            break;
        case AUDIO_FORMAT_S32_LE:
            fmt |= (4 << 4);
            break;
        case AUDIO_FORMAT_F32_LE: // unsupported!
        default:
            LOG(ERR, "Unknown audio format 0x%x\n", cfg->format);
            fmt |= (1 << 4);
            break;
    }

    switch (cfg->sample_rate) {
        case 48000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_1X; break;
        case 96000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_2X | HDA_RATE_DIV_1X; break;
        case 144000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_3X | HDA_RATE_DIV_1X; break;
        case 192000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_4X | HDA_RATE_DIV_1X; break;
        case 24000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_2X; break;
        case 16000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_3X; break;
        case 12000: fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_4X; break;
        case 8000 : fmt |= HDA_RATE_BASE_48KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_6X; break;
        case 44100: fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_1X; break;
        case 88200: fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_2X | HDA_RATE_DIV_1X; break;
        case 176400: fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_4X | HDA_RATE_DIV_1X; break;
        case 22050: fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_2X; break;
        case 11025: fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_4X; break;

        default:
            LOG(ERR, "Unsupported sample rate %d\n", cfg->sample_rate);
            fmt |= HDA_RATE_BASE_441KHZ | HDA_RATE_MULT_1X | HDA_RATE_DIV_1X;
            break;
    }

    return fmt;
}

/**
 * @brief hda get position
 */
static uint64_t hda_get_position(audio_stream_t *s) {
    hda_stream_t *stream = s->priv;
    audio_buffer_t *buf = &s->buffer;
    
    if (!buf->frame_size) return 0;
    uint64_t div = stream->regs->sdlpib;
    if (div) div = div / buf->frame_size;
    return div;
}

/**
 * @brief hda set state
 */
static int hda_set_state(audio_stream_t *s, audio_stream_state_t state) {
    hda_stream_t *stream = s->priv;
    switch (state) {
        case AUDIO_STREAM_STATE_STOPPED:
            stream->regs->sdctl[0] &= ~(1 << 1);
            break;

        case AUDIO_STREAM_STATE_PLAYING:
            stream->regs->sdctl[0] |= (1 << 1);
            break;

        case AUDIO_STREAM_STATE_PAUSED:
            assert(0 && "unimpl");

        case AUDIO_STREAM_STATE_CONFIGURED:
        case AUDIO_STREAM_STATE_DRAINING:
            break;

        default:
            assert(0);
    }

    return 0;
}

/**
 * @brief hda configure
 */
static int hda_configure(audio_stream_t *s, audio_stream_config_t *config) {
    hda_stream_t *stream = s->priv;
    hda_t *hda = stream->parent;

    if (config->channels > 16) return -EINVAL;
    if (config->format == AUDIO_FORMAT_F32_LE) return -ENOTSUP;

    // Configure the format
    uint16_t fmt = hda_calculateFormat(config);
    stream->regs->sdfmt = fmt;

    // Request the DAC to also do that
    uint32_t resp;
    int err = hda_sendCommandLong(hda, stream->codec, stream->dac_nid, 0x2, fmt, &resp);
    if (err != 0) {
        return err;
    }

    // Reconfigure the BDLs to match the length of a period
    int period_bytes = config->period_size * audio_getFrameSize(config);

    // Only configure the active entries requested by the stream
    for (unsigned i = 0; i < config->periods; i++) {
        stream->bdl[i].address = (uint64_t)arch_mmu_physical(NULL, (uintptr_t)s->buffer.vaddr + (i * period_bytes));
        stream->bdl[i].length = period_bytes;
        stream->bdl[i].ioc = 1;
    }

    // Setup more BDL data
    stream->regs->sdlvi = (uint16_t)(config->periods - 1);
    stream->regs->sdlcbl = (uint32_t)(period_bytes * config->periods);
    
    return 0;
}

/**
 * @brief Get value
 */
static int hda_volume_get_value(audio_control_t *c, audio_control_value_t *val) {
    hda_control_t *control = c->priv;
    
    // TODO: figure out how we should do L/R channels?
    // TODO: maybe expose gain mapping to userspace?
    uint32_t resp;
    int r = hda_sendCommandLong(control->hda, control->stream->codec, control->w->nid, 0xB, (1 << 15), &resp);
    if (r != 0) {
        return r;
    }

    val->value.integer.value = resp & 0x7f;
    return 0;
}

/**
 * @brief Set value
 */
static int hda_volume_set_value(audio_control_t *c, audio_control_value_t *val) {
    hda_control_t *control = c->priv;
    uint32_t resp;
    uint16_t verb = HDA_AMP_SET_OUTPUT | HDA_AMP_SET_LEFT | HDA_AMP_SET_RIGHT | (val->value.integer.value & 0x7f);
    return hda_sendCommandLong(control->hda, control->stream->codec, control->w->nid, 0x3, verb, &resp);
}

/**
 * @brief Initialize widget
 */
static int hda_initWidget(hda_t *hda, uint8_t codec, hda_fg_t *fg, uint8_t nid) {
    hda_widget_t *w = kzalloc(sizeof(hda_widget_t));
    w->next = fg->widgets;
    fg->widgets = w;

    w->nid = nid;
    w->caps = hda_getParameter(hda, codec, nid, HDA_PARAM_AUDIO_WIDGET_CAPS);
    w->type = (w->caps >> 20) & 0x0F;

    LOG(DEBUG, "Found widget at NID %02x type=%d\n", nid, w->type);
    
    // Prepare connection table
    uint32_t conn_len = hda_getParameter(hda, codec, nid, HDA_PARAM_CONNECTION_LIST_LENGTH);

    // HDAs can specify a higher connection tbl size if the msb is set
    bool islong = (conn_len & 0x80);
    conn_len &= 0x7F;

    w->nconn = 0;
    for (unsigned i = 0; i < conn_len; i += ((islong) ? 2 : 4)) {
        uint32_t resp;
        
        int r = hda_sendCommand(hda, codec, nid, HDA_GET_CONN_LIST, i, &resp);
        if (r != 0) {
            return r;
        }

        // todo ts sucks
        for (unsigned j = 0; j < ((islong)?2:4) && w->nconn < conn_len; j++) {
            uint8_t conn_nid;
            if (islong) conn_nid = (resp >> (j*16)) & 0xffff;
            else conn_nid = (resp >> (j*8)) & 0xff;

            bool range = conn_nid & (islong ? 0x8000 : 0x80);
            conn_nid &= (islong) ? 0x7fff : 0x7f;

            if (range && w->nconn) {
                uint8_t prev = w->conntbl[w->nconn-1];
                for (uint8_t r = prev+1; r <= conn_nid; r++) w->conntbl[w->nconn++] = r;
            } else {
                w->conntbl[w->nconn++] = conn_nid;
            }
        }
    }

    LOG(DEBUG, "Probed %d connections\n", w->nconn);
    return 0;
}

/**
 * @brief Initialize function group
 */
static int hda_initFunctionGroup(hda_t *hda, uint8_t codec, int fg_idx) {
    uint32_t node_count = hda_getParameter(hda, codec, fg_idx, HDA_PARAM_SUB_NODE_COUNT);
    uint8_t w_start = (node_count >> 16) & 0xFF;
    uint8_t w_total = node_count & 0xFF;

    hda_fg_t *fg = kzalloc(sizeof(hda_fg_t));
    fg->next = hda->codecs[codec].fgs;
    hda->codecs[codec].fgs = fg;

    for (unsigned i = w_start; i < w_start+w_total; i++) {
        int r = hda_initWidget(hda, codec, fg, i);
        if (r != 0) return r;
    }

    return 0;
}

/**
 * @brief Trace path backwards
 */
static int hda_tracePath(hda_fg_t *fg, uint8_t type, hda_widget_t *curr, uint8_t *path_out, int depth, int max_depth) {
    if (depth >= max_depth) {
        LOG(ERR, "Exceeded maximum depth while tracing path\n");
        return -1;
    }

    for (int i = 0; i < depth; i++) {
        if (path_out[i] == curr->nid) {
            LOG(ERR, "Detected a cycle while tracing path\n");
            return -1;
        }
    }

    path_out[depth++] = curr->nid;

    if (curr->type == type) {
        return depth;
    }

    for (unsigned i = 0; i < curr->nconn; i++) {
        uint8_t prev_nid = curr->conntbl[i];

        hda_widget_t *prev = hda_findByNID(fg, prev_nid);
        if (!prev) continue;

        int r = hda_tracePath(fg, type, prev, path_out, depth, max_depth);
        if (r > 0) {
            return r; 
        }
    }

    return -1;
}

/**
 * @brief Find path to widget
 */
int hda_findPath(hda_fg_t *fg, hda_widget_t *start, uint8_t type, uint8_t *out_path, int max_depth) {
    return hda_tracePath(fg, type, start, out_path, 0, max_depth);
}

/**
 * @brief Initialize codec
 */
static int hda_initCodec(hda_t *hda, uint8_t codec) {
    // Get the number of function groups
    uint32_t nodes = hda_getParameter(hda, codec, 0, HDA_PARAM_SUB_NODE_COUNT);
    hda->codecs[codec].addr = codec;

    uint8_t fg_start = (nodes >> 16) & 0xff;
    uint8_t fg_total = nodes & 0xff;

    LOG(DEBUG, "This codec has %d FGs, starting at %d\n", fg_start, fg_total);

    for (unsigned i = fg_start; i < fg_start + fg_total; i++) {
        uint32_t fg_type = hda_getParameter(hda, codec, i, HDA_PARAM_FUNCTION_GROUP_TYPE);
        LOG(INFO, "FG type is 0x%x\n", fg_type);
        if ((fg_type & 0xFF) == HDA_FUNCTION_GROUP_AFG) {
            LOG(INFO, "Found AFG function group at NID %d\n", i);
            
            int r = hda_initFunctionGroup(hda, codec, i);
            if (r != 0) {
                return r;
            }
        }
    }

    hda_fg_t *fg = hda->codecs[codec].fgs;
    while (fg) {
        hda_widget_t *w = fg->widgets;
        while (w) {
            if (w->type == HDA_WIDGET_TYPE_PIN_COMPLEX) {
                uint32_t cfg_default;
                if (hda_sendCommand(hda, codec, w->nid, HDA_GET_CONFIG_DEFAULT, 0, &cfg_default) == 0) {
                    uint8_t port_conn = (cfg_default >> HDA_CFG_PORT_CONN_SHIFT) & HDA_CFG_PORT_CONN_MASK;
                    if (port_conn == HDA_CFG_PORT_CONN_NONE) {
                        LOG(DEBUG, "Pin NID %02x has no physical connection, skipping\n", w->nid);
                        w = w->next;
                        continue;
                    }
                }

                uint8_t path[8];
                int len = hda_findPath(fg, w, HDA_WIDGET_TYPE_DAC, path, 8);

                if (len > 0) {
                    LOG(INFO, "Mapping pin to DAC successfully, initializing stream\n");
                    
                    int r = hda_createOutputStream(hda, codec, fg, path, len);
                    if (r != 0) return r;
                }
            }

            w = w->next;
        }

        fg = fg->next;
    }

    return 0;
}

/**
 * @brief HDA init
 */
static int hda_init(pci_device_t *dev) {
    LOG(INFO, "Found Intel HDA controller at %d.%d.%d\n", dev->bus, dev->slot, dev->function);

    // Enable memory space and bus-mastering
    uint16_t cmd;
    pci_readConfigWord(dev, PCI_COMMAND_OFFSET, &cmd);
    cmd |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    cmd &= ~(PCI_COMMAND_IO_SPACE);
    pci_writeConfigWord(dev, PCI_COMMAND_OFFSET, cmd);

    // Allocate HDA device
    hda_t *hda = kzalloc(sizeof(hda_t));
    MUTEX_INIT(&hda->cmd_lock);
    hda->pcidev = dev;
    
    // Map MMIO
    pci_bar_t *b = pci_getBAR(dev, 0);
    if (!b || b->mapped == 0) {
        LOG(ERR, "PCI BAR0 is invalid.\n");
        kfree(hda);
        return 1;
    }

    hda->regs = (volatile hda_regs_t*)b->mapped;

    LOG(INFO, "Version %d.%d\n", hda->regs->vmaj, hda->regs->vmin);
    LOG(INFO, "statests=0x%x\n", hda->regs->statests);

    hda->output_streams = (hda->regs->gcap >> 12) & 0x0f;
    hda->input_streams = (hda->regs->gcap >> 8) & 0x0f;
    hda->bidir_streams = (hda->regs->gcap >> 3) & 0x1f;
    LOG(INFO, "Output=%d input=%d bidir=%d\n", hda->output_streams, hda->input_streams, hda->bidir_streams);
    hda->streams = kzalloc(sizeof(hda_stream_t*) * (hda->output_streams + hda->input_streams + hda->bidir_streams));

    // 64-bit addressing is required
    if ((hda->regs->gcap & 1) == 0) {
        LOG(ERR, "This HDA controller does not support 64-bit addressing (TODO)\n");
        kfree(hda);
        return 1;
    }

    // Reset the HDA
    if (hda_reset(hda)) {
        LOG(ERR, "Failed to reset the HDA controller.\n");
        kfree(hda);
        return 1;
    }

    // Initialize interrupts
    int got = pci_allocateInterrupts(dev, 1, 1, PCI_IRQ_ALL);
    if (got != 1) {
        LOG(ERR, "Failed to allocate interrupts (%d)\n", got);
        kfree(hda);
        return 1;
    }

    pci_irq_t *irq = pci_getInterruptVector(dev, 0);
    assert(irq_register(irq->vector, hda_irq, IRQ_FLAG_DEFAULT, hda, NULL) == 0);

    // Initialize CORB and RIRB
    if (hda_initBuffers(hda)) {
        LOG(ERR, "Failed to initialize CORB/RIRB\n");
        kfree(hda);
        return 1;
    }

    // Enable all interrupts
    hda->regs->intctl = 0xFFFFFFFF;

    // Create audio device
    hda->dev = audio_createDevice(hda);

    // Probe codecs
    for (unsigned i = 0; i < 15; i++) {
        if (hda->regs->statests & (1 << i)) {
            LOG(DEBUG, "Initializing codec %d\n", i);
            int r = hda_initCodec(hda, i);
            if (r != 0) {
                LOG(ERR, "Codec %d failed to initialize\n", i);
                kfree(hda);
                return r;
            }
        }
    }

    return 0;
}

/**
 * @brief HDA scan callback
 */
static int hda_scan(pci_device_t *dev, void *context) {
    return hda_init(dev);
}

/**
 * @brief Driver init
 */
int driver_init(int argc, char *argv[]) {
    pci_scan_parameters_t params = {
        .id_list = NULL,
        .class_code = 0x04,
        .subclass_code = 0x03
    };

    return pci_scanDevice(hda_scan, &params, NULL);
}

/**
 * @brief Driver deinit
 */
int driver_deinit(int argc, char *argv[]) {
    return 0;
}

struct driver_metadata driver_metadata = {
    .name = "Intel HD Audio Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
