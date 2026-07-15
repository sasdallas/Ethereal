/**
 * @file file_browser/file_browser.c
 * @brief Ethereal file browser
 * 
 * @warning The code in this file browser is ass.
 *          I wrote it while developing Neutron out of a "can I quickly farm engagement by posting this"
 *          It is designed to look good on the outside, not the inside.
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <neutron/neutron.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define VIEW_MODE_COLUMN 0
#define VIEW_MODE_ICON 1

#define VIEW_ICON_SIZE ((file_browser_view_mode == VIEW_MODE_ICON) ? 32 : 16)

char file_browser_path[PATH_MAX] = { 0 };
unsigned char file_browser_view_mode = VIEW_MODE_COLUMN;

nt_window_t *file_browser_window = NULL;
nt_widget_t *scroll_container = NULL;
nt_widget_t *current_view = NULL;
nt_widget_t *path_input = NULL;
nt_widget_t *status_lbl = NULL;
nt_widget_t *sidebar = NULL;

nt_image_t *file_icon_32;
nt_image_t *folder_icon_32;
nt_image_t *file_icon;
nt_image_t *folder_icon;
nt_image_t *back_icon;
nt_image_t *fwd_icon;
nt_image_t *up_icon;
nt_image_t *home_icon;

typedef struct fb_entry {
    char d_name[256];
    struct stat st;
    nt_image_t *icon;
    char launch_cmd[512];
} fb_entry_t;

/* entry list */
fb_entry_t *fb_ent_list = NULL;

/* count of entries */
int fb_ent_count = 0;

/* count of folders */
int fb_ent_count_folders = 0;

/* current selected */
fb_entry_t *selected_entry = NULL;

/* protos galore */
void item_deselected(nt_widget_t *w, nt_signal_t *sig, void *data);
void item_selected(nt_widget_t *w, nt_signal_t *sig, void *data);
void double_click_event(nt_widget_t *w, nt_signal_t *sig, void *data);
void item_deselected_icon(nt_widget_t *w, nt_signal_t *sig, void *data);
void item_selected_icon(nt_widget_t *w, nt_signal_t *sig, void *data);
void double_click_event_icon(nt_widget_t *w, nt_signal_t *sig, void *data);
void home_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void up_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void back_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void forward_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void column_view_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void icon_view_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);

/* context menu */
void _context_menu_open(nt_widget_t *w, nt_signal_t *sig, void *data);
void _context_menu_open_terminal(nt_widget_t *w, nt_signal_t *sig, void *data);

// error out
void error_nonfatal(char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 256, fmt, ap);
    va_end(ap);

    nt_dialog_t *err_dlg = nt_dialog_create_message(file_browser_window, "Error", buffer, NT_DIALOG_MSG_TYPE_ERROR, NT_DIALOG_BTN_OK);
    nt_dialog_open(err_dlg);
}

// non-fatal perror
#define TRIGGER_ERROR_PERROR(msg) error_nonfatal("(line %d) In function %s:\n%s: %s", __LINE__, __FUNCTION__, msg, strerror(errno))


int file_sort_fn(const void *s1, const void *s2) {
    fb_entry_t *f1 = (fb_entry_t*)s1;
    fb_entry_t *f2 = (fb_entry_t*)s2;

    // Handle the case where a file and a directory are beign compared
    if (S_ISDIR(f1->st.st_mode) && !(S_ISDIR(f2->st.st_mode))) {
        return -1;
    } else if (!(S_ISDIR(f1->st.st_mode)) && S_ISDIR(f2->st.st_mode)) {
        return 1;
    }

    return strcmp(f1->d_name, f2->d_name);
}

void find_launcher(fb_entry_t *ent) {
    if (S_ISDIR(ent->st.st_mode)) {
        // Directories will always be entered
        ent->icon = (file_browser_view_mode == VIEW_MODE_ICON) ? folder_icon_32 : folder_icon;
        return;
    }

    // If it is executable we will just execute
    if (ent->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        snprintf(ent->launch_cmd, 256, "exec ./%s", ent->d_name);
        ent->icon = nt_icon_get("application", NULL, VIEW_ICON_SIZE);
        return;
    }

    // Else we dunno
    ent->icon = nt_icon_get("file", NULL, VIEW_ICON_SIZE);
    ent->launch_cmd[0] = 0;
}

void collect_files() {
    if (fb_ent_list) { free(fb_ent_list); fb_ent_list = NULL; }
    fb_ent_count = 0;
    fb_ent_count_folders = 0;

    DIR *dirp = opendir(".");
    if (!dirp) { TRIGGER_ERROR_PERROR("opendir"); return; }
    
    struct dirent *ent;
    while ((ent = readdir(dirp)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }

        // Reallocate the array (TODO slow-ish)
        fb_ent_list = realloc(fb_ent_list, (fb_ent_count+1) * sizeof(fb_entry_t));
        fb_entry_t *fent = &fb_ent_list[fb_ent_count];
        strncpy(fent->d_name,  ent->d_name, 256);
        
        if (stat(ent->d_name, &fent->st) < 0) {
            TRIGGER_ERROR_PERROR("stat");
            return;
        }

        if (S_ISDIR(fent->st.st_mode)) {
            fb_ent_count_folders++;
        }

        find_launcher(fent);

        fb_ent_count++;
    }

    closedir(dirp);

    // Sort the array
    qsort(fb_ent_list, fb_ent_count, sizeof(fb_entry_t), file_sort_fn);
}


// helper to open applications
void open_application(fb_entry_t *ent) {
    if (S_ISDIR(ent->st.st_mode)) {
        if (chdir(ent->d_name) < 0) {
            TRIGGER_ERROR_PERROR("chdir");
        }

        return;
    }

    pid_t cpid = fork();
    if (!cpid) {
        chdir(file_browser_path);
        char *argv[] = { "/bin/sh", "-c", ent->launch_cmd, NULL };
        execvp("/bin/sh", (char *const *)argv);
        exit(1);
    }
}

// about dialog
void about_pressed(nt_widget_t *w, nt_signal_t *s, void *data) {
    nt_dialog_t *dlg = nt_dialog_create(file_browser_window, "About File Browser", 350, 190);
    
    nt_widget_t *img = nt_image_create_from_image(nt_icon_get("folder", NULL, 48));
    nt_widget_set_expansion(img, NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(img, NT_ALIGN_CENTER);
    nt_box_append(dlg->content_box, img);

    nt_widget_t *lbl = nt_label_create("File Browser");
    nt_style_set_font(&lbl->style, NT_SANS_BOLD_12);

    nt_style_set_margin(&lbl->style, BOTTOM, 0);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(lbl, NT_ALIGN_CENTER);
    nt_box_append(dlg->content_box, lbl);

    lbl = nt_label_create("Version 1.0.0 (Built " __DATE__ ")");
    nt_style_set_font(&lbl->style, NT_SANS_12);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL);
    nt_widget_set_horizontal_alignment(lbl, NT_ALIGN_CENTER);
    nt_box_append(dlg->content_box, lbl);

    nt_dialog_add_button(dlg, "OK", 0);
    nt_dialog_open(dlg);
}

nt_widget_t *create_menubar() {
    nt_widget_t *menubar = nt_menubar_create();

    // Create file menu
    nt_widget_t *file_menu = nt_menu_create();
    nt_widget_t *new_folder_btn = nt_menu_item_create_button("New Folder"); 
    nt_widget_t *exit_btn = nt_menu_item_create_button("Exit");
    nt_menu_add_item(file_menu, new_folder_btn);
    nt_menu_add_item(file_menu, nt_menu_item_create_separator());
    nt_menu_add_item(file_menu, exit_btn);
    
    // Create edit menu
    nt_widget_t *edit_menu = nt_menu_create();
    nt_widget_t *copy_btn = nt_menu_item_create_button("Copy");
    nt_widget_t *paste_btn = nt_menu_item_create_button("Paste");
    nt_menu_add_item(edit_menu, copy_btn);
    nt_menu_add_item(edit_menu, paste_btn);

    // Create view menu
    nt_widget_t *view_menu = nt_menu_create();
    nt_widget_t *column_view_btn = nt_menu_item_create_button_icon("Column View", NULL);
    nt_widget_t *icon_view_btn = nt_menu_item_create_button_icon("Icon View (beta)", NULL);
    
    nt_signal_connect(column_view_btn, "pressed", column_view_pressed, NULL);
    nt_signal_connect(icon_view_btn, "pressed", icon_view_pressed, NULL);

    nt_menu_add_item(view_menu, column_view_btn);
    nt_menu_add_item(view_menu, icon_view_btn);

    // Create help menu
    nt_widget_t *help_menu = nt_menu_create();
    nt_widget_t *help = nt_menu_item_create_button_icon("Help", nt_icon_get("missing", NULL, 16));
    nt_widget_t *about = nt_menu_item_create_button_icon("About", folder_icon);

    nt_signal_connect(about, "pressed", about_pressed, NULL);

    nt_menu_add_item(help_menu, help);
    nt_menu_add_item(help_menu, nt_menu_item_create_separator());
    nt_menu_add_item(help_menu, about);

    // Append
    nt_menubar_append(menubar, "File", file_menu);
    nt_menubar_append(menubar, "Edit", edit_menu);
    nt_menubar_append(menubar, "View", view_menu);
    nt_menubar_append(menubar, "Help", help_menu);

    return menubar;
}

static nt_widget_t *button_from_image(nt_image_t *img) {
    nt_widget_t *img_wdgt = nt_image_create_from_image(img);
    nt_style_set_margin_all(&img_wdgt->style, 2);
    nt_style_set_bg_color(&img_wdgt->style, NT_COLOR(0,0,0,0));

    nt_widget_t *btn = nt_button_create(img_wdgt);
    nt_style_set_bg_color(&btn->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&btn->style, 2);
    return btn;
}

nt_widget_t *create_toolbar() {
    nt_widget_t *toolbar = nt_box_create_horizontal();
    nt_style_set_bg_color(&toolbar->style, NT_COLOR(0xde,0xde,0xde,0xff));
    nt_style_set_margin_all(&toolbar->style, 0);
    nt_widget_set_expansion(toolbar, NT_EXPAND_HORIZONTAL);
    
    nt_widget_t *back = button_from_image(back_icon);
    nt_box_append(toolbar, back);
    nt_widget_t *fwd = button_from_image(fwd_icon);
    nt_box_append(toolbar, fwd);
    nt_widget_t *up = button_from_image(up_icon);
    nt_box_append(toolbar, up);
    nt_widget_t *home = button_from_image(home_icon);
    nt_box_append(toolbar, home);

    nt_signal_connect(back, "pressed", back_pressed, NULL);
    nt_signal_connect(fwd, "pressed", forward_pressed, NULL);
    nt_signal_connect(up, "pressed", up_pressed, NULL);
    nt_signal_connect(home, "pressed", home_pressed, NULL);

    path_input = nt_input_create(NULL);
    nt_style_set_bg_color(&path_input->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&path_input->style, 1);
    nt_widget_set_expansion(path_input, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(path_input, NT_ALIGN_CENTER);
    nt_input_set_text(path_input, file_browser_path);

    nt_box_append(toolbar, path_input);

    return toolbar;
}

void to_normal_size(char *output, uint64_t bytes) {
    const char *suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    int i = 0;
    double double_bytes = (double)bytes;

    while (double_bytes >= 1024 && i < 6) {
        double_bytes /= 1024.0;
        i++;
    }
    
    snprintf(output,64, "%.2f %s", double_bytes, suffixes[i]);
} 

nt_widget_t *name_factory(void *data, void *ctx) {
    fb_entry_t *ent = (fb_entry_t*)data;
    nt_widget_t *box_mini = nt_box_create_horizontal();
    nt_style_set_margin_all(&box_mini->style, 0);
    nt_widget_set_expansion(box_mini, NT_EXPAND_HORIZONTAL);
    nt_widget_set_selectable(box_mini, true);

    nt_widget_t *img = nt_image_create_from_image(ent->icon);
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&img->style, 4);
    nt_box_append(box_mini, img);

    nt_widget_t *lbl = nt_label_create(ent->d_name);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_widget_set_vertical_alignment(lbl, NT_ALIGN_CENTER);
    nt_style_set_margin_all(&lbl->style, 4);
    nt_box_append(box_mini, lbl);
    return box_mini;
}

nt_widget_t *other_factory(void *data, void *ctx) {
    nt_widget_t *lbl = nt_label_create((char*)data);
    nt_style_set_margin_all(&lbl->style, 4);
    return lbl;
}

nt_widget_t *create_context_menu() {
    nt_widget_t *ctx = nt_menu_create();
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon("New Folder", nt_icon_get("folder", NULL, 16)));
    nt_menu_add_item(ctx, nt_menu_item_create_button("Paste"));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon("Open in Terminal", nt_icon_get("terminal", NULL, 16)));
    return ctx;
}

nt_widget_t *create_context_menu_item() {
    nt_widget_t *ctx = nt_menu_create();
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Open", _context_menu_open, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon_cb("Open with Terminal", nt_icon_get("terminal", NULL, 16), _context_menu_open_terminal, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon("Edit with Text Editor", nt_icon_get("file", NULL, 16)));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button("Copy"));
    nt_menu_add_item(ctx, nt_menu_item_create_button("Paste"));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());

    nt_menu_add_item(ctx, nt_menu_item_create_button("Rename"));
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon("Delete", nt_icon_get("ethereal", NULL, 16)));
    return ctx;
    
}

nt_widget_t *create_col_view() {
    nt_widget_t *col_view = nt_column_view_create();
    nt_widget_set_context_menu(col_view, create_context_menu());
    nt_style_set_margin_all(&col_view->style, 0);
    nt_style_set_padding_all(&col_view->style,1);
    nt_widget_set_expansion(col_view, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_column_view_append_column(col_view, nt_column_create("Name", name_factory, NULL));
    nt_column_view_append_column(col_view, nt_column_create("Size", other_factory, NULL));
    nt_column_view_append_column(col_view, nt_column_create("Date Modified", other_factory, NULL));


    for (int i = 0; i < fb_ent_count; i++) {
        fb_entry_t *ent = &fb_ent_list[i];

        char tmp[64];
        to_normal_size(tmp, ent->st.st_size);

        NT_CREATE_ROW_DATA(file_data, 3);
            NT_ADD_ROW_DATA(file_data, ent);
            NT_ADD_ROW_DATA(file_data, tmp);
            NT_ADD_ROW_DATA(file_data, "Idk");
            nt_widget_t *file_wdgt = nt_column_view_add_row(col_view, file_data);
            nt_widget_set_context_menu(file_wdgt, create_context_menu_item());
            nt_signal_connect(file_wdgt, "double-clicked", double_click_event, ent);
            nt_signal_connect(file_wdgt, "selected", item_selected, ent);
            nt_signal_connect(file_wdgt, "deselected", item_deselected, ent);
        NT_FINISH_ROW_DATA(file_data);
    }

    nt_scroll_container_set_child(scroll_container, col_view);
    if (current_view) nt_widget_free(current_view);
    current_view = col_view;
    return col_view;
}

nt_widget_t *create_icon_view() {
    nt_widget_t *icon_view = nt_icon_view_create();
    nt_widget_set_context_menu(icon_view, create_context_menu());

    nt_style_set_margin_all(&icon_view->style, 0);
    nt_style_set_padding_all(&icon_view->style,1);
    nt_widget_set_expansion(icon_view, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);

    nt_icon_store_t *store = nt_icon_store_create();

    for (int i = 0; i < fb_ent_count; i++) {
        nt_icon_store_append(store, fb_ent_list[i].d_name, fb_ent_list[i].icon, &fb_ent_list[i]);
    }

    nt_icon_view_set_store(icon_view, store);

    // !!! why not conform
    nt_signal_connect(icon_view, "icon-selected", item_selected_icon, NULL);
    nt_signal_connect(icon_view, "icon-deselected", item_deselected_icon, NULL);
    nt_signal_connect(icon_view, "icon-double-clicked", double_click_event_icon, NULL);

    // !!! this is stupid
    NT_ITERATE_CHILDREN(icon_view) {
        nt_widget_set_context_menu(child, create_context_menu_item());
    }

    nt_scroll_container_set_child(scroll_container, icon_view);
    if (current_view) nt_widget_free(current_view);
    current_view = icon_view;
    return icon_view;
}

nt_widget_t *create_view() {
    collect_files();
    
    if (file_browser_view_mode == VIEW_MODE_COLUMN) {
        return create_col_view();
    } else {
        return create_icon_view();
    }
}

// todo support non-$HOME entries
nt_widget_t *create_sidebar_entry(char *name, nt_image_t *icon) {
    nt_widget_t *box = nt_box_create_horizontal();
    nt_style_set_suggested_width(&box->style, 100);
    nt_style_set_bg_color(&box->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&box->style, 0);
    nt_widget_set_expansion(box, NT_EXPAND_HORIZONTAL);
    nt_widget_t *img = nt_image_create_from_image(icon);
    nt_widget_set_expansion(img, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&img->style, 2);
    nt_box_append(box, img);
    nt_widget_t *lbl = nt_label_create(name);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_font(&lbl->style, NT_SANS_12);
    nt_style_set_margin_all(&lbl->style, 2);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_box_append(box, lbl);
    return box;
}


nt_widget_t *create_sidebar() {
    sidebar = nt_list_view_create();
    nt_widget_set_expansion(sidebar, NT_EXPAND_VERTICAL);
    nt_list_view_append(sidebar, create_sidebar_entry("Desktop", folder_icon));
    nt_style_set_margin_all(&sidebar->style, 0);
    return sidebar;
}

void load_icons() {
    file_icon = nt_icon_get("file", NULL, 16);
    folder_icon = nt_icon_get("folder", NULL, 16);
    folder_icon_32 = nt_icon_get("folder", NULL, 32);
    file_icon_32 = nt_icon_get("file", NULL, 32);

    back_icon = nt_icon_get("missing", NULL, 16);
    fwd_icon = nt_icon_get("missing", NULL, 16);
    up_icon = nt_icon_get("missing", NULL, 16);
    home_icon = nt_icon_get("missing", NULL, 16);
}

void update_status_lbl_dir() {
    char tmp_buf[128];
    snprintf(tmp_buf, 128, "%d folders, %d files", fb_ent_count_folders, fb_ent_count - fb_ent_count_folders);
    nt_label_set_text(status_lbl, tmp_buf);
}

void update_status_lbl_selected(fb_entry_t *ent) {
    char size_buf[64];
    to_normal_size(size_buf, ent->st.st_size);

    char tmp_buf[128];
    snprintf(tmp_buf, 128, "\"%s\" (%s)", ent->d_name, size_buf);
    nt_label_set_text(status_lbl, tmp_buf);
}

void update_everything() {
    selected_entry = NULL;
    getcwd(file_browser_path, PATH_MAX);
    create_view();
    nt_input_set_text(path_input, file_browser_path);
    update_status_lbl_dir();
}

void double_click_event(nt_widget_t *w, nt_signal_t *sig, void *data) {
    fb_entry_t *ent = (fb_entry_t*)data;
    open_application(ent);
    update_everything();
}

void home_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char *home = getenv("HOME");
    if (!home) home = "/root/";
    update_everything();
}

void up_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    chdir("..");
    update_everything();
}

void back_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_dialog_t *dlg = nt_dialog_create_message(nt_widget_get_window(w), "Error", "Sorry, this is not implemented", NT_DIALOG_MSG_TYPE_ERROR, NT_DIALOG_MSG_BTNS_OK);
    nt_dialog_open(dlg);
}

void forward_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_dialog_t *dlg = nt_dialog_create_message(nt_widget_get_window(w), "Error", "Sorry, this is not implemented", NT_DIALOG_MSG_TYPE_ERROR, NT_DIALOG_MSG_BTNS_OK);
    nt_dialog_open(dlg);
}


void item_selected(nt_widget_t *w, nt_signal_t *sig, void *data) {
    selected_entry = (fb_entry_t*)data;
    update_status_lbl_selected((fb_entry_t*)data);
}

void item_deselected(nt_widget_t *w, nt_signal_t *sig, void *data) {
    // !!! cant set selected_entry to NULL here!
    update_status_lbl_dir();
}


void item_deselected_icon(nt_widget_t *w, nt_signal_t *sig, void *data) {
    // !!! cant set selected_entry to NULL here!
    update_status_lbl_dir();
}

void item_selected_icon(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_icon_entry_t *ent = NT_SIGNAL_DATA(sig);
    fb_entry_t *fbent = ent->priv;
    selected_entry = (fb_entry_t*)fbent;
    update_status_lbl_selected(fbent);
}

void double_click_event_icon(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_icon_entry_t *ent = NT_SIGNAL_DATA(sig);
    fb_entry_t *fbent = ent->priv;
    open_application(fbent);
    update_everything();
}

void column_view_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (file_browser_view_mode == VIEW_MODE_COLUMN) return;
    file_browser_view_mode = VIEW_MODE_COLUMN;
    update_everything();
}

void icon_view_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (file_browser_view_mode == VIEW_MODE_ICON) return;
    file_browser_view_mode = VIEW_MODE_ICON;
    update_everything();
}

void _context_menu_open(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (selected_entry) {
        open_application(selected_entry); 
    }
}
 
void _context_menu_open_terminal(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (selected_entry) {
        if (!fork()) {
            chdir(file_browser_path);

        #ifdef __ETHEREAL__
            char *term_name = "termemu";
        #else
            char *term_name = "kitty";
        #endif
            
            char *temp_argv[] = { term_name, selected_entry->launch_cmd, NULL };
            execvp(term_name, (char *const*)temp_argv);
            
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    // begin at cwd
    getcwd(file_browser_path, PATH_MAX);

    nt_init();
    load_icons();
    file_browser_window = nt_window_create("File Explorer", 800, 600);

    nt_widget_t *box = nt_box_create_vertical();
    nt_widget_set_expansion(box, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&box->style, 0);
    
    nt_widget_t *menubar = create_menubar();
    nt_box_append(box, menubar);

    nt_widget_t *toolbar = create_toolbar();
    nt_box_append(box, toolbar);

    nt_widget_t *content = nt_box_create_horizontal();
    nt_style_set_margin_all(&content->style, 0);
    nt_widget_set_expansion(content, NT_EXPAND_VERTICAL | NT_EXPAND_HORIZONTAL);

    // create the primary list view
    nt_box_append(content, create_sidebar());;

    // create the main file viewer
    nt_widget_t *main_box = nt_box_create_vertical();
    nt_style_set_margin_all(&main_box->style, 0);
    nt_widget_set_expansion(main_box, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);

    // Create the scrollable content
    scroll_container = nt_scroll_container_create();
    nt_style_set_padding_all(&scroll_container->style, 1);
    nt_style_set_margin_all(&scroll_container->style, 0);
    nt_widget_set_expansion(scroll_container, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_border_thickness(&scroll_container->style, 1);
    nt_style_set_border_color(&scroll_container->style, NT_COLOR(170,170,170,255));
    nt_style_set_border_rounded(&scroll_container->style, 0);
    nt_scroll_container_set_child(scroll_container, create_view());
    nt_box_append(main_box, scroll_container);

    // Create the status label
    status_lbl = nt_label_create("I am the status label");
    nt_style_set_padding_all(&status_lbl->style, 5);
    nt_style_set_margin_all(&status_lbl->style, 0);
    nt_widget_set_expansion(status_lbl, NT_EXPAND_HORIZONTAL);
    nt_style_set_bg_color(&status_lbl->style, NT_COLOR(0xde,0xde,0xde,0xff));
    nt_box_append(main_box, status_lbl);

    update_status_lbl_dir();

    nt_box_append(content, main_box);
    nt_box_append(box, content);

    nt_window_set_root(file_browser_window, box);
    nt_loop();
    return 0;
}
