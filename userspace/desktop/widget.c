/**
 * @file userspace/desktop/widget.c
 * @brief Desktop widget loader
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/desktop.h>
#include <dlfcn.h>
#include <stdio.h>
#include <structs/list.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <graphics/gfx.h>
#include <ethereal/celestial.h>

list_t *widgets_list = NULL;
desktop_tray_widget_t *highlighted = NULL;
desktop_tray_widget_t *active = NULL;

extern window_t *taskbar_window;

/* Current bounding */
int current_x = -1;

#define WIDGET_BOUNDING         5

/**
 * @brief Load default widgets
 */
void widgets_load() {
    if (current_x < 0) current_x = taskbar_window->width;
    if (!widgets_list) widgets_list = list_create("tray widgets");

    DIR *dirp = opendir("/usr/lib/widgets/");
    if (!dirp) {
        fprintf(stderr, "Failed to open /usr/lib/widgets\n");
        return;
    }

    struct dirent *ent = readdir(dirp);
    while (ent) {
        if (ent->d_name[0] == '.' || !strstr(ent->d_name, ".so")) goto _next_entry;

        char path[512] = { 0 };
        snprintf(path, 512, "/usr/lib/widgets/%s", ent->d_name);
        fprintf(stderr, "Loading widget: %s\n", path);

        // The idea of using .so files is from ToaruOS.
        void *dso = dlopen(path, RTLD_LAZY);
        if (!dso) {
            fprintf(stderr, "Error loading widget %s: %s\n", path, dlerror());
            goto _next_entry;
        }

        desktop_tray_widget_data_t *data = (desktop_tray_widget_data_t*)dlsym(dso, "this_widget");
        if (!data) {
            fprintf(stderr, "Error getting \"this_widget\" from widget %s: %s\n", path, dlerror());
            goto _next_entry;
        }

        // Okay, I think this is the right widget.
        desktop_tray_widget_t *widget = malloc(sizeof(desktop_tray_widget_t));
        memset(widget, 0, sizeof(desktop_tray_widget_t));
        widget->data = data;
        widget->dso = dso;
        widget->width = 40;
        widget->height = taskbar_window->height;
        widget->state = TRAY_WIDGET_STATE_IDLE;

        // Do initialization
        // TODO: Check fail
        if (data->init) data->init(widget);
        if (widget->height > taskbar_window->height) widget->height = taskbar_window->height;
        
        if (widget->padded_right) {
            // Reinitialize the context
            current_x -= widget->padded_right;
        }


        // Bound the widget
        current_x -= WIDGET_BOUNDING;
        current_x -= widget->width;

        widget->rect.x = current_x;
        widget->rect.y = widget->padded_top ? widget->padded_top : (taskbar_window->height / 2) - (widget->height / 2);
        widget->rect.height = widget->height;
        widget->rect.width = widget->width;
        widget->ctx = gfx_createContextSubrect(celestial_getGraphicsContext(taskbar_window), &widget->rect) ;
        gfx_createClip(celestial_getGraphicsContext(taskbar_window), widget->rect.x, widget->rect.y, widget->rect.width, widget->rect.height);
        current_x -= widget->padded_left;

        list_append(widgets_list, widget);
        widget->data->icon(widget);
    _next_entry:
        ent = readdir(dirp);
    }
}

/**
 * @brief Update widgets
 */
void widgets_update() {
    foreach(widgetnode, widgets_list) {
        desktop_tray_widget_t *widget = (desktop_tray_widget_t*)widgetnode->value;

        if (widget->state == TRAY_WIDGET_STATE_HIGHLIGHTED || widget->state == TRAY_WIDGET_STATE_ACTIVE) {
            gfx_drawRectangleFilled(widget->ctx, &GFX_RECT(0, 5, 1, widget->height - 10), GFX_RGB(170, 170, 170));
            gfx_drawRectangleFilled(widget->ctx, &GFX_RECT(widget->width - 1, 5, 1, widget->height - 10), GFX_RGB(170, 170, 170));
        } else if (widget->state == TRAY_WIDGET_STATE_HELD) {
            // gfx_drawRectangleFilledGradient(widget->ctx, &GFX_RECT(0, 0, GFX_WIDTH(highlighted->ctx), GFX_HEIGHT(highlighted->ctx)), GFX_GRADIENT_VERTICAL, GFX_RGBA(35, 35, 35, 0), GFX_RGBA(35, 35, 35, 200));
            widget->data->icon(widget);
            gfx_drawRectangleFilled(widget->ctx, &GFX_RECT(0, 5, 1, widget->height - 10), GFX_RGB(170, 170, 170));
            gfx_drawRectangleFilled(widget->ctx, &GFX_RECT(widget->width - 1, 5, 1, widget->height - 10), GFX_RGB(170, 170, 170));
            gfx_render(widget->ctx);
            continue;
        }

        widget->data->icon(widget);
        gfx_render(widget->ctx);
    }
}

/**
 * @brief Handle mouse movement
 */
void widget_mouseMovement(unsigned x, unsigned y) {
    int clr_highlighted = 1;
    foreach(widgetnode, widgets_list) {
        desktop_tray_widget_t *widget = (desktop_tray_widget_t*)widgetnode->value;
    
        if (widget->state == TRAY_WIDGET_STATE_DISABLED) continue;
        if (x >= widget->rect.x && x < widget->rect.x + widget->rect.width) {
            if (y >= widget->rect.y && y < widget->rect.y + widget->rect.height) {
                if (highlighted == widget) {
                    clr_highlighted = 0;
                } else {
                    if (highlighted) {
                        highlighted->state = TRAY_WIDGET_STATE_IDLE;
                    }
                
                    widget->state = TRAY_WIDGET_STATE_HIGHLIGHTED;
                    highlighted = widget;
                    clr_highlighted = 0;
                }
            }
        }
        
    }


    if (clr_highlighted && highlighted) {
        highlighted->state = TRAY_WIDGET_STATE_IDLE;
        highlighted = NULL;
    }
}

/**
 * @brief Handle mouse exit
 */
void widget_mouseExit() {
    if (highlighted && highlighted->state != TRAY_WIDGET_STATE_ACTIVE) {
        highlighted->state = TRAY_WIDGET_STATE_IDLE;
        highlighted->data->icon(highlighted);
        gfx_render(highlighted->ctx);
        celestial_flip(taskbar_window);
        highlighted = NULL;
    }
}

/**
 * @brief Handle mouse click
 */
void widget_mouseClick(unsigned x, unsigned y) {
    if (highlighted && highlighted->state != TRAY_WIDGET_STATE_ACTIVE) {
        highlighted->state = TRAY_WIDGET_STATE_HELD;
    }
}   

/**
 * @brief Handle mouse release
 */
void widget_mouseRelease(unsigned x, unsigned y) {
    if (highlighted != active && active) {
        if (active->data->set) active->data->set(active, 0);
        active->data->icon(active);
        gfx_render(active->ctx);
        celestial_flip(taskbar_window);
        active = NULL;
    }

    if (highlighted && highlighted->state != TRAY_WIDGET_STATE_ACTIVE) {
        highlighted->state = TRAY_WIDGET_STATE_ACTIVE;
        if (highlighted->data->set) highlighted->data->set(highlighted, 1);
        active = highlighted;
    } else if (highlighted && highlighted->state == TRAY_WIDGET_STATE_ACTIVE) {
        highlighted->state = TRAY_WIDGET_STATE_HIGHLIGHTED;
        if (highlighted->data->set) highlighted->data->set(highlighted, 0);
        active = NULL;
    }
}