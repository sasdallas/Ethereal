/**
 * @file hexahedron/include/kernel/drivers/usb/hid/hid.h
 * @brief USB Human Interface Device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_USB_HID_HID_H
#define DRIVERS_USB_HID_HID_H

/**** INCLUDES ****/
#include <stdint.h>

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

#define HID_REPORT_COLLECTION_PHYSICAL          0
#define HID_REPORT_COLLECTION_APPLICATION       1
#define HID_REPORT_COLLECTION_LOGICAL           2
#define HID_REPORT_COLLECTION_REPORT            3
#define HID_REPORT_COLLECTION_NAMED_ARRAY       4
#define HID_REPORT_COLLECTION_USAGE_SWITCH      5
#define HID_REPORT_COLLECTION_USAGE_MODIFIER    6

/**** TYPES ****/

typedef union USBHidOpcode {
    uint8_t raw;
    struct {
        uint8_t size:2;
        uint8_t desc_type:2;
        uint8_t opcode:4;
    };
} USBHidOpcode_t;

typedef struct USBHidReportItem {
    uint8_t opcode;                 // HID_REPORT_MAIN_ opcode
    uint8_t flags;                  // Additional opcode flags
    
    uint16_t usage_page;
    uint32_t usage_id;

    uint8_t report_id;
    uint32_t report_count;
    uint32_t report_size;

    uint32_t logical_min;
    uint32_t logical_max;

    uint32_t phys_min;
    uint32_t phys_max;

    uint32_t usage_min;
    uint32_t usage_max;
} USBHidReportItem_t;

typedef struct USBHidParserState {
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
} USBHidParserState_t;

typedef struct USBHidCollection {
    uint8_t type;
    list_t *items;
    list_t *collections;
} USBHidCollection_t;

typedef struct USBHidLocalState {
    uint32_t usage;
    uint32_t usage_minimum;
    uint32_t usage_maximum;
} USBHidLocalState_t;


typedef struct USBHidOptionalDescriptor {
    uint8_t bDescriptorType;
    uint16_t wItemLength;
} __attribute__((packed)) USBHidOptionalDescriptor_t;

typedef struct USBHidDescriptor {
    uint8_t bLength;                    // Size of this descriptor in bytes
    uint8_t bDescriptorType;            // USB_DESC_HID
    uint16_t bcdHid;                    // HID class specification release number 
    uint8_t bCountryCode;               // Country code
    uint8_t bNumDescriptors;            // Number of optional descriptors
    USBHidOptionalDescriptor_t desc[];  // Optional descriptors
} USBHidDescriptor_t;

typedef struct USBHidDevice {
    USBEndpoint_t *endp;                // Target interrupt in endpoint
    USBTransfer_t transfer;             // Generic transfer
} USBHidDevice_t;

/**** FUNCTIONS ****/

/**
 * @brief Register and initialize HID drivers
 */
void hid_init();

#endif