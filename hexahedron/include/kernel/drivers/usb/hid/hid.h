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

#define HID_INPUT_FLAG_VARIABLE                 0x2
#define HID_INPUT_FLAG_RELATIVE                 0x4

#define HID_MAX_USAGE_STACK                     32

/**** TYPES ****/


struct USBHidCollection;
struct USBHidReportItem;

/**
 * @brief Try to initialize a USB HID collection
 * @param collection The collection to attempt to initialize
 * @returns @c USB_SUCCESS on success 
 */
typedef USB_STATUS (*hid_init_collection_t)(struct USBHidCollection *collection);

/**
 * @brief Try to deinitialize a USB HID collection
 * @param collection The collection to deinitialize
 * @returns @c USB_STATUS on success
 */
typedef USB_STATUS (*hid_deinit_collection_t)(struct USBHidCollection *collection);

/**
 * @brief Begin report
 * @param collection The collection to begin the report on
 * @returns @c USB_STATUS on success
 */
typedef USB_STATUS (*hid_begin_report_t)(struct USBHidCollection *collection);

/**
 * @brief Finish report
 * @param collection The collection to finish the report on
 * @returns @c USB_STATUS on success
 */
typedef USB_STATUS (*hid_finish_report_t)(struct USBHidCollection *collection);

/**
 * @brief Process a relative data variable
 * @param collection The collection to process the variable on
 * @param item The report item being processed
 * @param usage_page The usage page of the variable
 * @param usage_id The usage ID
 * @param value The value
 * 
 * @note Only trust usage page and ID passed to you 
 */
typedef USB_STATUS (*hid_process_relative_var_t)(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, uint32_t usage_id, int64_t value);

/**
 * @brief Process an absolute data variable
 * @param collection The collection to process the variable on
 * @param item The report item being processed
 * @param usage_page The usage page of the variable
 * @param usage_id The usage ID
 * @param value The value
 * 
 * @note Only trust usage page and ID passed to you 
 */
typedef USB_STATUS (*hid_process_absolute_var_t)(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, uint32_t usage_id, int64_t value);

/**
 * @brief Process a data array
 * @param collection The collection to process the array on
 * @param item The report item being processed
 * @param usage_page The usage page of the array
 * @param array The data array
 * 
 * @note Only trust usage page and ID passed to you 
 */
typedef USB_STATUS (*hid_process_array_t)(struct USBHidCollection *collection, struct USBHidReportItem *item, uint16_t usage_page, int64_t array);

/**
 * @brief HID device driver object
 */
typedef struct USBHidDeviceDriver {
    char *name;                             // Driver name
    uint16_t usage_id;                      // Target usage ID
    uint16_t usage_page;                    // Target usage page

    hid_begin_report_t begin;               // Begin report
    hid_finish_report_t finish;             // Finish report

    hid_init_collection_t init;             // Init collection method
    hid_deinit_collection_t deinit;         // Deinit collection method
    hid_process_relative_var_t relative;    // Process relative variable method
    hid_process_absolute_var_t absolute;    // Process absolute variable method
    hid_process_array_t array;              // Process array method
} USBHidDeviceDriver_t;


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

    int32_t logical_min;
    int32_t logical_max;

    int32_t phys_min;
    int32_t phys_max;

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

    uint8_t has_report_id;
} USBHidParserState_t;

struct USBHidDevice;
typedef struct USBHidCollection {
    uint8_t opcode;                     // HID_REPORT_MAIN_COLLECTION
    uint8_t type;                       // Type of the collection
    uint16_t usage_page;                // Usage page of the collection
    uint32_t usage_id;                  // Usage ID of the collection

    list_t *items;                      // List of items in the collection. This can also contain collections.
    struct USBHidDevice *dev;           // HID device

    USBHidDeviceDriver_t *driver;       // Selected driver
    void *d;                            // Driver-specific
} USBHidCollection_t;

typedef struct USBHidLocalState {
    uint32_t usage_stack[HID_MAX_USAGE_STACK];
    uint8_t usage_stack_len;

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
    USBInterface_t *intf;               // Interface
    USBEndpoint_t *in_endp;             // INTERRUPT IN endpoint
    USBEndpoint_t *out_endp;            // INTERRUPT OUT endpoint
    USBTransfer_t transfer;             // Generic transfer
    uint8_t uses_report_id;             // Device uses report ID
    list_t *collections;                // Collection list
} USBHidDevice_t;

/**** FUNCTIONS ****/

/**
 * @brief Register and initialize HID drivers
 */
void hid_init();

/**
 * @brief Register an HID driver
 * @param driver The driver to register
 */
void hid_registerDriver(USBHidDeviceDriver_t *driver);

#endif