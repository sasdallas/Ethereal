/**
 * @file usersace/taskbar/taskbar.c
 * @brief Ethereal taskbar manager
 * 
 * @warning The code in here is, like everything Neutron-related, designed to look good when run and not care about code quality.
 *          I warn you: Look at your own risk.
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#define APP_NAME "taskbar"
#include <neutron/neutron.h>
#include <ethereal/log.h>
#include <ethereal/celestial.h>
#include <structs/ini.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>

/* Taskbar state */
nt_window_t *taskbar_window = NULL;
nt_widget_t *window_list = NULL;
nt_widget_t *tray_list = NULL;

/* Start menu state */
nt_window_t *start_menu_window = NULL;
bool start_window_shown = false;
nt_widget_t *start_scroll = NULL;
nt_widget_t *start_list = NULL;

/* Alt-tab window */
nt_window_t *alt_tab = NULL;
bool alt_tab_visible = false;
int alt_tab_index = 0;
int alt_tab_num_windows = 0;

/* Screen */
int screen_width, screen_height;

/* Stupid hack */
static nt_widget_vtable_t start_hack_vtable;
static bool did_start_hack_stupid = false;

/* Silly helper */
#define LERP(a,b,c) (float)(a) + ((float)(b) - (float)(a)) * (float)(c)

static nt_color_t premultiply(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return NT_COLOR(r * a / 255, g * a / 255, b * a / 255, a);
}

// Silly little window structure
struct taskbar_window {
    wid_t window_id;
    nt_fade_t *fade;
    bool highlighted;
    nt_widget_t *w;
    nt_widget_t *label;
    nt_widget_t *icon;
};

struct start_item {
    bool folder;
    char exec[256];
};

void change_directory(char *path);

void taskbar_window_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_rect_t rect = NT_RECT(0, 0, surf->width, surf->height);

    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));
    nt_render_rounded_rect_gradient(surf, &rect, 3, w->style.background.top, w->style.background.bot, false);
    nt_render_border_rounded_rect(surf, &rect, 1, 3, w->style.border.top);
}

// the big all in one for window lists. if not using hover, set to 0.0f
void taskbar_window_style(struct taskbar_window *tb, float hover) {
    nt_color_t top = tb->highlighted ? NT_COLOR(0x86, 0x83, 0x89, 125) : NT_COLOR(0x70, 0x6d, 0x73, 85);
    nt_color_t bot = tb->highlighted ? NT_COLOR(0x42, 0x3e, 0x48, 150) : NT_COLOR(0x2d, 0x29, 0x31, 115);

    nt_color_t border = tb->highlighted ? NT_COLOR(0xb8, 0xb3, 0xbd, 115) : NT_COLOR(0x9d, 0x98, 0xa2, 75);

    uint8_t top_a = LERP(NT_COLOR_A(top), 175, hover);
    uint8_t bot_a = LERP(NT_COLOR_A(bot), 180, hover);
    uint8_t border_a = LERP(NT_COLOR_A(border), 145, hover);

    nt_color_t top_color = premultiply(
        LERP(NT_COLOR_R(top), 0x98, hover),
        LERP(NT_COLOR_G(top), 0x94, hover),
        LERP(NT_COLOR_B(top), 0x9d, hover),
        top_a);

    nt_color_t bot_color = premultiply(
        LERP(NT_COLOR_R(bot), 0x4e, hover),
        LERP(NT_COLOR_G(bot), 0x49, hover),
        LERP(NT_COLOR_B(bot), 0x54, hover),
        bot_a);

    nt_color_t border_color = premultiply(
        LERP(NT_COLOR_R(border), 0xad, hover),
        LERP(NT_COLOR_G(border), 0xa8, hover),
        LERP(NT_COLOR_B(border), 0xb2, hover),
        border_a);


    nt_style_set_bg_gradient_colors(&tb->w->style, top_color, bot_color);
    nt_style_set_border_color(&tb->w->style, border_color);
}

void taskbar_window_fade(nt_fade_t *fade) {
    nt_widget_t *w = fade->priv;
    struct taskbar_window *tb = w->priv;

    taskbar_window_style(tb, fade->time);
    nt_widget_invalidate(w);
}

bool taskbar_window_event(nt_widget_t *w, nt_event_t *e) {
    struct taskbar_window *tb = w->priv;
    if (e->type == NT_EVENT_MOUSE_ENTER) {
        nt_fade_set(tb->fade, true);
        nt_fade_start(tb->fade);
    } else if (e->type == NT_EVENT_MOUSE_LEAVE) {
        nt_fade_set(tb->fade, false);
        nt_fade_start(tb->fade);
    } else if (e->type == NT_EVENT_MOUSE_DOWN) {
        nt_fade_stop(tb->fade);
        nt_style_set_bg_gradient_colors(&w->style, premultiply(0x78, 0x74, 0x7d, 125), premultiply(0x38, 0x35, 0x3d, 155));
        nt_widget_invalidate(w);
    } else if (e->type == NT_EVENT_MOUSE_UP) {
        nt_fade_stop(tb->fade);
        tb->fade->time = 1.0f;
        taskbar_window_style(tb, tb->fade->time);
        nt_widget_invalidate(w);

        celestial_setFocusID(tb->window_id, true);
    }

    return true;
}

bool start_button_event(nt_widget_t *w, nt_event_t *e) {
    if (e->type == NT_EVENT_MOUSE_DOWN) {
        nt_style_set_bg_color(&w->style, premultiply(0x58, 0x55, 0x5d, 150));
    } else if (e->type == NT_EVENT_MOUSE_UP) {
        nt_style_set_bg_color(&w->style, premultiply(0x90, 0x8c, 0x96, 145));

        if (start_window_shown == false) {
            change_directory("/etc/desktop.d");
        }

        start_window_shown = !start_window_shown;
        nt_window_set_visible(start_menu_window, start_window_shown);
        
        // hack
        celestial_setFocus((window_t*)start_menu_window->platform, true);
    } else if (e->type == NT_EVENT_MOUSE_ENTER) {
        nt_style_set_bg_color(&w->style, premultiply(0x90, 0x8c, 0x96, 145));
    } else if (e->type == NT_EVENT_MOUSE_LEAVE) {
        nt_style_set_bg_color(&w->style, NT_COLOR(0,0,0,0));
    }

    nt_widget_invalidate(w);

    return true;
}

void start_button_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_rect_t rect = NT_RECT(0, 0, surf->width, surf->height);

    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));

    // TODO: WHY IS THIS SHIT REQUIRED???
    nt_render_rounded_rect_gradient(surf, &rect, 6, w->style.background.top, w->style.background.top, false);
}

bool start_all_app_event(nt_widget_t *w, nt_event_t *e) {
    if (e->type == NT_EVENT_MOUSE_ENTER) {
        nt_style_set_bg_color(&w->style, NT_COLOR(220,220,220,255));
    } else if (e->type == NT_EVENT_MOUSE_LEAVE) {
        nt_style_set_bg_color(&w->style, NT_COLOR(255,255,255,255));
    } else if (e->type == NT_EVENT_MOUSE_DOWN) {

    }

    nt_widget_invalidate(w);

    return true;
}

bool start_menu_entry_event(nt_widget_t *w, nt_event_t *ev) {
    if (ev->type == NT_EVENT_MOUSE_ENTER) {
        nt_widget_set_selected(w, true);
    } else if (ev->type == NT_EVENT_MOUSE_LEAVE) {
        nt_widget_set_selected(w, false);
    }

    nt_widget_invalidate(w);
    return true;
}

void start_list_item_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));

    if (w->children && w->children->selected) {
        nt_rect_t rect = NT_RECT(2, 1, surf->width - 4, surf->height - 2);
        nt_render_rounded_rect_gradient(surf, &rect, 5, NT_COLOR(0x5d, 0xa3, 0xec, 255), NT_COLOR(0x39, 0x87, 0xe4, 255), false);
    }
}

struct taskbar_window *taskbar_get_window(wid_t window_id) {
    NT_ITERATE_CHILDREN(window_list) {
        struct taskbar_window *tb = child->priv;
        if (tb->window_id == window_id) {
            return tb;
        }
    }

    return NULL;
}

void taskbar_set_highlighted(struct taskbar_window *tb, bool highlighted) {
    tb->highlighted = highlighted;
    taskbar_window_style(tb, tb->fade->time);
    nt_widget_invalidate(tb->w);
}

void taskbar_add_window(wid_t window_id, char *window_name, char *window_icon, bool highlighted) {
    struct taskbar_window *exist = taskbar_get_window(window_id);
    if (exist) {
        nt_image_wdgt_t *img = (nt_image_wdgt_t*)exist->icon;
        img->img = nt_icon_get(window_icon, NULL, 16);
        nt_label_set_text(exist->label, window_name);
        taskbar_set_highlighted(exist, highlighted);
        nt_widget_invalidate(exist->icon);
        return;
    }

    nt_widget_t *w = nt_box_create_horizontal();
    
    struct taskbar_window *tb = malloc(sizeof(struct taskbar_window));
    tb->window_id = window_id;
    tb->fade = nt_fade_create(250, taskbar_window_fade, w);
    tb->highlighted = highlighted;
    tb->w = w;
    w->priv = tb;

    nt_event_set_handler(w, NT_EVENT_MOUSE_ENTER, taskbar_window_event);
    nt_event_set_handler(w, NT_EVENT_MOUSE_LEAVE, taskbar_window_event);
    nt_event_set_handler(w, NT_EVENT_MOUSE_DOWN, taskbar_window_event);
    nt_event_set_handler(w, NT_EVENT_MOUSE_UP, taskbar_window_event);
    nt_widget_set_expansion(w, NT_EXPAND_VERTICAL);
    nt_style_set_suggested_width(&w->style, 150);
    nt_style_set_margin_all(&w->style, 1);
    nt_style_set_margin(&w->style, RIGHT, 5);
    nt_style_set_bg_gradient(&w->style, NT_STYLE_GRADIENT_VERT);
    nt_style_set_border_thickness(&w->style, 1);
    nt_style_set_border_rounded(&w->style, 3);
    nt_style_set_padding_all(&w->style, 1);
    nt_widget_set_render_hook(w, taskbar_window_render);

    // set the initial style
    taskbar_window_style(tb, 0.0f);
    
    nt_widget_t *img = nt_image_create_from_image(nt_icon_get(window_icon, NULL, 16));
    nt_widget_set_expansion(img, NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(img, NT_ALIGN_CENTER);
    nt_style_set_margin_all(&img->style, 2);
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_box_append(w, img);

    nt_widget_t *lbl = nt_label_create(window_name);
    nt_widget_set_vertical_alignment(lbl, NT_ALIGN_CENTER);
    nt_widget_set_expansion(lbl, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&lbl->style, 2);
    nt_style_set_margin(&lbl->style, TOP, 3);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_style_set_fg_color(&lbl->style, NT_COLOR(255,255,255,255));
    nt_box_append(w, lbl);


    tb->label = lbl;
    tb->icon = img;
    tb->w = w;

    nt_box_append(window_list, w);
}

void taskbar_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));

    nt_rect_t rect = NT_RECT(0, 0, surf->width, surf->height);
    nt_render_fill_rect_gradient(surf, &rect, premultiply(0x32, 0x30, 0x38, 145), premultiply(0x24, 0x22, 0x28, 175), false);

    nt_rect_t highlight = NT_RECT(0, 0, surf->width, 1);
    nt_render_fill_rect(surf, &highlight, premultiply(0xe0, 0xdd, 0xe5, 80));
}

void taskbar_create() {
    nt_widget_t *taskbar_box = nt_box_create_horizontal();
    nt_widget_set_expansion(taskbar_box, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&taskbar_box->style, 0);
    nt_style_set_bg_color(&taskbar_box->style, NT_COLOR(0,0,0,0));
    nt_widget_set_render_hook(taskbar_box, taskbar_render);
    nt_window_set_root(taskbar_window, taskbar_box);

    nt_widget_t *start_box = nt_box_create_horizontal();

    nt_widget_set_expansion(start_box, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&start_box->style, 0);
    nt_style_set_suggested_width(&start_box->style, 100);
    nt_style_set_bg_color(&start_box->style, NT_COLOR(0, 0, 0, 0));
    nt_box_append(taskbar_box, start_box);

    nt_widget_t *start_button = nt_box_create_horizontal();
    nt_event_set_handler(start_button, NT_EVENT_MOUSE_DOWN, start_button_event);
    nt_event_set_handler(start_button, NT_EVENT_MOUSE_UP, start_button_event);
    nt_event_set_handler(start_button, NT_EVENT_MOUSE_ENTER, start_button_event);
    nt_event_set_handler(start_button, NT_EVENT_MOUSE_LEAVE, start_button_event);
    nt_style_set_margin_all(&start_button->style, 3);
    // nt_style_set_bg_gradient(&start_button->style, NT_STYLE_GRADIENT_VERT);
    // nt_style_set_bg_gradient_colors(&start_button->style, premultiply(0x70, 0x6c, 0x75, 105), premultiply(0x35, 0x32, 0x3a, 130));
    nt_style_set_bg_color(&start_button->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(start_button, NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(start_button, NT_ALIGN_CENTER);
    nt_style_set_bg_rounded(&start_button->style, 6);
    nt_widget_set_render_hook(start_button, start_button_render);
    nt_box_append(start_box, start_button);

    nt_widget_t *start_icon = NULL;

    nt_image_t *img = nt_icon_get("ethereal", NULL, 24);
    if (img) {
        start_icon = nt_image_create_from_image(img);
    } else {
        start_icon = nt_label_create("Start");
        nt_style_set_font(&start_icon->style, NT_SANS_16);
        nt_style_set_fg_color(&start_icon->style, NT_COLOR(255,255,255,255));
    }

    nt_widget_set_expansion(start_icon, NT_EXPAND_VERTICAL | NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(start_icon, NT_ALIGN_CENTER);
    nt_widget_set_vertical_alignment(start_icon, NT_ALIGN_CENTER);
    nt_style_set_margin_all(&start_icon->style, 0);
    nt_style_set_padding_all(&start_icon->style, 1);
    nt_style_set_bg_color(&start_icon->style, NT_COLOR(0,0,0,0));
    nt_box_append(start_button, start_icon);

    window_list = nt_box_create_horizontal();
    nt_widget_set_expansion(window_list, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&window_list->style, 0);
    nt_style_set_bg_color(&window_list->style, NT_COLOR(0,0,0,0));
    nt_box_append(taskbar_box, window_list);


    tray_list = nt_box_create_horizontal();
    nt_widget_set_expansion(tray_list, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&tray_list->style, 0);
    nt_style_set_margin(&tray_list->style, LEFT, 10);
    nt_style_set_margin(&tray_list->style, RIGHT, 2);
    nt_style_set_bg_color(&tray_list->style, NT_COLOR(0,0,0,0));
    nt_box_append(taskbar_box, tray_list);

    nt_widget_t *lbl2 = nt_label_create("9:47 PM");
    nt_style_set_fg_color(&lbl2->style, NT_COLOR(255,255,255,255));
    nt_style_set_font(&lbl2->style, NT_SANS_12);
    nt_style_set_bg_color(&lbl2->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(lbl2, NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(lbl2, NT_ALIGN_CENTER);
    nt_box_append(tray_list, lbl2);

}

void start_item_pressed(nt_widget_t *w, nt_signal_t *sig, void *ctx) {
    struct start_item *start = ctx;

    if (start->folder) {
        change_directory(start->exec);
    } else {
        if (!fork()) {
            chdir("/");
            char *args[] = {
                "/usr/bin/essence",
                "-c",
                start->exec,
                NULL
            };

            execvp("/usr/bin/essence", args);
            exit(EXIT_FAILURE);
        }
        
        start_window_shown = false;
        nt_window_set_visible(start_menu_window, false);
    }
}

void start_add_item(nt_widget_t *w, char *item_name, char *item_icon, char *item_exec, bool folder) {
    struct start_item *start = malloc(sizeof(struct start_item));

    if (folder) item_icon = "folder";
    if (item_exec) {
        strncpy(start->exec, item_exec, 256);

        if (strlen(item_exec) >= 256) {
            TRACE_ERROR("Exec \"%s\" is too long!\n");    
            return;
        }
    } else {
        NT_WARN("Missing Exec entry in start menu item!\n");
        strncpy(start->exec, "show-dialog --error --text=\"Corrupt start menu entry\" --title=\"Taskbar\"", 256);
    }

    if (!item_name) item_name = "[bad INI file]";
    if (!item_icon) item_icon = "missing";

    start->folder = folder;

    nt_widget_t *box = nt_box_create_horizontal();
    nt_style_set_margin_all(&box->style, 0);
    nt_style_set_bg_color(&box->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(box, NT_EXPAND_HORIZONTAL);
    box->priv = start;

    nt_widget_t *img = nt_image_create_from_image(nt_icon_get(item_icon, NULL, 24));
    nt_style_set_margin_all(&img->style, 2);
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_box_append(box, img);
    
    nt_widget_t *lbl = nt_label_create(item_name);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_box_append(box, lbl);

    nt_event_set_handler(box, NT_EVENT_MOUSE_ENTER, start_menu_entry_event);
    nt_event_set_handler(box, NT_EVENT_MOUSE_LEAVE, start_menu_entry_event);

    nt_list_view_append(w, box);

    // THIS IS A FUCKING HACK TO GET ROUNDED LIST ITEMS (overriding the render)
    if (!did_start_hack_stupid) {
        start_hack_vtable = *box->parent->vtbl;
        start_hack_vtable.render = start_list_item_render;
        did_start_hack_stupid = true;
    }

    box->parent->vtbl = &start_hack_vtable;
    nt_style_set_bg_color(&box->parent->style, NT_COLOR(0, 0, 0, 0));

    nt_signal_connect(box, "selected", start_item_pressed, start);
}

static int start_sort(const void *s1, const void *s2) {
    return strcmp(*(const char**)s1, *(const char**)s2);
}

nt_widget_t *start_build_list_view() {
    nt_widget_t *list_view = nt_list_view_create();
    nt_style_set_border_thickness(&list_view->style, 0);
    nt_style_set_margin_all(&list_view->style, 0);
    nt_widget_set_expansion(list_view, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);

    DIR *dirp = opendir(".");
    if (dirp == NULL) {
        TRACE_ERROR("opendir failed with error: %s\n", strerror(errno));
        return list_view;
    }

    char **files = NULL;
    char **folders = NULL;
    int num_files = 0;
    int num_folders = 0;

    struct dirent *ent;
    while ((ent = readdir(dirp))) {
        if (ent->d_name[0] == '.') {
            continue;
        }

        TRACE_DEBUG("Test name: %s (%d)\n", ent->d_name, ent->d_type);
        if (ent->d_type == DT_DIR) {
            folders = realloc(folders, (num_folders+1) * sizeof(char*));
            folders[num_folders++] = strdup(ent->d_name);
        } else if (ent->d_type == DT_REG && isdigit(ent->d_name[0])) {
            files = realloc(files, (num_files+1) * sizeof(char*));
            files[num_files++] = strdup(ent->d_name);
        } else {
            TRACE_DEBUG("Extra entry: %s\n", ent->d_name);
        }
    }

    closedir(dirp);

    qsort(files, num_files, sizeof(char*), start_sort);
    qsort(folders, num_folders, sizeof(char*), start_sort);

    if (folders) {
        for (unsigned i = 0; i < num_folders; i++) {
            if (!strcmp(folders[i], "Back")) {
                start_add_item(list_view, folders[i], "folder", "..", true);
            } else {
                start_add_item(list_view, folders[i], "folder", folders[i], true);
            }

            free(folders[i]);
        }

        free(folders);
    }


    if (files) {
        for (unsigned i = 0; i < num_files; i++) {
            ini_t *ini = ini_load(files[i]);
            if (ini == NULL) {
                TRACE_WARN("Failed to open %s\n", files[i]);
                continue;
            }

            char *name = ini_get(ini, "Desktop", "Name");
            char *icon = ini_get(ini, "Desktop", "Icon");
            char *exec = ini_get(ini, "Desktop", "Exec");
            start_add_item(list_view, name, icon, exec, false);
            ini_destroy(ini);

            free(files[i]);
        }

        free(files);
    }

    return list_view;
}

void start_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));

    nt_rect_t rect = NT_RECT(0, 0, surf->width, surf->height);
    nt_render_fill_rect_gradient(surf, &rect, premultiply(0x32, 0x30, 0x38, 145), premultiply(0x24, 0x22, 0x28, 175), false);
}

void tab_render(nt_widget_t *w, nt_render_surface_t *surf) {
    nt_render_clear(surf, NT_COLOR(0, 0, 0, 0));

    nt_rect_t rect = NT_RECT(0, 0, surf->width, surf->height);
    nt_render_rounded_rect_gradient(surf, &rect, 4, premultiply(0x32, 0x30, 0x38, 195), premultiply(0x24, 0x22, 0x28, 220), false);

}

void unfocused_handler(window_t *win, uint32_t event_type, void *event) {
    start_window_shown = false;
    nt_window_set_visible(start_menu_window, false);
}

void start_create() {
    start_menu_window = nt_window_create_flags(410, 448, CELESTIAL_WINDOW_FLAG_BLURRED);
    nt_window_set_visible(start_menu_window, start_window_shown);
    nt_window_set_pos(start_menu_window, 0, screen_height-31-448);

    // When this window loses focus it should go invisible
    celestial_setHandler((window_t*)start_menu_window->platform, CELESTIAL_EVENT_UNFOCUSED, unfocused_handler);

    // Create the root box
    nt_widget_t *root = nt_box_create_horizontal();
    nt_style_set_margin_all(&root->style, 0);
    nt_widget_set_expansion(root, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_window_set_root(start_menu_window, root);
    nt_style_set_bg_color(&root->style, NT_COLOR(0,0,0,0));
    nt_widget_set_render_hook(root, start_render);

    // Create the left side
    nt_widget_t *left = nt_box_create_vertical();
    nt_style_set_border_thickness(&left->style, 2);
    nt_style_set_border_rounded(&left->style, 3);
    nt_style_set_border_color(&left->style, NT_COLOR(0xf3,0xf3,0xf3,255));
    nt_style_set_padding_all(&left->style, 2);
    nt_widget_set_expansion(left, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_bg_color(&left->style, NT_COLOR(0xf3,0xf3,0xf3,255));
    nt_style_set_margin_all(&left->style, 5);
    nt_box_append(root, left);

    // Scroll container
    nt_widget_t *sc = nt_scroll_container_create();
    nt_widget_set_expansion(sc, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&sc->style, 0);
    nt_box_append(left, sc);
    start_scroll = sc;

    // Main list view for all applications
    start_list = start_build_list_view();
    nt_scroll_container_set_child(sc, start_list);

    // Create "show applications" button
    nt_widget_t *all_apps = nt_box_create_horizontal();
    nt_event_set_handler(all_apps, NT_EVENT_MOUSE_ENTER, start_all_app_event);
    nt_event_set_handler(all_apps, NT_EVENT_MOUSE_LEAVE, start_all_app_event);
    nt_event_set_handler(all_apps, NT_EVENT_MOUSE_DOWN, start_all_app_event);
    nt_widget_set_expansion(all_apps, NT_EXPAND_HORIZONTAL);
    nt_style_set_margin_all(&all_apps->style, 0);
    nt_style_set_margin(&all_apps->style, TOP, 2);
    nt_style_set_bg_color(&all_apps->style, NT_COLOR(255,255,255,255));
    nt_box_append(left, all_apps);

    nt_widget_t *lbl = nt_label_create("All Applications");
    nt_widget_set_expansion(lbl, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&lbl->style, 1);
    nt_style_set_margin(&lbl->style, TOP, 3);
    nt_style_set_margin(&lbl->style, BOTTOM, 3);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_widget_set_vertical_alignment(lbl, NT_ALIGN_CENTER);
    nt_box_append(all_apps, lbl);

    // Separator
    nt_widget_t *sep = nt_separator_create(NT_SEPARATOR_HORIZONTAL);
    nt_style_set_margin_all(&sep->style, 0);
    nt_box_append(left, sep);

    // Search
    nt_widget_t *search = nt_input_create("search non-functional yet!");
    nt_widget_set_expansion(search, NT_EXPAND_HORIZONTAL);
    nt_box_append(left, search);
    nt_window_set_root(start_menu_window, root);

    // Right panel (account, quick access, power)
    nt_widget_t *right = nt_box_create_vertical();
    nt_widget_set_expansion(right, NT_EXPAND_VERTICAL);
    nt_style_set_suggested_width(&right->style, 150);
    nt_style_set_bg_color(&right->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&right->style, 2);
    nt_style_set_margin(&right->style, RIGHT, 4);
    nt_box_append(root, right);
}


void change_directory(char *path) {
    if (chdir(path) < 0) {
        TRACE_ERROR("Failed to change directory to %s: %s\n", path, strerror(errno));
        return;
    }

    nt_widget_t *new = start_build_list_view();
    nt_scroll_container_set_child(start_scroll, new);

    NT_ITERATE_CHILDREN(start_list) {
        free(start_list->priv);
    }

    nt_widget_free(start_list);
    start_list = new;
}


void app_hook(nt_widget_t *w, nt_render_surface_t *surf) {
    int idx = (int)(uintptr_t)w->priv;

    nt_render_clear(surf, NT_COLOR(0,0,0,0));
    if (idx == alt_tab_index) {
        nt_render_rounded_rect(surf, &NT_RECT(0, 0, nt_widget_get_width_inner(w), nt_widget_get_height_inner(w)), 4, NT_COLOR(170,170,170,255));
    }
}

void alt_tab_show() {
    alt_tab = nt_window_create_flags(screen_width, 125, 0);
    nt_window_set_visible(alt_tab, true);

    nt_widget_t *container = nt_box_create_horizontal();
    nt_style_set_margin_all(&container->style, 0);
    nt_style_set_bg_color(&container->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(container, NT_EXPAND_VERTICAL | NT_EXPAND_HORIZONTAL);
    nt_window_set_root(alt_tab, container);

    nt_widget_t *root = nt_box_create_horizontal();
    nt_style_set_margin_all(&root->style, 0);
    nt_widget_set_expansion(root, NT_EXPAND_VERTICAL | NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(root, NT_ALIGN_CENTER);
    nt_style_set_bg_color(&root->style, NT_COLOR(0,0,0,0));
    nt_widget_set_render_hook(root, tab_render);
    nt_box_append(container, root);

    int idx = 0;

    NT_ITERATE_CHILDREN(window_list) {
        struct taskbar_window *tb = child->priv;

        window_info_t info;
        if (celestial_queryWindow(tb->window_id, &info)) {
            continue;
        }

        nt_widget_t *w = nt_box_create_vertical();
        nt_widget_set_expansion(w, NT_EXPAND_VERTICAL);
        nt_style_set_bg_color(&w->style, NT_COLOR(0,0,0,0));
        nt_widget_set_render_hook(w, app_hook);
        w->priv = (void*)(uintptr_t)idx++;

        nt_widget_t *icon = nt_image_create_from_image(nt_icon_get(info.icon, NULL, 64));
        nt_style_set_bg_color(&icon->style, NT_COLOR(0,0,0,0));
        nt_box_append(w, icon);
        nt_widget_set_expansion(icon, NT_EXPAND_HORIZONTAL);
        nt_widget_set_horizontal_alignment(icon, NT_ALIGN_CENTER);

        nt_widget_t *name = nt_label_create(info.name);
        nt_widget_set_expansion(name, NT_EXPAND_HORIZONTAL);
        nt_widget_set_horizontal_alignment(name, NT_ALIGN_CENTER);
        nt_style_set_fg_color(&name->style, NT_COLOR(255,255,255,255));
        nt_style_set_font(&name->style, NT_SANS_12);
        nt_style_set_bg_color(&name->style, NT_COLOR(0,0,0,0));
        nt_box_append(w, name);

        nt_box_append(root, w);
    }

    alt_tab_num_windows = idx;

    celestial_setFocus((window_t*)alt_tab->platform, true);
}

void alt_tab_hide() {
    if (alt_tab == NULL) return;
    
    nt_window_close(alt_tab);
    alt_tab = NULL;

    // find the window being referenced by alt-tab index.
    // this is hacky and may not work perfectly
    int i = 0;
    NT_ITERATE_CHILDREN(window_list) {
        if (i == alt_tab_index) {
            struct taskbar_window *tb = child->priv;
            celestial_setFocusID(tb->window_id, true);
            
        }

        i++;
    }

    alt_tab_visible = false;
    alt_tab_index = 0;
}

void announce_handler(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_WINDOW_CHANGED) {
        celestial_event_window_changed_t *change = event;
        struct taskbar_window *tb = taskbar_get_window(change->changed_window);

        if (change->changed_event == CELESTIAL_WINDOW_CHANGE_ADVERTISED) {
            window_info_t info;
            if (celestial_queryWindow(change->changed_window, &info) != 0) {
                TRACE_ERROR("celestial_queryWindow failed: %s\n", strerror(errno));
                return;
            }

            if (tb != NULL) {
                // tiny hack :)
                nt_image_wdgt_t *img = (nt_image_wdgt_t*)tb->icon;
                img->img = nt_icon_get(info.icon, NULL, 16);
                nt_widget_invalidate(tb->icon);
                nt_label_set_text(tb->label, info.name);
            } else {
                taskbar_add_window(change->changed_window, info.name, info.icon, info.focused);
            }
        } else if (change->changed_event == CELESTIAL_WINDOW_CHANGE_CLOSING) {
            if (!tb) return;
            nt_widget_mark_recalc(tb->w);
            nt_widget_free(tb->w);
            free(tb);
        } else if (change->changed_event == CELESTIAL_WINDOW_CHANGE_FOCUSED) {
            if (!tb) return;
            taskbar_set_highlighted(tb, true);
        } else if (change->changed_event == CELESTIAL_WINDOW_CHANGE_UNFOCUSED) {
            if (!tb) return;
            taskbar_set_highlighted(tb, false);
        }
    }
}

void key_handler(window_t *win, uint32_t event_type, void *event) {
    if (event_type == CELESTIAL_EVENT_BOUND_KEY) {
        celestial_event_bound_key_t *key = event;

        if (key->sc == SCANCODE_F4 && (key->mods & KEYBOARD_MOD_LEFT_ALT)) {
            if (key->pressed == false) return;
            if (key->focused != (wid_t)-1 && taskbar_get_window(key->focused) != NULL) {
                celestial_req_close_window_t close_window = {
                    .magic = CELESTIAL_MAGIC,
                    .size = sizeof(close_window),
                    .type = CELESTIAL_REQ_CLOSE_WINDOW,
                    .wid = key->focused
                };

                celestial_sendRequest(&close_window, sizeof(close_window));
            }
        } else if (key->sc == 't' && key->mods == (KEYBOARD_MOD_LEFT_CTRL | KEYBOARD_MOD_LEFT_ALT)) {
            if (key->pressed == false) return;
            if (!fork()) {
                char *argv[] = { "termemu", NULL }; 
                execvp("termemu", argv);
                exit(1);
            }
        } else if (key->sc == '\t' && key->mods == (KEYBOARD_MOD_LEFT_ALT)) {
            if (key->pressed == false) return;
            if (!alt_tab_visible) {
                alt_tab_visible = true;
                alt_tab_show();
            } else {
                alt_tab_index = (alt_tab_index + 1) % alt_tab_num_windows;
                nt_widget_invalidate(alt_tab->root_frame->children); // anything below a root_frame
                nt_window_update(alt_tab); // << hack
            }
        } else if (key->sc == SCANCODE_LEFT_ALT) {
            if (key->pressed == true) return;
            alt_tab_hide();
        }
    }
}


int main(int argc, char *argv[]) {
    // TODO: Options

    if (chdir("/etc/desktop.d") < 0) {
        TRACE_ERROR("Could not change directory to \"/etc/desktop.d\": %s\n", strerror(errno));
        return 1;
    }

    if (nt_init()) {
        TRACE_ERROR("nt_init failed\n");
        return 1;
    }

    nt_platform_get_display_size(&screen_width, &screen_height);

    // Create the taskbar's window
    taskbar_window = nt_window_create_flags(screen_width, 32, CELESTIAL_WINDOW_FLAG_BLURRED);
    nt_window_set_pos(taskbar_window, 0, screen_height-32);

    // Create the taskbar
    taskbar_create();
    TRACE_DEBUG("taskbar_create succeeded\n");

    // Create the start menu
    start_create();
    TRACE_DEBUG("start_create succeeded\n");

    // Set us at root window
    window_t *win = (window_t*)taskbar_window->platform;
    celestial_setHandler(win, CELESTIAL_EVENT_WINDOW_CHANGED, announce_handler);
    celestial_setHandler(win, CELESTIAL_EVENT_BOUND_KEY, key_handler);
    if (celestial_setRootWindow(win) < 0) {
        TRACE_ERROR("celestial_setRootWindow failed: %s\n", strerror(errno));
    }

    celestial_bindKey(win, SCANCODE_F4, KEYBOARD_MOD_LEFT_ALT, true);
    celestial_bindKey(win, '\t', KEYBOARD_MOD_LEFT_ALT, false);
    celestial_bindKey(win, SCANCODE_LEFT_ALT, 0, false);
    celestial_bindKey(win, 't', KEYBOARD_MOD_LEFT_CTRL | KEYBOARD_MOD_LEFT_ALT, true);

    // Query all the currently available window IDs
    wid_t *wids;
    size_t nwids;
    int r = celestial_queryWindowIDs(&nwids, &wids);
    if (r == 0) {
        // if (nwids > 5) nwids = 5;
        for (unsigned i = 0; i < nwids; i++) {
            window_info_t info;
            if (celestial_queryWindow(wids[i], &info)) {
                continue;
            }

            if (info.flags & CELESTIAL_WINDOW_FLAG_DECORATED) {
                taskbar_add_window(wids[i], info.name, info.icon, info.focused);
            }
        }
    } else {
        TRACE_ERROR("celestial_queryWindowIDs failed: %s\n", strerror(errno));
    }

    nt_loop();
    return 0;
}
