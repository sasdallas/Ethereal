/**
 * @file hexahedron/drivers/usb/hid/hid.c
 * @brief USB human interface device handler
 * 
 * @warning The parsing is code is mostly recursive garbage and unsafe.
 * @warning Weird casts are used
 * @warning This barely accounts for any corrupted HID descriptors, doesn't support all bytecode, etc., etc.
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
#include <kernel/drivers/usb/hid/keyboard.h>
#include <kernel/drivers/usb/hid/mouse.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <kernel/misc/util.h>
#include <kernel/misc/mutex.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID", __VA_ARGS__)

/* Tabs for debug */
static int tabs = 0;

/* i hate C */
#pragma GCC diagnostic ignored "-Wsign-compare"
#define HID_CAST_REPORT_SIZE(s, val) ((s) ? (signed)(val) : (unsigned)(val))

/* Push/pop usage from stack */
#define HID_USAGE_PUSH(ls, u) ({ ls.usage_stack[ls.usage_stack_len] = (u); ls.usage_stack_len++; assert(ls.usage_stack_len < HID_MAX_USAGE_STACK); })
#define HID_USAGE_POP(ls) ({ assert(ls->usage_stack_len != 0); ls->usage_stack_len--; ls->usage_stack[ls->usage_stack_len]; })

/* Clear local state */
#define HID_CLEAR_LOCAL_STATE(ls) memset(&ls, 0, sizeof(USBHidLocalState_t))

/* To allow loading HID drivers from driver files, store un-drivered (?) configurations here */
static list_t *hid_collections_without_drivers = NULL;

/* HID driver list */
static list_t *hid_driver_list = NULL;

/* HID driver list mutex */
static mutex_t *hid_driver_mutex = NULL;

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
    if (local_state->usage_stack_len) {
        uint32_t idx = 0;

        // !!!: Create a temporary list, just to reverse it...
        list_t *ihatehid = list_create("a temporary list so i can reverse it");
        while (local_state->usage_stack_len) {
            uint32_t report_count = (idx + 1 < local_state->usage_stack_len+1) ? 1 : state->report_count - idx;
            uint32_t usage = HID_USAGE_POP(local_state);

            USBHidReportItem_t *item = kzalloc(sizeof(USBHidReportItem_t));
            item->flags = flags;
            item->logical_max = state->logical_maximum;
            item->logical_min = state->logical_minimum;
            item->opcode = type;
            item->phys_max = state->physical_maximum ? state->physical_maximum : state->logical_maximum;
            item->phys_min = state->physical_minimum ? state->physical_minimum : state->logical_minimum;
            item->report_count = report_count;
            item->report_id = state->report_id;
            item->report_size = state->report_size;
            item->usage_id = usage & 0xFFFF;
            item->usage_page = (usage >> 16) ? usage >> 16 : state->usage_page;
            item->usage_max = local_state->usage_maximum;
            item->usage_min = local_state->usage_minimum;
            list_append(ihatehid, item);
            idx++;
        }


        // !!!: Reverse the damned thing... don't let anyone see this code..
        node_t *n = ihatehid->tail;
        while (n) {
            list_append(collection->items, n->value);;
            n = n->prev;
        }

        list_destroy(ihatehid, false);

    } else {

        USBHidReportItem_t *item = kzalloc(sizeof(USBHidReportItem_t));
        item->flags = flags;
        item->logical_max = state->logical_maximum;
        item->logical_min = state->logical_minimum;
        item->opcode = type;
        item->phys_max = state->physical_maximum ? state->physical_maximum : state->logical_maximum;
        item->phys_min = state->physical_minimum ? state->physical_minimum : state->logical_minimum;
        item->report_count = state->report_count;
        item->report_id = state->report_id;
        item->report_size = state->report_size;
        item->usage_id = 0;
        item->usage_page = state->usage_page;
        item->usage_max = local_state->usage_maximum;
        item->usage_min = local_state->usage_minimum;
        list_append(collection->items, item);
    }
}

/**
 * @brief Process collection
 * @param dev The USB HID device
 * @param state Parser state
 * @param local_state Local parser state
 * @param p Data buffer input + output pointer
 * @returns Collection object
 */
USBHidCollection_t *hid_parseCollection(USBHidDevice_t *dev, USBHidParserState_t *state, USBHidLocalState_t *local_state_in, uint8_t **p_out) {
    uint8_t *p = *p_out;
    
    tabs++;

    USBHidCollection_t *collection = kzalloc(sizeof(USBHidCollection_t));
    collection->opcode = HID_REPORT_MAIN_COLLECTION;
    collection->items = list_create("usb hid collection items");
    collection->usage_page = state->usage_page;
    collection->dev = dev;

    USBHidLocalState_t local_state = { 0 };

    // Starting opcode is Collection
    USBHidOpcode_t opcode = (USBHidOpcode_t)*p;
    assert(opcode.opcode == HID_REPORT_MAIN_COLLECTION);
    collection->type = *(p+1);
    
    if (collection->type != HID_REPORT_COLLECTION_LOGICAL) {
        collection->usage_id = HID_USAGE_POP(local_state_in);
    } else {
        collection->usage_id = 0; // idk?
    }

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
                    USBHidCollection_t *col = hid_parseCollection(dev, state, &local_state, &p_target);
                    p = p_target;
                    list_append(collection->items, col);
                    HID_CLEAR_LOCAL_STATE(local_state);
                    continue;

                case HID_REPORT_MAIN_END_COLLECTION:
                    tabs--;
                    hid_printOpcode(state, opcode, val, report_size);
                    p += report_size + 1;
                    *p_out = p;
                    return collection;
            
                case HID_REPORT_MAIN_INPUT:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_INPUT, *(p+1));
                    HID_CLEAR_LOCAL_STATE(local_state);
                    break;

                case HID_REPORT_MAIN_OUTPUT:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_OUTPUT, *(p+1));
                    HID_CLEAR_LOCAL_STATE(local_state);
                    break;

                case HID_REPORT_MAIN_FEATURE:
                    hid_addItemToCollection(collection, state, &local_state, HID_REPORT_MAIN_FEATURE, *(p+1));
                    HID_CLEAR_LOCAL_STATE(local_state);
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
                case HID_REPORT_GLOBAL_REPORT_ID: state->report_id = HID_CAST_REPORT_SIZE(0, val); state->has_report_id = 1; break;
                case HID_REPORT_GLOBAL_REPORT_COUNT: state->report_count = HID_CAST_REPORT_SIZE(0, val); break;

                default:
                    LOG(ERR, "Unrecognized global opcode: 0x%x\n", opcode.opcode);
                    break;
            }
        } else if (opcode.desc_type == HID_REPORT_LOCAL) {
            switch (opcode.opcode) {
                case HID_REPORT_LOCAL_USAGE: HID_USAGE_PUSH(local_state, HID_CAST_REPORT_SIZE(0, val)); break;
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
        
        if (item->opcode == HID_REPORT_MAIN_COLLECTION) {
            hid_dumpCollection((USBHidCollection_t*)item, depth+1);
        } else {
            LOG(DEBUG, "DUMP: ");
            for (int i = 0; i < depth; i++) LOG(NOHEADER, "\t");
            LOG(NOHEADER, "\tItem type=%x flags=%x report_id=%d report_count=%d report_size=%d usage_id=%04x usage_page=%04x logical_min=%d logical_max=%d\n", item->opcode, item->flags, item->report_id, item->report_count, item->report_size, item->usage_id, item->usage_page, item->logical_min, item->logical_max);
        }
    }
} 

/**
 * @brief Process report descriptor
 * @param device The USB HID device
 * @param data Report data
 * @param size Size of report data
 * @param uses_report_id Output boolean for if report ID is used
 * @returns A list of HID collections
 */
list_t *hid_parseReportDescriptor(USBHidDevice_t *device, uint8_t *data, size_t size, uint8_t *uses_report_id) {
    uint8_t *p = data;

    HEXDUMP(p, size);

    USBHidParserState_t *state = kzalloc(sizeof(USBHidParserState_t));
    list_t *collections = list_create("usb hid collections");

    USBHidLocalState_t local_state = { 0 };

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
                USBHidCollection_t *collection = hid_parseCollection(device, state, &local_state, &p_target);
                list_append(collections, collection);
                p = p_target;
                HID_CLEAR_LOCAL_STATE(local_state);
                continue;
            } else {
                LOG(WARN, "HID parser encountered an unexpected MAIN opcode at this time: 0x%x\n", opcode.opcode);
                printf("HID bad main opcode 0x%x\n", opcode.opcode);
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
                case HID_REPORT_GLOBAL_REPORT_ID: state->report_id = HID_CAST_REPORT_SIZE(0, val); state->has_report_id = 1; break;
                case HID_REPORT_GLOBAL_REPORT_COUNT: state->report_count = HID_CAST_REPORT_SIZE(0, val); break;

                case HID_REPORT_GLOBAL_PUSH: assert(0); // TODO
                case HID_REPORT_GLOBAL_POP: assert(0); // TODO

                default:
                    LOG(ERR, "Unrecognized global opcode: 0x%x\n", opcode.opcode);
                    printf("HID bad global opcode 0x%x\n", opcode.opcode);
                    break;
            }
        } else if (opcode.desc_type == HID_REPORT_LOCAL) {
            switch (opcode.opcode) {
                case HID_REPORT_LOCAL_USAGE: HID_USAGE_PUSH(local_state, HID_CAST_REPORT_SIZE(0, val)); break;
                case HID_REPORT_LOCAL_USAGE_MAXIMUM: local_state.usage_maximum = HID_CAST_REPORT_SIZE(0, val); break;
                case HID_REPORT_LOCAL_USAGE_MINIMUM: local_state.usage_minimum = HID_CAST_REPORT_SIZE(0, val); break;
            
                default:
                    printf("HID bad/unknown local opcode 0x%x\n", opcode.opcode);
                // TODO: Use the rest?
            }
        }
    
        p += report_size + 1;
    } 

    foreach(collection, collections) {
        hid_dumpCollection((USBHidCollection_t*)collection->value, 0);
    } 

    *uses_report_id = state->has_report_id;
    kfree(state);

    return collections;
}

/**
 * @brief Extract item report value
 * @param item The item to extract the report value from
 * @param data The data pointer to extract from
 * @param offset Current bit offset
 * @param total_size The total size
 */
int64_t hid_extractItemReportValue(USBHidReportItem_t *item, uint8_t *data, size_t bit_offset, size_t total_size) {
    if (bit_offset > total_size) return 0;

    size_t bit_count = item->report_size;
    if (bit_offset + bit_count > total_size) bit_count = total_size - bit_offset;

    // Extract the bits from the data
    uint64_t result = 0;
    for (size_t i = 0; i < bit_count; i++) {
        size_t byte_offset = (bit_offset + i) / 8;
        size_t bit_in_byte = (bit_offset + i) % 8;
        if (data[byte_offset] & (1 << bit_in_byte)) {
            result |= (1ULL << i);
        }
    }

    // Sign-extend if the value is signed and the most significant bit is set
    if (item->logical_min < 0 && (result & (1ULL << (bit_count - 1)))) {
        result |= ~((1ULL << bit_count) - 1);
    }

    return result;
}

/**
 * @brief Process an HID collection during a callback
 * @param collection The collection to process
 * @param report_id Report ID (0x0 = reserved)
 * @param data_ptr The data pointer
 * @param offset In and out pointer for current bit offset
 * @param data_size Total amount of bits in the data
 */
void hid_processCollectionData(USBHidCollection_t *collection, uint8_t report_id, uint8_t *data_ptr, size_t *offset, size_t data_size) {
    size_t bit_offset = *offset;

    // Begin report
    if (collection->driver && collection->driver->begin) collection->driver->begin(collection);

    foreach(node, collection->items) {
        USBHidReportItem_t *item = (USBHidReportItem_t*)node->value;

        if (item->opcode == HID_REPORT_MAIN_COLLECTION) {
            hid_processCollectionData((USBHidCollection_t*)item, report_id, data_ptr, &bit_offset, data_size);
        } else if (item->opcode == HID_REPORT_MAIN_INPUT) {
            if (report_id && report_id != item->report_id) continue;
            uint32_t usage_id = item->usage_id ? item->usage_id : item->usage_min; 

            if (!collection->driver || !(item->usage_id || item->usage_max || item->usage_min)) {
                // We have no reason to process this data
                bit_offset += item->report_size * item->report_count;
                continue;
            }

            for (unsigned i = 0; i < item->report_count; i++) {

                // Extract the value
                int64_t logical_val = hid_extractItemReportValue(item, data_ptr, bit_offset, data_size);

                // Check range
                if (!IN_RANGE(logical_val, item->logical_min, item->logical_max)) {
                    bit_offset += item->report_size;
                    continue;
                }

                if (!(item->flags & HID_INPUT_FLAG_VARIABLE)) {
                    // Array
                    // LOG(DEBUG, "Sending array data: %d\n", usage_id + logical_val);
                    if (collection->driver && collection->driver->array) collection->driver->array(collection, item, item->usage_page, usage_id + logical_val);
                } else {
                    // Determine physical
                    int64_t physical_val = (item->phys_max - item->phys_min) * (logical_val - item->logical_min) / (item->logical_max - item->logical_min) + item->phys_min;

                    if (item->flags & HID_INPUT_FLAG_RELATIVE) {
                        // Relative variable
                        if (collection->driver && collection->driver->relative) collection->driver->relative(collection, item, item->usage_page, usage_id + i, physical_val);
                    } else {
                        // Absolute variable
                        if (collection->driver && collection->driver->absolute) collection->driver->absolute(collection, item, item->usage_page, usage_id + i, physical_val); 
                    }
                }

                bit_offset += item->report_size;
            }
        }
    }


    if (collection->driver && collection->driver->finish) collection->driver->finish(collection);
    *offset = bit_offset;
}

/**
 * @brief USB transfer callback
 * @param endp The endpoint the transfer was completed on
 * @param complete Completion structure
 */
void hid_callback(USBEndpoint_t *endp, USBTransferCompletion_t *complete) {
    if (complete->transfer->status != USB_TRANSFER_SUCCESS) return;
    // LOG(DEBUG, "Transfer complete on endpoint %d, received %d bytes\n", USB_ENDP_GET_NUMBER(endp), complete->length);

    USBInterface_t *intf = (USBInterface_t*)complete->transfer->parameter;
    USBHidDevice_t *hid = (USBHidDevice_t*)intf->d;
    if (hid->in_endp != endp || !complete->length) return; // ???

    // Depending on whether this device uses the report ID, we can accept it.
    uint8_t *data_ptr = (uint8_t*)complete->transfer->data;
    uint8_t report_id = 0x0; // Reserved ID
    if (hid->uses_report_id) {
        report_id = *data_ptr;
        data_ptr++;
        assert(report_id); //
    }

    HEXDUMP(data_ptr, (report_id) ? (complete->transfer->length-1) : complete->transfer->length);

    size_t current_offset = 0;
    foreach (collection_node, hid->collections) {
        USBHidCollection_t *col = (USBHidCollection_t*)collection_node->value;
        
        // Look through each input
        if (col->opcode == HID_REPORT_MAIN_COLLECTION) {
            LOG(DEBUG, "Processing data for collection type=%d\n", col->type);
            hid_processCollectionData(col, report_id, data_ptr, &current_offset, (report_id) ? (complete->length-1)*8 : complete->length * 8);
        }  
    } 

    usb_interruptTransfer(intf->dev, &hid->transfer);
}


/**
 * @brief Initialize USB device
 * @param intf The interface to initialize on
 */
USB_STATUS hid_initializeDevice(USBInterface_t *intf) {
    if (!hid_collections_without_drivers) hid_collections_without_drivers = list_create("hid configurations that are driverless");

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
        LOG(DEBUG, "Disabling boot protocol and switching to report protocol\n");
        if (usb_requestDevice(intf->dev, USB_RT_H2D | USB_RT_CLASS | USB_RT_INTF, HID_REQ_SET_PROTOCOL, 1, intf->desc.bInterfaceNumber, 0, NULL) != USB_TRANSFER_SUCCESS) {
            LOG(ERR, "HID_REQ_SET_PROTOCOL (Report) failed\n");
            return USB_FAILURE;
        } 
    }

    LOG(INFO, "HID version %d.%2d (country code: %d)\n", hid_desc->bcdHid >> 8, hid_desc->bcdHid & 0xFF, hid_desc->bCountryCode);

    int report_desc_count = 0;
    uint8_t uses_report_id = 0;


    // Create HID device object
    USBHidDevice_t *d = kzalloc(sizeof(USBHidDevice_t));
    d->collections = list_create("hid collections");

    // Locate and parse REPORT descriptors
    for (size_t i = 0; i < hid_desc->bNumDescriptors; i++) {
        LOG(DEBUG, "Located HID descriptor: %d\n", hid_desc->desc[i].bDescriptorType);
        if (hid_desc->desc[i].bDescriptorType != USB_DESC_REPORT) continue;

        size_t desc_length = hid_desc->desc[i].wItemLength;

        // Report descriptor located
        uintptr_t buffer = dma_map(desc_length);
        if (usb_controlTransferInterface(intf, USB_RT_STANDARD | USB_RT_D2H, USB_REQ_GET_DESC, (USB_DESC_REPORT << 8) | report_desc_count, 0, desc_length, (void*)buffer) != USB_SUCCESS) {
            LOG(ERR, "Failed to read REPORT descriptor %d\n", report_desc_count);
            return USB_FAILURE;
        }

        report_desc_count++;

        LOG(INFO, "Got REPORT descriptor %d (size: %d)\n", report_desc_count, desc_length);

        list_t *l = hid_parseReportDescriptor(d, (uint8_t*)buffer, desc_length, &uses_report_id);
        dma_unmap(buffer, desc_length);
    
        foreach(node, l) {
            USBHidCollection_t *col = (USBHidCollection_t*)node->value;

            if (col->opcode != HID_REPORT_MAIN_COLLECTION) {
                LOG(ERR, "Unknown item outside of collection (opcode=%d)\n", col->opcode);
                continue;
            }

            list_append(d->collections, col);
        }

        list_destroy(l, false);
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

    // Configuire it
    if (usb_configureEndpoint(intf->dev, target) != USB_SUCCESS) {
        LOG(ERR, "Error configuring endpoint\n");
        return USB_FAILURE;
    }

    // Setup HID device object
    d->intf = intf;
    d->in_endp = target;
    d->uses_report_id = uses_report_id;

    d->transfer.callback = hid_callback;
    d->transfer.parameter = (void*)intf;
    d->transfer.endp = target;
    d->transfer.data = (void*)dma_map(d->in_endp->desc.wMaxPacketSize & 0x7FF);
    d->transfer.length = d->in_endp->desc.wMaxPacketSize & 0x7FF;
    // NOTE: Actual request does not matetr

    intf->d = (void*)d;

    // Locate driver
    if (hid_driver_list) mutex_acquire(hid_driver_mutex);
    
    foreach(cn, d->collections) {
        USBHidCollection_t *collection = (USBHidCollection_t*)cn->value;

        if (hid_driver_list) {
            foreach(driver_node, hid_driver_list) {
                USBHidDeviceDriver_t *driver = (USBHidDeviceDriver_t*)driver_node->value;
                if (driver->usage_page && driver->usage_page != collection->usage_page) continue;
                if (driver->usage_id && driver->usage_id != collection->usage_id) continue;

                // Determined valid driver. Test init
                USB_STATUS success = driver->init(collection);
                if (success != USB_SUCCESS) continue;

                collection->driver = driver;
                
                // Parent all subcollections with this driver
                foreach(subcol, collection->items) {
                    USBHidCollection_t *child_collection = (USBHidCollection_t*)subcol->value;
                    if (child_collection->opcode == HID_REPORT_MAIN_COLLECTION) {
                        child_collection->driver = driver;
                        child_collection->d = collection->d;
                    }
                }

                break;
            }
        }

        if (!collection->driver) {
            list_append(hid_collections_without_drivers, collection);
        }
    }

    if (hid_driver_list) mutex_release(hid_driver_mutex);
    
    // Begin transfer
    usb_interruptTransfer(intf->dev, &d->transfer);
    return USB_SUCCESS;
}

/**
 * @brief Register and initialize HID drivers
 */
void hid_init() {
    // Initialize builtin HID drivers
    usb_keyboardInit();
    usb_mouseInit();

    // Register USB driver
    USBDriver_t *d = usb_createDriver();
    d->name = strdup("USB HID Driver");
    d->dev_init = hid_initializeDevice;
    d->find = NULL;
    d->weak_bind = 0;
    usb_registerDriver(d);
}

/**
 * @brief Register an HID driver
 * @param driver The driver to register
 */
void hid_registerDriver(USBHidDeviceDriver_t *driver) {
    if (!hid_driver_list) {
        hid_driver_list = list_create("hid driver list");
        hid_driver_mutex = mutex_create("hid driver list mutex");
    }
    
    list_append(hid_driver_list, driver);

    if (hid_collections_without_drivers) {
        mutex_acquire(hid_driver_mutex);
        node_t *cn = hid_collections_without_drivers->head;
        while (cn) {
            USBHidCollection_t *col = (USBHidCollection_t*)cn->value;

            if (driver->usage_page && driver->usage_page != col->usage_page) continue;
            if (driver->usage_id && driver->usage_id != col->usage_id) continue;

            // Determined valid driver. Test init
            USB_STATUS success = driver->init(col);
            if (success != USB_SUCCESS) continue;

            col->driver = driver;
            
            node_t *old = cn;
            cn = cn->next;
            list_delete(hid_collections_without_drivers, old);
        }
        mutex_release(hid_driver_mutex);
    }
}