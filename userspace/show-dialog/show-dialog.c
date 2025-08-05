/**
 * @file userspace/show-dialog/show-dialog.c
 * @brief Show message box (and other dialogues)
 * 
 * Basically a zenity clone
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/celestial.h>
#include <ethereal/widget.h>
#include <stdio.h>
#include <getopt.h>

#define DIALOG_TYPE_NONE            0
#define DIALOG_TYPE_INFO            1
#define DIALOG_TYPE_ERROR           2

/* Dialog type */
int dialog_type = DIALOG_TYPE_NONE;

/* Text to use in the dialog */
char *dialog_text = "You forgot --text lmao";

/* Dialog title */
char *dialog_title = NULL;

/* Font */
gfx_font_t *font = NULL;

/**
 * @brief Usage function
 */
void usage() {
    fprintf(stderr, "Usage: show-dialog [OPTIONS]\n");
    fprintf(stderr, "Show a dialog box of your choosing\n");
    fprintf(stderr, " --info                Display an info dialog\n");
    fprintf(stderr, " --error               Display an error dialog\n");
    fprintf(stderr, " --text=TEXT           Set the text of the dialog\n");
    fprintf(stderr, " --title=TITLE         Set the title of the dialog\n");
    fprintf(stderr, " --help                Show this help message\n");
    fprintf(stderr, " --version             Print the version of show-dialog\n");

    exit(1);
}

/**
 * @brief Version function
 */
void version() {
    printf("show-dialog version 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

/**
 * @brief Set title from dialog type
 */
void set_title(window_t *win) {
    if (dialog_title) {
        celestial_setTitle(win, dialog_title);
        return;
    }

    switch (dialog_type) {
        case DIALOG_TYPE_INFO:
            return celestial_setTitle(win, "Information");
        case DIALOG_TYPE_ERROR:
            return celestial_setTitle(win, "Error");
    }
}

void btn_callback(widget_t *w, void *d) {
    celestial_closeWindow((window_t*)d);
}


int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "info", .flag = NULL, .has_arg = no_argument, .val = 'i', },
        { .name = "error", .flag = NULL, .has_arg = no_argument, .val = 'e', },
        { .name = "text", .flag = NULL, .has_arg = required_argument, .val = 't' },
        { .name = "title", .flag = NULL, .has_arg = required_argument, .val = 'T' },
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'v'},
    };

    int ch;
    int index;
    while ((ch = getopt_long(argc, argv, "", (const struct option*)options, &index)) != -1) {
        if (!ch) {
            ch = options[index].val;
        }
        
        switch (ch) {
            case 'i':
                if (dialog_type) { fprintf(stderr, "show-dialog: error: Two or more dialog types specified.\n"); exit(1); }
                dialog_type = DIALOG_TYPE_INFO;
                break;

            case 'e':
                if (dialog_type) { fprintf(stderr, "show-dialog: error: Two or more dialog types specified.\n"); exit(1); }
                dialog_type = DIALOG_TYPE_ERROR;
                break;

            case 't':
                dialog_text = strdup(optarg);
                break;

            case 'T':
                dialog_title = strdup(optarg);
                break;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    if (dialog_type == DIALOG_TYPE_NONE) {
        usage();
    }

    // Load font
    font = gfx_loadFont(NULL, "/usr/share/DejaVuSans.ttf");
    gfx_setFontSize(font, 13);
    gfx_string_size_t s;
    gfx_getStringSize(font, dialog_text, &s);
    if (s.width < 100) s.width = 100;
    gfx_destroyFont(font);

    // Prepare to create the window
    wid_t wid = celestial_createWindow(0, s.width + 80, 125);
    window_t *win = celestial_getWindow(wid);
    set_title(win);
    gfx_context_t *ctx = celestial_getGraphicsContext(win);
    
    widget_t *root = frame_createRoot(win);

    // Start making buttons
    widget_t *btn = button_create(root, "OK", GFX_RGB(0, 0, 0), BUTTON_ENABLED);    
    widget_renderAtCoordinates(btn, GFX_WIDTH(ctx) - btn->width - 18, GFX_HEIGHT(ctx) - btn->height - 18);
    widget_setHandler(btn, WIDGET_EVENT_CLICK, btn_callback, (void*)win);

    widget_t *lbl = label_create(root, dialog_text, 13);
    widget_renderAtCoordinates(lbl, 10, 43);

    // Render
    widget_render(ctx, root);
    gfx_render(ctx);
    celestial_flip(win);

    celestial_mainLoop();
    return 0;
}