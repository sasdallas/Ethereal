/**
 * @file userspace/lib/include/ethereal/widget/input.h
 * @brief Input textbox
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _WIDGET_INPUT_H
#define _WIDGET_INPUT_H

/**** INCLUDES ****/
#include <ethereal/widget/widget.h>
#include <stdbool.h>

/**** DEFINITIONS ****/

#define INPUT_TYPE_DEFAULT          0
#define INPUT_TYPE_PASSWORD         1

/**** TYPES ****/
typedef void (*input_newline_handler_t)(widget_t *, void *);

typedef struct widget_input {
    int type;
    unsigned int idx;
    char *buffer;
    char *placeholder;
    bool focused;
    bool csr_state;
    uint64_t last_csr_update;

    // on ENTER
    input_newline_handler_t nl;
    void *nl_ctx;
} widget_input_t;


/**** FUNCTIONS ****/

/**
 * @brief Create a text input box
 * @param frame The frame to create the input box on
 * @param type Type of textbox
 * @param placeholder Optional placeholder
 */
widget_t *input_create(widget_t *frame, int type, char *placeholder, size_t width, size_t height);

/**
 * @brief Register newline handler
 * @param input Input box
 * @param handler Handler
 * @param context Context
 */
void input_onNewline(widget_t *input, input_newline_handler_t handler, void *context);

#endif