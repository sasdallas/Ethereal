/**
 * @file hexahedron/drivers/usb2/hid_parser.c
 * @brief HID bytecode parser
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/usb2/usb.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "USB:HID:PARSER", __VA_ARGS__)

/* HID too far */
#define HID_WILL_BE_PAST_END(state,ptr,i) (((uintptr_t)(ptr)+(i)) > ((uintptr_t)(state)->end))
#define HID_PAST_END(state,ptr) (((uintptr_t)(ptr)) > ((uintptr_t)(state)->end))

/* Push/pop from usage stack */
#define HID_PUSH_USAGE(state, usage) ({ assert((state)->usage_stack_len < 32); (state)->usage_stack[(state)->usage_stack_len++] = (usage); })
#define HID_POP_USAGE(state) ({ assert((state)->usage_stack_len); (state)->usage_stack_len--; (state)->usage_stack[(state)->usage_stack_len]; })

/**
 * @brief Consume HID opcode
 * 
 * Outputs sign-extended value (signed_val), regular value (val), report_size.
 */
static bool hid_getNextItem(hid_parser_state_t *state, hid_parser_item_t *next) {
    if (HID_PAST_END(state, state->data)) {

    }
    
    hid_item_t item = (hid_item_t)(*state->data);
    next->item = item;

    switch (HID_ITEM_SIZE(item)) {
        case 1: next->report_size = 1; break;
        case 2: next->report_size = 2; break;
        case 3: next->report_size = 4; break;
        default: next->report_size = 0; break;
    }

    if (HID_WILL_BE_PAST_END(state, state->data, next->report_size + 1)) {
        LOG(ERR, "Item would extend to end of descriptor\n");
        return false;
    }

    state->data += 1;

    next->unsigned_val = 0;
    switch (next->report_size) {
        case 1: next->unsigned_val = (uint32_t)(*(uint8_t*)(state->data)); break;
        case 2: next->unsigned_val = (uint32_t)(*(uint16_t*)(state->data)); break;
        case 4: next->unsigned_val = (uint32_t)(*(uint32_t*)(state->data)); break;
    }

    next->signed_val = 0;
    switch (next->report_size) {
        case 1: next->signed_val = (int32_t)(int8_t)next->unsigned_val; break;
        case 2: next->signed_val = (int32_t)(int16_t)next->unsigned_val; break;
        case 4: next->signed_val = (int32_t)next->unsigned_val; break;
    }

    state->data += next->report_size;
    return true;
}

/**
 * @brief Make item
 */
static void hid_makeField(hid_report_t *report, hid_field_t *field, hid_parser_state_t *state, hid_parser_item_t *item) {
    memset(field, 0, sizeof(hid_field_t));

    // convert item flags to field flags
    field->flags = item->unsigned_val; // HID_EVENT_FLAG holds the same stuff

    field->logical_min = state->logical_minimum;
    field->logical_max = state->logical_maximum;
    field->physical_min = state->physical_minimum;
    field->physical_max = state->physical_maximum;
    field->report_count = state->report_count;
    field->report_id = state->report_id;
    field->report_size = state->report_size;
    field->unit = state->unit;
    field->unit_exponent = state->unit_exponent;
    field->type = report->field_type;
    field->usage_page = state->usage_page;

    if ((field->flags & HID_EVENT_FLAG_VARIABLE) == 0) {
        // HID array values can be less than a byte long so one byte per thing is necessary
        field->current_state = kzalloc(field->report_count * sizeof(int32_t));
        field->last_state = kzalloc(field->report_count * sizeof(int32_t));
    }

    field->parent = state->collection;
    field->app = state->app_collection;

    size_t usages = min(state->usage_stack_len, field->report_count);

    // usage_stack_len gets reset after anyways
    for (unsigned i = 0; i < usages; i++) {
        field->usages[i] = state->usage_stack[i];
    }

    if (state->has_usage_range) {
        field->usage_min = state->usage_minimum;
        field->usage_max = state->usage_maximum;
        field->usage_range = true;
    }

    field->num_usage = usages;

    field->bit_offset = report->bit_length;
    report->bit_length += field->report_size * field->report_count;
}

/**
 * @brief Add an item to the collection
 */
static usb_status_t hid_addItem(hid_device_t *device, hid_parser_state_t *state, hid_parser_item_t *item) {
    // locate the field
    hid_field_type_t t;
    hid_report_t **list;
    switch (HID_ITEM_TAG(item->item)) {
        case HID_REPORT_MAIN_INPUT: t = HID_FIELD_INPUT; list = device->input_reports; break;
        case HID_REPORT_MAIN_OUTPUT: t = HID_FIELD_OUTPUT; list = device->output_reports; break;
        case HID_REPORT_MAIN_FEATURE: t = HID_FIELD_FEATURE; list = device->feature_reports; break;
        default: assert(0);
    }    

    hid_report_t *r = list[state->report_id];

    if (!r) {
        // Create a new report
        r = kmalloc(sizeof(hid_report_t));
        r->num_fields = 0;
        r->fields = NULL;
        r->field_type = t;
        r->report_id = state->report_id;
        r->bit_length = 0;

        list[state->report_id] = r;
    }

    size_t this_field = r->num_fields++;
    r->fields = krealloc(r->fields, sizeof(hid_field_t) * r->num_fields);

    hid_makeField(r, &r->fields[this_field], state, item);
    return USB_SUCCESS;
}

/**
 * @brief HID parse collection
 */
static usb_status_t hid_parseItems(hid_device_t *device, hid_parser_state_t *state, bool is_collection) {
    hid_collection_t *collection = state->collection;

    while (true) {
        hid_parser_item_t item;
        
        if (hid_getNextItem(state, &item) == false) {
            break;
        }

        // LOG(DEBUG, "HID item: report_size=%d val=%08x type=%d tag=%d\n", item.report_size, item.unsigned_val, HID_ITEM_TYPE(item.item), HID_ITEM_TAG(item.item));
        
        // Parse the item using this big parser loop
        uint8_t tag = HID_ITEM_TAG(item.item);
        uint8_t type = HID_ITEM_TYPE(item.item);
        if (type == HID_REPORT_MAIN) {
            if (tag == HID_REPORT_MAIN_COLLECTION) {
                // Create a new collection
                hid_collection_t *new = kzalloc(sizeof(hid_collection_t));
                new->type = item.unsigned_val;
                new->usage_page = state->usage_page;
                new->device = device;

                if (item.unsigned_val == HID_COLLECTION_TYPE_LOGICAL) {
                    new->usage_id = 0;
                } else {
                    new->usage_id = HID_POP_USAGE(state);
                }

                STAILQ_INSERT_TAIL(&device->collections, new, node);

                // Save app collection before parsing
                hid_collection_t *app = state->app_collection;

                // Apply it to state temporarily
                state->collection = new;

                if (new->type == HID_COLLECTION_TYPE_APPLICATION) {
                    state->app_collection = new;
                }

                // Parse it
                LOG(INFO, "Collection type=%02x usage_id=%08x usage_page=%04x\n", new->type, new->usage_id, new->usage_page);
                usb_status_t status = hid_parseItems(device, state, true);
                if (USB_ERROR(status)) {
                    LOG(ERR, "Collection parse failed: %s\n", usb_strerror(status));
                    return status;
                }

                state->collection = collection;
                state->app_collection = app;

                // reset state
                state->usage_stack_len = 0;
                state->has_usage_range = false;
            } else if (tag == HID_REPORT_MAIN_END_COLLECTION) {
                assert(is_collection);
                return USB_SUCCESS;
            } else if (tag == HID_REPORT_MAIN_INPUT || tag == HID_REPORT_MAIN_OUTPUT || tag == HID_REPORT_MAIN_FEATURE) {
                assert(is_collection);
                hid_addItem(device, state, &item);

                // reset state
                state->usage_stack_len = 0;
                state->has_usage_range = false;
            } else {
                LOG(WARN, "Encountered unknown MAIN item with tag=%02x report_size=%02x\n", HID_ITEM_TAG(item.item), item.report_size);
            }
        } else if (type == HID_REPORT_GLOBAL) {
            switch (tag) {
                case HID_REPORT_GLOBAL_USAGE_PAGE:
                    state->usage_page = item.unsigned_val;
                    break;

                case HID_REPORT_GLOBAL_UNIT:
                    state->unit = item.unsigned_val;
                    break;

                case HID_REPORT_GLOBAL_LOGICAL_MAXIMUM:
                    if (state->logical_minimum < 0) {
                        state->logical_maximum = item.signed_val;
                    } else {
                        state->logical_maximum = item.unsigned_val;
                    }
                    
                    break;

                case HID_REPORT_GLOBAL_LOGICAL_MINIMUM:
                    state->logical_minimum = item.signed_val;
                    break;

                case HID_REPORT_GLOBAL_PHYSICAL_MAXIMUM:
                    if (state->physical_minimum < 0) {
                        state->physical_maximum = item.signed_val;
                    } else {
                        state->physical_maximum = item.unsigned_val;
                    }

                    break;

                case HID_REPORT_GLOBAL_PHYSICAL_MINIMUM:
                    state->physical_minimum = item.signed_val;
                    break;

                case HID_REPORT_GLOBAL_UNIT_EXPONENT:
                    state->unit_exponent = item.unsigned_val;
                    break;

                case HID_REPORT_GLOBAL_REPORT_SIZE:
                    state->report_size = item.unsigned_val;
                    break;

                case HID_REPORT_GLOBAL_REPORT_ID:
                    state->report_id = item.unsigned_val;
                    state->has_report_id = true;
                    break;

                case HID_REPORT_GLOBAL_REPORT_COUNT:
                    state->report_count = item.unsigned_val;
                    break;
            }

        } else if (type == HID_REPORT_LOCAL) {
            switch (tag) {
                case HID_REPORT_LOCAL_USAGE:
                    HID_PUSH_USAGE(state, item.unsigned_val);
                    break;

                case HID_REPORT_LOCAL_USAGE_MAXIMUM:
                    state->has_usage_range = true;
                    state->usage_maximum = item.unsigned_val;
                    break;

                case HID_REPORT_LOCAL_USAGE_MINIMUM:
                    state->has_usage_range = true;
                    state->usage_minimum = item.unsigned_val;
                    break;
            }
        }
    }

    if (state->has_report_id) {
        device->uses_report_id = true;
    }

    return USB_SUCCESS;
}

/**
 * @brief Parse report descriptor
 */
static usb_status_t hid_parseReportDescriptor(hid_device_t *device, uint8_t *desc, size_t desc_length) {
    LOG(DEBUG, "Parsing report descriptor (%d bytes long)\n", desc_length);
    
    HEXDUMP(desc, desc_length);
    // Initialize HID parser state
    hid_parser_state_t state = {
        .data = desc,
        .end = desc + desc_length,
    };

    return hid_parseItems(device, &state, false);
}

/**
 * @brief Dump report
 */
static void hid_dumpReport(hid_report_t *report) {
    LOG(INFO, "\tReport id=%d field_type=%d bit_length=%d\n", report->report_id, report->field_type, report->bit_length);

    for (unsigned i = 0; i < report->num_fields; i++) {
        hid_field_t *item = &report->fields[i];
        LOG(INFO, "\t\tField type=0x%x flags=0x%x report_id=%d report_count=%d report_size=%d usage_page=0x%04x logical_min=%d logical_max=%d\n", item->type, item->flags, item->report_id, item->report_count, item->report_size, item->usage_page, item->logical_min, item->logical_max);
    }
}

/**
 * @brief Parse the bytecode for a HID device
 * @param device The device to parse the bytecode for
 * @param desc The HID descriptor
 */
usb_status_t hid_parse(hid_device_t *device, usb_hid_desc_t *desc) {
    LOG(INFO, "Parsing %d descriptors for HID device.\n", desc->bNumDescriptors);
    assert(desc->bNumDescriptors * sizeof(usb_hid_optional_desc_t) < desc->bLength);

    int report_desc_count = 0;
    for (unsigned i = 0; i < desc->bNumDescriptors; i++) {
        usb_hid_optional_desc_t *opt = &desc->desc[i];
        if (opt->bDescriptorType != HID_DESC_REPORT) continue;
    
        uint8_t *report_desc = kmalloc(opt->wItemLength);
        size_t desc_length = opt->wItemLength;

        // read the descriptor
        // TODO add API for this
        usb_device_request_t req = {
            .bmRequestType = USB_RT_INTF | USB_RT_STANDARD | USB_RT_D2H,
            .bRequest = USB_REQ_GET_DESC,
            .wValue = (HID_DESC_REPORT << 8) | report_desc_count,
            .wIndex = device->intf->desc.bInterfaceNumber,
            .wLength = desc_length
        };

        usb_status_t status = usb_request(device->device, &req, report_desc);
        if (USB_ERROR(status)) {
            LOG(ERR, "Failed to get REPORT descriptor %d: %s\n", report_desc_count, usb_strerror(status));
            kfree(report_desc);
            return status;
        }

        status = hid_parseReportDescriptor(device, report_desc, desc_length);
        if (USB_ERROR(status)) {
            LOG(ERR, "Parse report descriptor failed: %s\n", usb_strerror(status));
            kfree(report_desc);
            return status;
        }

        kfree(report_desc);

        // next descriptor
        report_desc_count += 1;
    }

    // DEBUG: Dump all collection data
    hid_collection_t *collection = STAILQ_FIRST(&device->collections);
    while (collection) {
        LOG(INFO, "Collection usage_id=0x%08x usage_page=0x%04x type=%d\n", collection->usage_id, collection->usage_page, collection->type);
        collection = STAILQ_NEXT(collection, node);
    }

    // DEBUG: Dump all input data
    for (unsigned i = 0; i < 256; i++) {
        if (device->input_reports[i] != NULL) {
            hid_dumpReport(device->input_reports[i]);
        }

        if (device->output_reports[i] != NULL) {
            hid_dumpReport(device->output_reports[i]);
        }

        if (device->feature_reports[i] != NULL) {
            hid_dumpReport(device->feature_reports[i]);
        }
    }    

    LOG(INFO, "Successfully parsed %d report descriptors\n", report_desc_count);

    return USB_SUCCESS;
}

/**
 * @brief Extract item report value
 */
static bool hid_extractValue(hid_field_t *field, uint8_t *data, size_t bit_offset, size_t total_size, int64_t *val_out) {
    size_t bit_count = field->report_size;
    total_size = total_size * 8;

    if (bit_offset > total_size) return false;
    if (bit_offset + bit_count > total_size) bit_count = total_size - bit_offset;


    assert(bit_count < 64);

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
    if (field->logical_min < 0 && (result & (1ULL << (bit_count - 1)))) {
        result |= ~((1ULL << bit_count) - 1);
    }

    *val_out = result;
    return true;
}

/**
 * @brief Check whether value did change or not in array
 */
static bool hid_arrayContains(int32_t *array, int32_t usage, size_t max) {
    for (unsigned i = 0; i < max; i++) {
        if (array[i] == usage) return true;
    }

    return false;
}

/**
 * @brief Parse report for HID device
 * @param device The device to parse the report for
 * @param report The report buffer
 * @param report_size The size of the report processed
 */
void hid_process(hid_device_t *device, void *reportbuf, size_t report_size) {
    uint8_t *buffer = (uint8_t*)reportbuf;
    uint8_t report_id = 0;

    HEXDUMP(reportbuf, report_size);

    if (device->uses_report_id) {
        if (report_size == 0) {
            LOG(ERR, "Malformed empty HID report\n");
            return;
        }

        report_id = *buffer++;
        report_size--;

        if (report_id == 0) {
            LOG(ERR, "Malformed HID report with report ID 0\n");
            return;
        }
    }

    hid_report_t *report = device->input_reports[report_id];
    if (report == NULL) {
        LOG(WARN, "No report to handle report ID 0x%x\n", report_id);
        return;
    }

    size_t expected_size = (report->bit_length + 7) / 8;
    if (report_size < expected_size) {
        LOG(ERR, "Malformed report on report 0x%x, got %zu bytes but expected %zu\n",
            report->report_id, report_size, expected_size);
        return;
    }

    // For each field
    for (unsigned i = 0; i < report->num_fields; i++) {
        hid_field_t *f = &report->fields[i];

        if (f->flags & HID_EVENT_FLAG_CONSTANT) {
            // Constant events arent sent to the driver
            continue;
        }

        hid_collection_t *col = f->app;
        if (col->driver == NULL) {
            continue;
        }

        size_t bit_offset = f->bit_offset;

        for (unsigned j = 0; j < f->report_count; j++) {
            int64_t value;
            if (hid_extractValue(f, buffer, bit_offset, report_size, &value) == false) {
                continue;
            }

            if (!IN_RANGE(value, f->logical_min, f->logical_max)) {
                continue;
            }

            uint32_t usage;

            if (f->flags & HID_EVENT_FLAG_VARIABLE) {
                usage = f->usage_range ? f->usage_min + j : f->usages[min(j,f->num_usage-1)];

                // Recompute the logical value to physical (this is a monstrocity)
                if (f->physical_max || f->physical_min) {
                    value = (f->physical_max - f->physical_min) * (value - f->logical_min) / (f->logical_max - f->logical_min) + f->physical_min;
                }

                // Send the event here for variable
                hid_event_t e = {
                    .flags = f->flags,
                    .field = f,
                    .device = device,
                    .report_id = report_id,
                    .usage = usage,
                    .usage_page = f->usage_page,
                    .value = value,
                };

                col->touched = true;
                col->driver->ops.event(col, &e);
            } else {
                // Array values select a usage
                if (f->usage_range) {
                    usage = f->usage_min + (value - f->logical_min);
                } else {
                    size_t usage_index = value - f->logical_min;

                    if (usage_index >= f->num_usage) {
                        continue;
                    }

                    usage = f->usages[usage_index];
                }

                f->current_state[j] = usage;
            }

            bit_offset += f->report_size;
        }

        if ((f->flags & HID_EVENT_FLAG_VARIABLE) == 0) {
            // This is an array
            for (unsigned i = 0; i < f->report_count; i++) {
                int32_t usage = f->current_state[i]; 
                if (!usage) continue;

                if (hid_arrayContains(f->last_state, usage, f->report_count) == false) {
                    hid_event_t e = {
                        .flags = f->flags,
                        .field = f,
                        .device = device,
                        .report_id = report_id,
                        .usage = usage,
                        .usage_page = f->usage_page,
                        .value = 1,
                    };

                    col->touched = true;
                    col->driver->ops.event(col, &e);
                }
            }

            // Repeat for releases
            for (unsigned i = 0; i < f->report_count; i++) {
                int32_t usage = f->last_state[i]; 
                if (!usage) continue;

                if (hid_arrayContains(f->current_state, usage, f->report_count) == false) {
                    hid_event_t e = {
                        .flags = f->flags,
                        .field = f,
                        .device = device,
                        .report_id = report_id,
                        .usage = usage,
                        .usage_page = f->usage_page,
                        .value = 0,
                    };

                    col->touched = true;
                    col->driver->ops.event(col, &e);
                }
            }

            memcpy(f->last_state, f->current_state, f->report_count * sizeof(int32_t));
            memset(f->current_state, 0, f->report_count * sizeof(int32_t));
        }
    }

    hid_collection_t *iter = STAILQ_FIRST(&device->collections);
    while (iter) {
        if (iter->touched && iter->driver->ops.end) {
            iter->touched = false;
            iter->driver->ops.end(iter);
        }

        iter = STAILQ_NEXT(iter, node);
    }
}
