/**
 * @file drivers/audio/hda/hda.h
 * @brief Intel High Definition Audio
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef _HDA_H
#define _HDA_H

/**** INCLUDES ****/
#include <kernel/subsystems/audio.h>
#include <kernel/misc/waitqueue.h>
#include <kernel/drivers/pci.h>
#include <kernel/misc/util.h>
#include <stdint.h>


/**** DEFINITIONS ****/

#define HDA_GCTL_CRST               (1 << 0)

#define HDA_CORBCTL_CORBRUN         (1 << 1)
#define HDA_RIRBCTL_RIRBRUN         (1 << 1)

#define HDA_GET_PARAMETERS                  0xF00
#define HDA_GET_CONN_LIST                   0xF02
#define HDA_GET_CONFIG_DEFAULT              0xF1C

#define HDA_CFG_PORT_CONN_SHIFT             30
#define HDA_CFG_PORT_CONN_MASK              0x3
#define HDA_CFG_PORT_CONN_JACK              0x0
#define HDA_CFG_PORT_CONN_NONE              0x1
#define HDA_CFG_PORT_CONN_FIXED             0x2
#define HDA_CFG_PORT_CONN_BOTH              0x3

#define HDA_PARAM_SUB_NODE_COUNT            0x04
#define HDA_PARAM_FUNCTION_GROUP_TYPE       0x05
#define HDA_PARAM_AUDIO_WIDGET_CAPS         0x09
#define HDA_PARAM_PIN_CAPS                  0x0C
#define HDA_PARAM_CONNECTION_LIST_LENGTH    0x0E
#define HDA_PARAM_OUTPUT_AMP_CAPS           0x12

#define HDA_FUNCTION_GROUP_AFG              1

#define HDA_WIDGET_TYPE_DAC                 0x00
#define HDA_WIDGET_TYPE_AUDIO_IN            0x01
#define HDA_WIDGET_TYPE_MIXER               0x02
#define HDA_WIDGET_TYPE_SELECTOR            0x03
#define HDA_WIDGET_TYPE_PIN_COMPLEX         0x04
#define HDA_WIDGET_TYPE_POWER               0x05
#define HDA_WIDGET_TYPE_VOLUME              0x06

#define HDA_MAX_CONNECTIONS                 32

#define HDA_RATE_BASE_48KHZ                 (0 << 14)
#define HDA_RATE_BASE_441KHZ                (1 << 14)

#define HDA_RATE_MULT_1X                    (0 << 11)
#define HDA_RATE_MULT_2X                    (1 << 11)
#define HDA_RATE_MULT_3X                    (2 << 11)
#define HDA_RATE_MULT_4X                    (3 << 11)

#define HDA_RATE_DIV_1X                     (0 << 8)
#define HDA_RATE_DIV_2X                     (1 << 8)
#define HDA_RATE_DIV_3X                     (2 << 8)
#define HDA_RATE_DIV_4X                     (3 << 8)
#define HDA_RATE_DIV_6X                     (5 << 8)

#define HDA_AMP_MUTE                        (1 << 7)
#define HDA_AMP_SET_RIGHT                   (1 << 12)
#define HDA_AMP_SET_LEFT                    (1 << 13)
#define HDA_AMP_SET_INPUT                   (1 << 14)
#define HDA_AMP_SET_OUTPUT                  (1 << 15)

#define HDA_PIN_CAP_HDPHONE                 (1 << 2)
#define HDA_PIN_CAP_OUTPUT                  (1 << 4)
#define HDA_PIN_CAP_INPUT                   (1 << 5)
#define HDA_PIN_CAP_EAPD                    (1 << 16)

#define HDA_CONFIG_CONNECTIVITY(x)          (((x) >> 30) & 0x3)
#define HDA_CONFIG_DEF_DEVICE(x)            (((x) >> 20) & 0xF)
#define HDA_CONFIG_COLOR(x)                 (((x) >> 12) & 0xF)

#define HDA_CONFIG_CONN_JACK                0
#define HDA_CONFIG_CONN_NONE                1
#define HDA_CONFIG_CONN_FIXED               2
#define HDA_CONFIG_CONN_BOTH                3

#define HDA_CONFIG_DEVICE_LINE_OUT          0
#define HDA_CONFIG_DEVICE_SPEAKER           1
#define HDA_CONFIG_DEVICE_HP_OUT            2
#define HDA_CONFIG_DEVICE_CD                3
#define HDA_CONFIG_DEVICE_SPDIF_OUT         4
#define HDA_CONFIG_DEVICE_DIGITAL_OUT       5
// todo the ins


/**** TYPES ****/

typedef struct hda_bdl_entry {
    uint64_t address;
    uint32_t length;
    uint32_t ioc;
} __attribute__((packed)) hda_bdl_entry_t;

typedef struct hda_stream_regs_t {
    uint8_t sdctl[3];
    uint8_t sdsts;
    uint32_t sdlpib;
    uint32_t sdlcbl;
    uint16_t sdlvi;
    uint8_t rsvd0[2];
    uint16_t sdfifos;
    uint16_t sdfmt;
    uint8_t rsvd1[4];
    uint32_t sdbdpl;
    uint32_t sdbdpu;
} __attribute__((packed)) hda_stream_regs_t;

STATIC_ASSERT(sizeof(hda_stream_regs_t) == 32);

typedef struct hda_regs {
    uint16_t gcap;
    uint8_t vmin;
    uint8_t vmaj;
    uint16_t outpay;
    uint16_t inpay;
    uint32_t gctl;
    uint16_t wakeen;
    uint16_t statests;
    uint16_t gsts;
    uint8_t rsvd0[6];
    uint16_t outstrmpay;
    uint16_t instrmpay;
    uint32_t rsvd1;
    uint32_t intctl;
    uint32_t intsts;
    uint64_t rsvd2;
    uint32_t wallclk;
    uint32_t rsvd3;
    uint32_t ssync;
    uint32_t rsvd4;
    uint32_t corblbase;
    uint32_t corbubase;
    uint16_t corbwp;
    uint16_t corbrp;
    uint8_t corbctl;
    uint8_t corbsts;
    uint8_t corbsize;
    uint8_t rsvd5;
    uint32_t rirblbase;
    uint32_t rirbubase;
    uint16_t rirbwp;
    uint16_t rintcnt;
    uint8_t rirbctl;
    uint8_t rirbsts;
    uint8_t rirbsize;
    uint8_t rsvd6;
    uint32_t icoi;
    uint32_t icii;
    uint16_t icis;
    uint8_t rsvd7[6];
    uint32_t dplbase;
    uint32_t dpubase;
    uint64_t rsvd8;    
} __attribute__((packed)) hda_regs_t;

STATIC_ASSERT(sizeof(hda_regs_t) == 0x80);


typedef struct hda_widget {
    struct hda_widget *next;
    uint8_t nid;
    uint8_t type;
    uint32_t caps;

    uint8_t conntbl[HDA_MAX_CONNECTIONS];
    size_t nconn;
} hda_widget_t;

typedef struct hda_fg {
    struct hda_fg *next;
    uint8_t nid;
    hda_widget_t *widgets;
} hda_fg_t;

typedef struct hda_codec {
    uint8_t addr;
    hda_fg_t *fgs;
} hda_codec_t;

struct hda;
typedef struct hda_stream {
    struct hda *parent;
    int index;
    uint8_t codec;
    uint8_t pin_nid;
    uint8_t dac_nid;
    hda_bdl_entry_t *bdl;
    audio_stream_t *s;
    volatile hda_stream_regs_t *regs;
} hda_stream_t;

typedef struct hda_control {
    struct hda *hda;
    hda_stream_t *stream;
    hda_widget_t *w;
} hda_control_t;

typedef struct hda {
    pci_device_t *pcidev;
    audio_device_t *dev;
    volatile hda_regs_t *regs;
    mutex_t cmd_lock;
    wait_queue_t cmd_queue;

    struct {
        uint32_t *vaddr;
        unsigned int index;
        size_t sz;
    } corb;

    struct {
        uint64_t *vaddr;
        unsigned int index;
        size_t sz;
    } rirb;

    hda_codec_t codecs[16];

    hda_stream_t **streams;
    int nstreams;

    uint8_t output_streams;
    uint8_t input_streams;
    uint8_t bidir_streams;
} hda_t;

/**** MACROS ****/
#define HDA_CORB(codec, node_id, command, data) (uint32_t)(((codec) << 28) | ((node_id) << 20) | ((command) << 8) | ((data) & 0xFF))

#endif
