/**
 * @file userspace/desktopmgr/desktopmgr.c
 * @brief Provides the background's icons in the desktop
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#define APP_NAME "desktopmgr"
#include <ethereal/log.h>
#include <ethereal/celestial.h>
#include <neutron/neutron.h>
#include <structs/ini.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

/* Desktop window */
nt_window_t *desktop_window = NULL;
nt_widget_t *desktop_grid = NULL;
char *home = NULL;
int sw, sh;
int gx = 0;
int gy = 0;
int gw = 0;
int gh = 0;

/* protos */
static void refresh();

static bool box_event(nt_widget_t *widget, nt_event_t *event) {
    if (widget->type != NT_WIDGET_BOX) return true; // pass up in chain
    if (event->type == NT_EVENT_FOCUS_ENTER) {
        nt_widget_set_selected(event->w, true);
    } else if (event->type == NT_EVENT_FOCUS_EXIT) {
        nt_widget_set_selected(event->w, false);
        nt_widget_invalidate(event->w);
    } else if (event->type == NT_EVENT_MOUSE_DOUBLE_CLICK) {
        // if children are double-clicked we want to capture that event!
        NT_DEBUG("Launch %s\n", widget->priv);

        if (!fork()) {
            char *argv[] = { "/bin/sh", "-c", widget->priv, NULL };
            execv("/bin/sh", argv);
            TRACE_ERROR("Error launching \"%s\": %s\n", widget->priv, strerror(errno));
            exit(1);
        }
    }

    return false;
}

void _context_menu_open(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (!fork()) {
        char *argv[] = { "/bin/sh", "-c", w->parent->priv, NULL };
        execv("/bin/sh", argv);
        TRACE_ERROR("Error launching \"%s\": %s\n", w->parent->priv, strerror(errno));
        exit(1);
    }
}

void _context_menu_copy(nt_widget_t *w, nt_signal_t *sig, void *data) {
    system("show-dialog --text=\"Not implemented :(\" --title=\"Error\" --error");
}

void _context_menu_paste(nt_widget_t *w, nt_signal_t *sig, void *data) {
    system("show-dialog --text=\"Not implemented :(\" --title=\"Error\" --error");
}

void _context_menu_delete(nt_widget_t *w, nt_signal_t *sig, void *data) {
    system("show-dialog --text=\"Not implemented :(\" --title=\"Error\" --error");   
}

void _context_menu_new_folder_sig(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_widget_t *inp = (nt_widget_t*)data;
    char *text = nt_input_get_text(inp);

    NT_DEBUG("Create folder %s\n", text);
    if (mkdir(text, 0755) < 0) {
        // Show error dialog
        // TODO: show actual error
        char txt[256]; snprintf(txt, 256, "Create folder failed: %s", strerror(errno));
        nt_dialog_t *d = nt_dialog_create_message(desktop_window, "Error", txt, NT_DIALOG_MSG_TYPE_ERROR, NT_DIALOG_MSG_BTNS_OK);
        nt_dialog_open(d);
    }
}

void _context_menu_new_folder(nt_widget_t *w, nt_signal_t *sig, void *data) {
    // HACK: update the main window to allow the menu to be unrendered
    nt_window_update(nt_widget_get_window(w));

    nt_dialog_t *dlg = nt_dialog_create_message(desktop_window, "New Folder", "Enter folder name.", NT_DIALOG_MSG_TYPE_QUESTION, NT_DIALOG_MSG_BTNS_NONE);
    nt_widget_t *inp = nt_input_create("Folder Name");
    nt_style_set_margin_all(&dlg->content_box->children->style, 2); // lmao
    nt_style_set_margin_all(&inp->style, 2);
    nt_widget_set_expansion(inp, NT_EXPAND_HORIZONTAL);
    nt_box_append(dlg->content_box, inp);
    
    nt_widget_t *ok = nt_dialog_add_button(dlg, "OK", NT_DIALOG_BTN_OK);
    nt_signal_connect(ok, "pressed", _context_menu_new_folder_sig, inp);

    nt_dialog_add_button(dlg, "Cancel", NT_DIALOG_BTN_CANCEL);
    nt_dialog_open(dlg);
    refresh();
}

void _context_menu_refresh(nt_widget_t *w, nt_signal_t *sig, void *data) {
    refresh();
}

void _context_menu_open_terminal(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (!fork()) {
        char *argv[] = { "termemu", NULL };
        execvp("termemu", argv);
        exit(1);
    }
}

void _context_menu_change_wallpaper(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (!fork()) {
        char *argv[] = { "wallpaper-chooser", NULL };
        execvp("wallpaper-chooser", argv);
        exit(1);
    }
}

static void add_launch_item(char *name, char *icon, char *exec) {
    if (gx >= gw) return;

    nt_widget_t *widget = nt_box_create_vertical();
    nt_widget_set_selectable(widget, true);
    nt_widget_set_focusable(widget, true);
    nt_style_set_suggested_width(&widget->style, 120);
    nt_style_set_margin(&widget->style, LEFT, 20);
    nt_style_set_bg_color(&widget->style, NT_COLOR(0,0,0,0));

    nt_event_set_handler(widget, NT_EVENT_FOCUS_ENTER, box_event);
    nt_event_set_handler(widget, NT_EVENT_FOCUS_EXIT, box_event);
    nt_event_set_handler(widget, NT_EVENT_MOUSE_DOUBLE_CLICK, box_event);

    nt_widget_t *img = nt_image_create_from_image(nt_icon_get(icon, NULL, 48));
    nt_style_set_margin_all(&img->style, 4);
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(img, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_widget_set_horizontal_alignment(img, NT_ALIGN_CENTER);

    nt_widget_t *lbl = nt_label_create(name);
    nt_label_set_glow(lbl, true);
    nt_style_set_margin_all(&lbl->style, 4);
    nt_style_set_font(&lbl->style, NT_SANS_12);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_style_set_fg_color(&lbl->style, NT_COLOR(0xFA,0xFA,0xFA,255));
    nt_style_set_select_invert(&lbl->style, false);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(lbl, NT_ALIGN_CENTER);

    nt_box_append(widget, img);
    nt_box_append(widget, lbl);

    nt_widget_t *ctx = nt_menu_create();
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Open", _context_menu_open, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Copy", _context_menu_copy, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Paste", _context_menu_paste, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Delete", _context_menu_paste, NULL));
    nt_menu_add_item(ctx,nt_menu_item_create_button_cb("Refresh", _context_menu_refresh, NULL));
    nt_menu_add_item(ctx,nt_menu_item_create_button_icon_cb("Open in Terminal", nt_icon_get("terminal", NULL, 16), _context_menu_open_terminal, NULL));
    nt_widget_set_context_menu(widget, ctx);

    widget->priv = strdup(exec);
    nt_grid_set(desktop_grid, gx, gy, widget);

    gy += 1;
    if (gy >= gh) {
        gy = 0;
        gx += 1;
    }
}

static void add_item(struct dirent *ent) {
    if (ent->d_type != DT_DIR && ent->d_type != DT_REG) {
        return;
    }

    // TODO: sort these items
    char *name = ent->d_name;
    char *icon = "missing";
    char exec[1024];
    strncpy(exec, "show-dialog --error --text=\"Cannot open this file, unknown extension\" --title=\"Error\"", 1024);

    if (ent->d_type == DT_DIR) {
        icon = "folder";
        snprintf(exec, 1024, "file-browser %s", home, ent->d_name);
    } else {
        icon = "file";

        char *extension = strchr(name, '.');
        if (extension) {
            *extension = 0;
            extension++;

            if (!strcmp(extension, "txt")) {
                icon = "text";
                // snprintf(exec, 1024, "text-editor %s", ent->d_name);
            } else if (!strcmp(extension, "jpg") || !strcmp(extension, "bmp") || !strcmp(extension, "png")) {
                icon = "image";
                snprintf(exec, 1024, "image-viewer %s", ent->d_name);
            } else {
                icon = "file";
            }
        }
    }

    add_launch_item(name, icon, exec);
}

static int sort_fn(const void *s1, const void *s2) {
    return strcmp(*(const char**)s1, *(const char**)s2);
}

static void refresh() {
    if (desktop_grid) {
        NT_ITERATE_CHILDREN(desktop_grid) {
            char *p = child->priv;
            child->priv = NULL;
            free(p);
        }

        nt_widget_free(desktop_grid);
    }
    
    desktop_grid = nt_grid_create(sw / 64, sh / 64);
    nt_widget_set_expansion(desktop_grid, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_bg_color(&desktop_grid->style, NT_COLOR(0,0,0,0));
    nt_box_append(desktop_window->root_frame, desktop_grid);

    // begin the parsing
    home = getenv("HOME");
    if (!home) {
        TRACE_WARN("$HOME is not set, assuming /root/\n");
        home = "/root/";
    }

    char path[512];
    snprintf(path, 512, "%s/Desktop/", home);

    if (chdir(path) < 0) {
        TRACE_ERROR("chdir to %s failed: %s\n", path, strerror(errno));
        return;
    }

    DIR *dirp = opendir(path);
    if (!dirp) {
        TRACE_WARN("Could not open user desktop directory \"%s\": %s\n", path, strerror(errno));
        return;
    }

    // First look for launchers as they take priority
    int num_launchers = 0;
    char **launchers = NULL;
    struct dirent *ent;
    while ((ent = readdir(dirp))) {
        if (ent->d_name[0] == '.') continue;
        
        if (ent->d_type == DT_REG) {
            // could be a launch, get extension
            char *ext = strchr(ent->d_name, '.');
            if (ext) {
                ext++;
                if (!strcmp(ext, "launch")) {
                    // This is a launcher!
                    launchers = realloc(launchers, (num_launchers+1) * sizeof(char*));
                    launchers[num_launchers++] = strdup(ent->d_name);
                }
            }
        }
    }

    if (num_launchers > 0) {
        qsort(launchers, num_launchers, sizeof(char*), sort_fn);

        for (int i = 0; i < num_launchers; i++) {
            ini_t *ini = ini_load(launchers[i]);
            if (!ini) {
                TRACE_WARN("Error loading launcher %s (%s)\n", launchers[i], strerror(errno));
                continue;
            }

            char *name = ini_get(ini, "Application", "Name");
            char *exec = ini_get(ini, "Application", "Exec");
            char *icon = ini_get(ini, "Application", "Icon");

            if (!name) {
                TRACE_WARN("Missing Name entry in %s\n", launchers[i]);
                name = "Bad INI file";
            }

            if (!exec) {
                TRACE_WARN("Missing Exec entry in %s\n", launchers[i]);
                exec = "echo error";
            }

            if (!icon) {
                TRACE_WARN("Missing Icon entry in %s\n", launchers[i]);
                icon = "missing";
            }

            add_launch_item(name, icon, exec);

            ini_destroy(ini);
        }
    }

    // Process the remaining files
    rewinddir(dirp);
    while ((ent = readdir(dirp))) {
        if (ent->d_name[0] == '.') continue;
        
        // check for launch extension
        char *ext = strchr(ent->d_name, '.');
        if (ext && !strcmp(ext, ".launch")) {
            continue;
        }

        add_item(ent);
    }
}


int main(int argc, char *argv[]) {
    TRACE_INFO("desktopmgr 1.0.0\n");

    if (nt_init() != 0) {
        TRACE_ERROR("nt_init failed\n");
        return 1;
    }

    nt_platform_get_display_size(&sw, &sh);
    sh -= 32; // taskbar width

    desktop_window = nt_window_create_undecorated(sw, sh);
    celestial_setZArray(desktop_window->platform, CELESTIAL_Z_BACKGROUND);
    nt_window_set_pos(desktop_window, 0, 0);
    nt_platform_set_window_transparent(desktop_window);

    // Create the root box
    nt_widget_t *b = nt_box_create_vertical();
    nt_window_set_root(desktop_window, b);
    nt_style_set_bg_color(&b->style, NT_COLOR(0,0,0,0));;
    nt_widget_set_expansion(b, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);

    // Create the context menu
    nt_widget_t *context_menu = nt_menu_create();
    nt_menu_add_item(context_menu,nt_menu_item_create_button_icon_cb("New Folder", nt_icon_get("folder", NULL, 16), _context_menu_new_folder, NULL));
    nt_menu_add_item(context_menu,nt_menu_item_create_separator());
    nt_menu_add_item(context_menu,nt_menu_item_create_button_icon_cb("Change Wallpaper", nt_icon_get("ethereal", NULL, 16), _context_menu_change_wallpaper, NULL));
    nt_menu_add_item(context_menu,nt_menu_item_create_button_cb("Refresh", _context_menu_refresh, NULL));
    nt_menu_add_item(context_menu,nt_menu_item_create_button_icon_cb("Open in Terminal", nt_icon_get("terminal", NULL, 16), _context_menu_open_terminal, NULL));
    nt_widget_set_context_menu(b, context_menu);

    gw = sw / 64;
    gh = sh / 64;

    refresh();
    nt_loop();
    return 0;
}
