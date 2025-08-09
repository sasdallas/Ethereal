/**
 * @file hexahedron/drivers/usb/hid/hid.c
 * @brief USB human interface device handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/usb/usb.h>
#include <kernel/drivers/usb/hid/hid.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/misc/util.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID", __VA_ARGS__)

/* Tabs for debug */
static int tabs = 0;

/* i hate C */
#pragma GCC diagnostic ignored "-Wsign-compare"
#define HID_CAST_REPORT_SIZE(s, val) ((s) ? (signed)(val) : (unsigned)(val))

void hid_printOpcode(USBHidParserState_t *parser, USBHidOpcode_t opc, uint32_t val, uint8_t report_size) {
    LOG(DEBUG, "PARSER: ");
    for (int i = 0; i < tabs; i++) {
        dprintf(NOHEADER, "\t");
    }

    if (opc.desc_type == HID_REPORT_MAIN) {
        switch (opc.opcode) {
            case HID_REPORT_MAIN_INPUT:
                dprintf(NOHEADER, "Input()\n"); break;
            case HID_REPORT_MAIN_OUTPUT:
                dprintf(NOHEADER, "Output()\n"); break;
            case HID_REPORT_MAIN_FEATURE:
                dprintf(NOHEADER, "Feature\n"); break;
            case HID_REPORT_MAIN_COLLECTION:
                dprintf(NOHEADER, "Collection"); break;
            case HID_REPORT_MAIN_END_COLLECTION:
                dprintf(NOHEADER, "EndCollection()\n"); break;
            default:
                dprintf(NOHEADER, "Unknown()\n"); return;
        }

        if (opc.opcode == HID_REPORT_MAIN_COLLECTION) {
            uint8_t collection = HID_CAST_REPORT_SIZE(0, val);
            switch (collection) {
                case HID_REPORT_COLLECTION_PHYSICAL:
                    dprintf(NOHEADER, "(Physical)\n"); break;
                case HID_REPORT_COLLECTION_APPLICATION:
                    dprintf(NOHEADER, "(Application)\n"); break;
                
                case HID_REPORT_COLLECTION_LOGICAL:
                    dprintf(NOHEADER, "(Logical)\n"); break;
                case HID_REPORT_COLLECTION_REPORT:
                    dprintf(NOHEADER, "(Report)\n"); break;
                case HID_REPORT_COLLECTION_NAMED_ARRAY:
                    dprintf(NOHEADER, "(NamedArray)\n"); break;
                case HID_REPORT_COLLECTION_USAGE_SWITCH:
                    dprintf(NOHEADER, "(UsageSwitch)\n"); break;
                case HID_REPORT_COLLECTION_USAGE_MODIFIER:
                    dprintf(NOHEADER, "(UsageModifier)\n"); break;

                default:
                    if (collection >= 0x80) {
                        dprintf(NOHEADER, "(VendorSpecific)\n");
                    } else {
                        dprintf(NOHEADER, "(0x%x)\n", collection);
                    }
                    break;
            }
        }
    } else if (opc.desc_type == HID_REPORT_GLOBAL)  {
        switch (opc.opcode) {
            case HID_REPORT_GLOBAL_USAGE_PAGE: dprintf(NOHEADER, "UsagePage(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_LOGICAL_MINIMUM: dprintf(NOHEADER, "LogicalMinimum(%d)\n", HID_CAST_REPORT_SIZE(1, (int8_t)val)); break;
            case HID_REPORT_GLOBAL_LOGICAL_MAXIMUM: dprintf(NOHEADER, "LogicalMaximum(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_PHYSICAL_MINIMUM: dprintf(NOHEADER, "PhysicalMinimum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_GLOBAL_PHYSICAL_MAXIMUM: dprintf(NOHEADER, "PhysicalMaximum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_GLOBAL_UNIT_EXPONENT: dprintf(NOHEADER, "UnitExponent(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_UNIT: dprintf(NOHEADER, "Unit(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_REPORT_SIZE: dprintf(NOHEADER, "ReportSize(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_REPORT_ID: dprintf(NOHEADER, "ReportId(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_REPORT_COUNT: dprintf(NOHEADER, "ReportCount(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_GLOBAL_PUSH: dprintf(NOHEADER, "Push()\n"); break;
            case HID_REPORT_GLOBAL_POP: dprintf(NOHEADER, "Pop()\n"); break;
            default: dprintf(NOHEADER, "??? (0x%x)\n", opc.opcode); break;
        }
    } else if (opc.desc_type == HID_REPORT_LOCAL) {
        switch (opc.opcode) {
            case HID_REPORT_LOCAL_USAGE: dprintf(NOHEADER, "UsageId(0x%x)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_LOCAL_USAGE_MAXIMUM: dprintf(NOHEADER, "UsageMaximum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_USAGE_MINIMUM: dprintf(NOHEADER, "UsageMinimum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_DESIGNATOR_IDX: dprintf(NOHEADER, "DesignatorIndex(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_LOCAL_DESIGNATOR_MINIMUM: dprintf(NOHEADER, "DesignatorMinimum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_DESIGNATOR_MAXIMUM: dprintf(NOHEADER, "DesignatorMaximum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_STRING_INDEX: dprintf(NOHEADER, "StringIndex(%d)\n", HID_CAST_REPORT_SIZE(0, val)); break;
            case HID_REPORT_LOCAL_STRING_MINIMUM: dprintf(NOHEADER, "StringMinimum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_STRING_MAXIMUM: dprintf(NOHEADER, "StringMaximum(%d)\n", HID_CAST_REPORT_SIZE(1, val)); break;
            case HID_REPORT_LOCAL_DELIMETER: dprintf(NOHEADER, "Delimeter\n"); break;
            default: dprintf(NOHEADER, "??? (0x%x)\n", opc.opcode); break;
        }
    } else {
        dprintf(NOHEADER, "???\n");
    }
}

/**
 * @brief Add item to collection
 * @param collection The collection to add an item to
 * @param state Global parser state
 * @param local_state Local state
 * @param type The type of item to add
 * @param flags Additional flags of the item
 */
void hid_addItemToCollection(USBHidCollection_t *collection, USBHidParserState_t *state, USBHidLocalState_t *local_state, uint8_t type, uint8_t flags) {
    USBHidReportItem_t *item = kzalloc(sizeof(USBHidReportItem_t));

    item->flags = flags;
    item->logical_max = state->logical_maximum;
    item->logical_min = state->logical_minimum;
    item->opcode = type;
    item->phys_max = state->physical_maximum;
    item->phys_min = state->physical_minimum;
    item->report_count = state->report_count;
    item->report_id = state->report_id;
    item->report_size = state->report_size;
    item->usage_id = local_state->usage;
    item->usage_page = state->usage_page;
    item->usage_max = local_state->usage_maximum;
    item->usage_min = local_state->usage_minimum;
    
    list_append(collection->items, item);
}

/**
 * @brief Process collection
 * @param state Parser state
 * @param p Data buffer input + output pointer
 * @param size Size of the buffer
 * @returns Collection object
 */
USBHidCollection_t *hid_parseCollection(USBHidParserState_t *state, uint8_t **p_out) {
    uint8_t *p = *p_out;
    
    tabs++;

    USBHidCollection_t *collection = kzalloc(sizeof(USBHidCollection_t));
    collection->collections = list_create("usb hid collections");
    collection->items = list_create("usb hid collection items");

    USBHidLocalState_t local_state = { 0 };

    // Starting opcode is Collection
    USBHidOpcode_t opcode = (USBHidOpcode_t)*p;
    assert(opcode.opcode == HID_REPORT_MAIN_COLLECTION);
    collection->type = *(p+1);
    p += 2;

    while (1) {
        USBHidOpcode_t opcode = (USBHidOpcode_t)*p;
        size_t report_size = 0;
        switch (opcode.size) {
            case 1: report_size = 1; break;
            case 2: report_size = 2; break;
            case 3: report_size = 4; break;
        }

        uint32_t val = 0;
        switch (report_size) {
            case 1: val = (uint32_t)(*(uint8_t*)(p+1)); break;
            case 2: val = (uint32_t)(*(uint16_t*)(p+1)); break;
            case 4: val = (uint32_t)(*(uint32_t*)(p+1)); break;
        }

        // Process the opcode
        if (opcode.desc_type == HID_REPORT_MAIN) {
            switch (opcode.opcode) {
                case HID_REPORT_MAIN_COLLECTION:
                    // Starting ANOTHER collection
                    hid_printOpcode(state, opcode, val, report_size);
                    uint8_t *p_target = p;
                    USBHidCollection_t *col = hid_parseCollection(state, &p_target);
                    p = p_target;
                    list_append(collection->collections, col);
                    continue;

                case HID_REPORT_MAIN_END_COLLECTION:
                    tabs--;
                    hid_printOpcode(state, opcode, val, report_size);
                    p += report_size + 1;
                    *p_out = p;
                    return collection;
            
                case HID_REPORT_MAIN_INPUT:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_INPUT, *(p+1));
                    break;

                case HID_REPORT_MAIN_OUTPUT:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_OUTPUT, *(p+1));
                    break;

                case HID_REPORT_MAIN_FEATURE:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_FEATURE, *(p+1));
                    break;
            }
        } else if (opcode.desc_type == HID_REPORT_GLOBAL) {
            switch (opcode.opcode) {
                case HID_REPORT_GLOBAL_USAGE_PAGE: state->usage_page = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_UNIT: state->unit = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_LOGICAL_MAXIMUM: state->logical_maximum = HID_CAST_REPORT_SIZE(0, (uint8_t)val); break;
                case HID_REPORT_GLOBAL_LOGICAL_MINIMUM: state->logical_minimum = HID_CAST_REPORT_SIZE(1, (int8_t)val); break;
                case HID_REPORT_GLOBAL_PHYSICAL_MAXIMUM: state->physical_maximum = HID_CAST_REPORT_SIZE(0, (uint8_t)val); break;
                case HID_REPORT_GLOBAL_PHYSICAL_MINIMUM: state->physical_minimum = HID_CAST_REPORT_SIZE(1, val); break;
                case HID_REPORT_GLOBAL_UNIT_EXPONENT: state->unit_exponent = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_SIZE: state->report_size = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_ID: state->report_id = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_COUNT: state->report_count = HID_CAST_REPORT_SIZE(0, val); break;

                default:
                    LOG(ERR, "Unrecognized global opcode: 0x%x\n", opcode.opcode);
                    break;
            }
        } else if (opcode.desc_type == HID_REPORT_LOCAL) {
            switch (opcode.opcode) {
                case HID_REPORT_LOCAL_USAGE: local_state.usage = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_LOCAL_USAGE_MAXIMUM: local_state.usage_maximum = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_LOCAL_USAGE_MINIMUM: local_state.usage_minimum = HID_CAST_REPORT_SIZE(0, val); break;
            
                // TODO: Store the rest
            }
        } 

        hid_printOpcode(state, opcode, val, report_size);

        p += report_size + 1;
    }

    tabs--;
    *p_out = p;
    return collection;
}

/**
 * @brief Dump collection
 */
void hid_dumpCollection(USBHidCollection_t *collection, int depth) {
    LOG(DEBUG, "DUMP: ");
    for (int i = 0; i < depth; i++) LOG(NOHEADER, "\t");
    LOG(NOHEADER, "Collection type %d\n", collection->type);

    foreach(item_node, collection->items) {
        USBHidReportItem_t *item = (USBHidReportItem_t*)item_node->value;
        for (int i = 0; i < depth; i++) LOG(NOHEADER, "\t");
        LOG(NOHEADER, "\tItem type=%x flags=%x report_id=%d report_count=%d report_size=%d\n", item->opcode, item->flags, item->report_id, item->report_count, item->report_size);
    }

    foreach(col, collection->collections) {
        hid_dumpCollection((USBHidCollection_t*)col->value, depth+1);
    }
} 

/**
 * @brief Process report descriptor
 * @param data Report data
 * @param size Size of report data
 * @returns A list of HID collections
 */
list_t *hid_parseReportDescriptor(uint8_t *data, size_t size) {
    uint8_t *p = data;

    HEXDUMP(p, size);

    USBHidParserState_t *state = kzalloc(sizeof(USBHidParserState_t));
    list_t *collections = list_create("usb hid collections");

    // Start parsing HID descriptor
    uint8_t *end = p + size;
    while (p < end) {
        // Acquire the descriptor type
        USBHidOpcode_t opcode = (USBHidOpcode_t)*p;
        size_t report_size = 0;
        switch (opcode.size) {
            case 1: report_size = 1; break;
            case 2: report_size = 2; break;
            case 3: report_size = 4; break;
        }

        uint32_t val = 0;
        switch (report_size) {
            case 1: val = (uint32_t)(*(uint8_t*)(p+1)); break;
            case 2: val = (uint32_t)(*(uint16_t*)(p+1)); break;
            case 4: val = (uint32_t)(*(uint32_t*)(p+1)); break;
        }

        hid_printOpcode(state, opcode, val, report_size);

        if (opcode.desc_type == HID_REPORT_MAIN) {
            if (opcode.opcode == HID_REPORT_MAIN_COLLECTION) {
                uint8_t *p_target = p;
                USBHidCollection_t *collection = hid_parseCollection(state, &p_target);
                list_append(collections, collection);
                p = p_target;
                continue;
            } else {
                LOG(WARN, "HID parser encountered an unexpected MAIN opcode at this time: 0x%x\n", opcode.opcode);
            }
        } else if (opcode.desc_type == HID_REPORT_GLOBAL) {
            switch (opcode.opcode) {
                case HID_REPORT_GLOBAL_USAGE_PAGE: state->usage_page = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_UNIT: state->unit = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_LOGICAL_MAXIMUM: state->logical_maximum = (int8_t)val; break;
                case HID_REPORT_GLOBAL_LOGICAL_MINIMUM: state->logical_minimum = (int8_t)val; break;
                case HID_REPORT_GLOBAL_PHYSICAL_MAXIMUM: state->physical_maximum = (int32_t)val; break;
                case HID_REPORT_GLOBAL_PHYSICAL_MINIMUM: state->physical_minimum = (int32_t)val; break;
                case HID_REPORT_GLOBAL_UNIT_EXPONENT: state->unit_exponent = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_SIZE: state->report_size = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_ID: state->report_id = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_GLOBAL_REPORT_COUNT: state->report_count = HID_CAST_REPORT_SIZE(0, val); break;

                case HID_REPORT_GLOBAL_PUSH: assert(0);
                case HID_REPORT_GLOBAL_POP: assert(0);

                default:
                    LOG(ERR, "Unrecognized global opcode: 0x%x\n", opcode.opcode);
                    break;
            }
        } else if (opcode.desc_type == HID_REPORT_LOCAL) {
            LOG(ERR, "TODO: Local opcode\n");
        }
    
        p += report_size + 1;
    } 

    foreach(collection, collections) {
        hid_dumpCollection((USBHidCollection_t*)collection->value, 0);
    } 

    return collections;
}

/**
 * @brief USB transfer callback
 * @param endp The endpoint the transfer was completed on
 * @param complete Completion structure
 */
void hid_callback(USBEndpoint_t *endp, USBTransferCompletion_t *complete) {
    LOG(DEBUG, "Transfer complete on endpoint %d, received %d bytes\n", USB_ENDP_GET_NUMBER(endp), complete->length);

    USBInterface_t *intf = (USBInterface_t*)complete->transfer->parameter;
    USBHidDevice_t *hid = (USBHidDevice_t*)intf->d;
    if (hid->endp != endp) return; // ???

    HEXDUMP(complete->transfer->data, complete->length);

    usb_interruptTransfer(intf->dev, &hid->transfer);
}


/**
 * @brief Initialize USB device
 * @param intf The interface to initialize on
 */
USB_STATUS hid_initializeDevice(USBInterface_t *intf) {
    if (intf->desc.bInterfaceClass != 3) {
        return USB_FAILURE;
    }

    LOG(INFO, "Initializing USB device as a HID\n");

    // Locate HID descriptor
    USBHidDescriptor_t *hid_desc = NULL;
    foreach(misc_descriptor, intf->additional_desc_list) {
        USBHidDescriptor_t *desc = (USBHidDescriptor_t*)misc_descriptor->value;
        if (desc->bDescriptorType == USB_DESC_HID) {
            LOG(DEBUG, "HidDescriptor with %d additional descriptors (blength = %d)\n", desc->bNumDescriptors, desc->bLength);
            hid_desc = desc;
            break;
        }
    }


    if (!hid_desc) {
        LOG(WARN, "Detected HID but missing USB_DESC_HID\n");
        return USB_FAILURE;
    }

    // Disable boot protocol
    if (intf->desc.bInterfaceSubClass == 1) {
        // Boot protocol supported
        if (usb_requestDevice(intf->dev, USB_RT_H2D | USB_RT_CLASS | USB_RT_INTF, HID_REQ_SET_PROTOCOL, 1, intf->desc.bInterfaceNumber, 0, NULL) != USB_TRANSFER_SUCCESS) {
            LOG(ERR, "HID_REQ_SET_PROTOCOL (Report) failed\n");
            return USB_FAILURE;
        } 
    }

    LOG(INFO, "HID version %d.%2d (country code: %d)\n", hid_desc->bcdHid >> 8, hid_desc->bcdHid & 0xFF, hid_desc->bCountryCode);

    int report_desc_count = 0;

    for (size_t i = 0; i < hid_desc->bNumDescriptors; i++) {
        LOG(DEBUG, "Located HID descriptor: %d\n", hid_desc->desc[i].bDescriptorType);
        if (hid_desc->desc[i].bDescriptorType != USB_DESC_REPORT) continue;

        size_t desc_length = hid_desc->desc[i].wItemLength;

        // Report descriptor located
        uintptr_t buffer = mem_allocateDMA(desc_length);
        if (usb_controlTransferInterface(intf, USB_RT_STANDARD | USB_RT_D2H, USB_REQ_GET_DESC, (USB_DESC_REPORT << 8) | report_desc_count, 0, desc_length, (void*)buffer) != USB_SUCCESS) {
            LOG(ERR, "Failed to read REPORT descriptor %d\n", report_desc_count);
            return USB_FAILURE;
        }

        report_desc_count++;

        LOG(INFO, "Got REPORT descriptor %d (size: %d)\n", report_desc_count, desc_length);

        hid_parseReportDescriptor((uint8_t*)buffer, desc_length);
        mem_freeDMA(buffer, desc_length);
    }

    // Find the target endpoint
    USBEndpoint_t *target = NULL;
    foreach(endp_node, intf->endpoint_list) {
        USBEndpoint_t *endp = (USBEndpoint_t*)endp_node->value;
        if (USB_ENDP_GET_DIRECTION(endp) != USB_ENDP_DIRECTION_IN || !USB_ENDP_IS_INTERRUPT(endp)) {
            continue;
        }

        target = endp;
        break;
    }

    if (!target) {
        LOG(ERR, "USB HID device has no valid INTERRUPT IN endpoint\n");
        return USB_FAILURE; // TODO: Fix memory leak
    }

    if (usb_configureEndpoint(intf->dev, target) != USB_SUCCESS) {
        LOG(ERR, "Error configuring endpoint\n");
        return USB_FAILURE;
    }

    // Create HID device object
    USBHidDevice_t *d = kzalloc(sizeof(USBHidDevice_t));
    d->endp = target;

    d->transfer.callback = hid_callback;
    d->transfer.parameter = (void*)intf;
    d->transfer.endp = target;
    d->transfer.data = (void*)mem_allocateDMA(PAGE_SIZE);
    d->transfer.length = PAGE_SIZE;
    // NOTE: Actual request does not matetr

    intf->d = (void*)d;

    usb_interruptTransfer(intf->dev, &d->transfer);
    return USB_SUCCESS;
}

/**
 * @brief Register and initialize HID drivers
 */
void hid_init() {
    USBDriver_t *d = usb_createDriver();
    d->name = strdup("USB HID Driver");
    d->dev_init = hid_initializeDevice;
    d->find = NULL;
    d->weak_bind = 0;
    usb_registerDriver(d);
}