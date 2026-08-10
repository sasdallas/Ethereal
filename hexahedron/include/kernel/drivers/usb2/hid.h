/**
 * @file hexahedron/include/kernel/drivers/usb2/hid.h
 * @brief HID data
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_USB_HID_H
#define DRIVERS_USB_HID_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>
#include <structs/list.h>
#include <kernel/drivers/usb2/protocol.h>
#include <kernel/drivers/usb2/status.h>

/**** DEFINITIONS ****/

#define HID_REQ_GET_REPORT          0x01
#define HID_REQ_GET_IDLE            0x02
#define HID_REQ_GET_PROTOCOL        0x03
#define HID_REQ_SET_REPORT          0x09
#define HID_REQ_SET_IDLE            0x0A
#define HID_REQ_SET_PROTOCOL        0x0B

#define HID_REPORT_MAIN                     0b00
#define HID_REPORT_GLOBAL                   0b01
#define HID_REPORT_LOCAL                    0b10

#define HID_REPORT_MAIN_INPUT               0b1000
#define HID_REPORT_MAIN_OUTPUT              0b1001
#define HID_REPORT_MAIN_FEATURE             0b1011
#define HID_REPORT_MAIN_COLLECTION          0b1010
#define HID_REPORT_MAIN_END_COLLECTION      0b1100

#define HID_REPORT_GLOBAL_USAGE_PAGE        0b0000
#define HID_REPORT_GLOBAL_LOGICAL_MINIMUM   0b0001
#define HID_REPORT_GLOBAL_LOGICAL_MAXIMUM   0b0010
#define HID_REPORT_GLOBAL_PHYSICAL_MINIMUM  0b0011
#define HID_REPORT_GLOBAL_PHYSICAL_MAXIMUM  0b0100
#define HID_REPORT_GLOBAL_UNIT_EXPONENT     0b0101
#define HID_REPORT_GLOBAL_UNIT              0b0110
#define HID_REPORT_GLOBAL_REPORT_SIZE       0b0111
#define HID_REPORT_GLOBAL_REPORT_ID         0b1000
#define HID_REPORT_GLOBAL_REPORT_COUNT      0b1001
#define HID_REPORT_GLOBAL_PUSH              0b1010
#define HID_REPORT_GLOBAL_POP               0b1011

#define HID_REPORT_LOCAL_USAGE              0b0000
#define HID_REPORT_LOCAL_USAGE_MINIMUM      0b0001
#define HID_REPORT_LOCAL_USAGE_MAXIMUM      0b0010
#define HID_REPORT_LOCAL_DESIGNATOR_IDX     0b0011
#define HID_REPORT_LOCAL_DESIGNATOR_MINIMUM 0b0100
#define HID_REPORT_LOCAL_DESIGNATOR_MAXIMUM 0b0101
#define HID_REPORT_LOCAL_STRING_INDEX       0b0111
#define HID_REPORT_LOCAL_STRING_MINIMUM     0b1000
#define HID_REPORT_LOCAL_STRING_MAXIMUM     0b1001
#define HID_REPORT_LOCAL_DELIMETER          0b1010

#define HID_COLLECTION_TYPE_PHYSICAL        0
#define HID_COLLECTION_TYPE_APPLICATION     1
#define HID_COLLECTION_TYPE_LOGICAL         2
#define HID_COLLECTION_TYPE_REPORT          3
#define HID_COLLECTION_TYPE_NAMED_ARRAY     4
#define HID_COLLECTION_TYPE_USAGE_SWITCH    5
#define HID_COLLECTION_TYPE_USAGE_MODIFIER  6

#define HID_CLASS                           0x3
#define HID_DESC_TYPE                       0x21

#define HID_DESC_REPORT                     0x22

#define HID_INPUT_FLAG_VARIABLE             0x2
#define HID_INPUT_FLAG_RELATIVE             0x4

/* used in the actual events */
#define HID_EVENT_FLAG_CONSTANT             0x1
#define HID_EVENT_FLAG_VARIABLE             0x2
#define HID_EVENT_FLAG_RELATIVE             0x4

/* limitation */
#define HID_MAX_USAGE                       32

/* matching data */
#define HID_MATCH_ANY_USAGE_ID              (uint32_t)0xFFFFFFFF
#define HID_MATCH_ANY_USAGE_PAGE            (uint16_t)0xFFFF
#define HID_MATCH_NONE { 0 }

/**** TYPES ****/

struct usb_device;
struct usb_transfer;
struct hid_driver;
struct hid_device;
struct hid_collection;
struct hid_field;

typedef uint8_t hid_item_t;

typedef struct hid_event {
    struct hid_device *device;
    uint32_t usage_page;
    uint32_t usage;
    int64_t value;
    uint8_t report_id;
    uint32_t flags;

    struct hid_field *field;
} hid_event_t;

typedef struct hid_parser_state {
    uint8_t *data;              // Current parser pointer
    uint8_t *end;               // End of descriptor in memory
    
    struct hid_collection *collection;
    struct hid_collection *app_collection;

    uint16_t usage_page;
    int32_t logical_minimum;
    int32_t logical_maximum;
    int32_t physical_minimum;
    int32_t physical_maximum;
    uint32_t unit_exponent;
    uint32_t unit;
    uint32_t report_size;
    uint32_t report_count;
    uint8_t report_id;
    bool has_report_id;
    bool has_usage_range;

    // locals, reset after an input/output/feature item
    uint32_t usage_stack[32];
    uint8_t usage_stack_len;
    uint32_t usage_minimum;
    uint32_t usage_maximum;
} hid_parser_state_t;

typedef struct hid_parser_item {
    hid_item_t item;
    size_t report_size;
    int32_t signed_val;
    uint32_t unsigned_val;
} hid_parser_item_t;

typedef enum hid_field_type {
    HID_FIELD_INPUT,
    HID_FIELD_OUTPUT,
    HID_FIELD_FEATURE,
} hid_field_type_t;

typedef struct hid_field {
    STAILQ_ENTRY(struct hid_field) node;
    hid_field_type_t type;
    uint32_t flags;
    uint32_t bit_offset;
    uint32_t report_size;
    uint32_t report_count;
    uint8_t report_id;
    int32_t logical_min;
    int32_t logical_max;
    int32_t physical_min;
    int32_t physical_max;
    uint32_t unit;
    int32_t unit_exponent;
    uint16_t usage_page;

    uint32_t usage_min;
    uint32_t usage_max;
    bool usage_range;    
    size_t num_usage;
    uint32_t usages[HID_MAX_USAGE];

    // for arrays only
    int32_t *last_state;
    int32_t *current_state;

    struct hid_collection *parent;
    struct hid_collection *app;
} hid_field_t;

typedef struct hid_report {
    uint32_t report_id;
    hid_field_type_t field_type;
    uint32_t bit_length;
    size_t num_fields;
    hid_field_t *fields;
} hid_report_t;

typedef struct hid_collection {
    STAILQ_ENTRY(struct hid_collection) node;
    struct hid_device *device;
    
    uint8_t type;
    uint16_t usage_page;
    uint32_t usage_id;
    
    bool touched;

    struct hid_driver *driver;
    void *priv;
} hid_collection_t;

typedef struct hid_device {
    struct usb_device *device;
    struct usb_interface *intf;
    struct usb_transfer *transfer;
    struct usb_pipe *in_pipe;
    struct usb_pipe *out_pipe;
    bool uses_report_id;

    void *buffer;

    STAILQ_HEAD(collections, hid_collection_t);
    SLIST_ENTRY(struct hid_device) node;
    
    hid_report_t *input_reports[256];
    hid_report_t *output_reports[256];
    hid_report_t *feature_reports[256];
} hid_device_t;

typedef struct hid_match {
    uint16_t usage_page;
    uint32_t usage_id;
} hid_match_t;

typedef struct hid_driver {
    char *name;
    hid_match_t *matches;
    size_t num_matches;

    STAILQ_ENTRY(struct hid_driver) node;

    struct {
        bool (*probe)(hid_collection_t *);
        void (*attach)(hid_collection_t *);
        void (*remove)(hid_collection_t *);
        void (*event)(hid_collection_t *, hid_event_t *);
        void (*end)(hid_collection_t *);
    } ops;
} hid_driver_t;

/**** MACROS ****/

#define HID_ITEM_SIZE(item) ((item) & 0x3)
#define HID_ITEM_TYPE(item) (((item) >> 2) & 0x3)
#define HID_ITEM_TAG(item) (((item) >> 4) & 0xF)

/**** FUNCTIONS ****/

/**
 * @brief Register an HID device driver
 * @param driver The driver to register
 */
void hid_register(hid_driver_t *driver);

/**
 * @brief Parse the bytecode for a HID device
 * @param device The device to parse the bytecode for
 * @param desc The HID descriptor
 */
usb_status_t hid_parse(hid_device_t *device, usb_hid_desc_t *desc);

/**
 * @brief Parse report for HID device
 * @param device The device to parse the report for
 * @param report The report buffer
 * @param report_size The size of the report processed
 */
void hid_process(hid_device_t *device, void *report, size_t report_size);

#endif
