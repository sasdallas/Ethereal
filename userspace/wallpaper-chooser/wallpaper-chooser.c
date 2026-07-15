/**
 * @file userspace/wallpaper-chooser/wallpaper-chooser.c
 * @brief Wallpaper selector
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <graphics/gfx.h>
#include <neutron/neutron.h>

struct wp {
    struct wp *next;
    struct wp *prev;
    char name[255];
};

typedef struct nt_wallpaper {
    nt_widget_t w;
    struct wp *wallpaper;
    nt_image_t img;
} nt_wallpaper_t;

struct wp *wallpaper_list = NULL;
struct wp *wallpaper_list_tail = NULL;

nt_wallpaper_t *wallpaper_widget = NULL;

static void wallpaper_calc_size(nt_widget_t *w) {
    w->size_data.min_width = 50;
    w->size_data.min_height = 25;
    w->size_data.pref_width = 50;
    w->size_data.pref_height = 25;
}

static void wallpaper_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_wallpaper_t *wp = (nt_wallpaper_t*)w;
    nt_render_draw_image_scaled(surf, &wp->img, NT_RECT(w->style.padding[LEFT], w->style.padding[TOP], nt_widget_get_width_inner(w), nt_widget_get_height_inner(w)));
}

nt_widget_vtable_t wallpaper_vtable = {
    .class_size = sizeof(nt_wallpaper_t),
    .calc_size = wallpaper_calc_size,
    .adjust_size = NULL,
    .init = NULL,
    .free = NULL,
    .measure = NULL,
    .render = wallpaper_render
};

int load_wallpaper(nt_wallpaper_t *wp, struct wp *wallpaper) {
    if (wp->wallpaper) {
        nt_render_free_image(&wp->img);
    }

    if (nt_render_create_image(&wp->img, wallpaper->name)) {
        fprintf(stderr, "Error loading wallpaper: %s\n", wallpaper->name);
        return 1;
    }

    wp->wallpaper = wallpaper;

    nt_widget_invalidate(&wp->w);

    return 0;
}

static void left_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (wallpaper_widget->wallpaper->prev) {
        load_wallpaper(wallpaper_widget, wallpaper_widget->wallpaper->prev);
    }
}

static void right_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (wallpaper_widget->wallpaper->next) {
        load_wallpaper(wallpaper_widget, wallpaper_widget->wallpaper->next);
    }
}

static void set_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char cmd[4096];
    snprintf(cmd, 4096, "set-wallpaper /usr/share/wallpapers/%s", wallpaper_widget->wallpaper->name);
    system(cmd);
}

int main(int argc, char *argv[]) {
    if (nt_init()) {
        fprintf(stderr, "Failed to initialize Neutron\n");
        return 1;
    }

    nt_window_t *win = nt_window_create("Wallpaper Chooser", 700, 410);
    nt_widget_t *box_root = nt_box_create_vertical();
    nt_widget_set_expansion(box_root, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_window_set_root(win, box_root);
    
    nt_widget_t *box = nt_box_create_horizontal();
    nt_widget_set_expansion(box, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    
    nt_box_append(box_root, box);

    if (chdir("/usr/share/wallpapers") < 0) {
        perror("chdir");
        return 1;
    }

    DIR *d = opendir(".");
    if (!d) {
        perror("opendir");
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;

        struct wp *w = malloc(sizeof(struct wp));
        strncpy(w->name, ent->d_name, 255);

        if (wallpaper_list_tail) wallpaper_list_tail->next = w;
        w->next = NULL;
        w->prev = wallpaper_list_tail;
        wallpaper_list_tail = w;
        if (!wallpaper_list) wallpaper_list = w;
    }

    closedir(d);

    // Create left and right buttons
    nt_widget_t *left_btn = nt_button_create_label("<");
    nt_widget_t *right_btn = nt_button_create_label(">");

    // Create the wallpaper widget, which is a custom widget since nt_image cant do what is needed
    nt_widget_t *custom = nt_widget_create(NT_WIDGET_CUSTOM, &wallpaper_vtable);
    nt_widget_set_expansion(custom, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    load_wallpaper((nt_wallpaper_t*)custom, wallpaper_list);
    wallpaper_widget = (nt_wallpaper_t*)custom;

    // E x p a n d
    nt_widget_set_expansion(left_btn, NT_EXPAND_VERTICAL);
    nt_widget_set_expansion(right_btn, NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(left_btn, NT_ALIGN_CENTER);
    nt_widget_set_vertical_alignment(right_btn, NT_ALIGN_CENTER);

    // And in the box you go
    nt_box_append(box, left_btn);
    nt_box_append(box, custom);
    nt_box_append(box, right_btn);

    nt_signal_connect(left_btn, "pressed", left_pressed, NULL);
    nt_signal_connect(right_btn, "pressed", right_pressed, NULL);

    // Add a set button
    nt_widget_t *set_btn = nt_button_create_label("Set Wallpaper");
    nt_widget_set_expansion(set_btn, NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(set_btn, NT_ALIGN_CENTER);
    nt_signal_connect(set_btn, "pressed", set_pressed, NULL);
    nt_box_append(box_root, set_btn);

    nt_loop();
    return 0;;
}
