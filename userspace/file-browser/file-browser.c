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
#include <getopt.h>
#include <fcntl.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define VIEW_MODE_COLUMN 0
#define VIEW_MODE_ICON 1

#define VIEW_ICON_SIZE ((file_browser_view_mode == VIEW_MODE_ICON) ? 32 : 16)

char file_browser_path[PATH_MAX] = { 0 };
unsigned char file_browser_view_mode = VIEW_MODE_ICON;

nt_window_t *file_browser_window = NULL;
nt_widget_t *scroll_container = NULL;
nt_widget_t *current_view = NULL;
nt_widget_t *path_input = NULL;
nt_widget_t *status_lbl = NULL;
nt_widget_t *sidebar = NULL;

/* History and pins */
#define FB_HISTORY_MAX 64
#define FB_PIN_MAX 64

static char *history[FB_HISTORY_MAX];
static int history_count = 0, history_pos = -1;
static char *pins[FB_PIN_MAX];
static int pin_count = 0;
static char **clipboard = NULL;
static size_t clipboard_count = 0;

/* Icons */
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

typedef struct {
    char **paths;
    size_t count;
} fb_paths_t;

typedef struct {
    nt_widget_t *input;
    char *name;
    nt_dialog_t *dlg;
} fb_prompt_t;

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
void icon_selection_changed(nt_widget_t *w, nt_signal_t *sig, void *data);
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
void new_folder_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void copy_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void paste_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void delete_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void pin_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void path_enter_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void exit_pressed(nt_widget_t *w, nt_signal_t *sig, void *data);
void update_everything(void);
static int navigate_to(const char *path, bool add_history);
static void rebuild_sidebar(void);

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

// Helper to join some paths
static int path_join(char *out, size_t cap, const char *parent, const char *name) {
    int n = snprintf(out, cap, "%s/%s", parent, name);
    if (n < 0 || (size_t)n >= cap) { return -1; }
    return 0;
}

// Filename from a path
static const char *path_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Copy helper
static int copy_tree(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) < 0) {
        error_nonfatal("lstat(%s) failed: %s\n", src, strerror(errno));
        return -1;
    }

    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t len = readlink(src, target, sizeof(target) - 1);
        if (len < 0) {
            error_nonfatal("readlink(%s) failed: %s\n", src, strerror(errno));
            return -1;
        }

        target[len] = 0;
        if (symlink(target, dst) < 0) {
            error_nonfatal("symlink(%s) failed: %s\n", dst, strerror(errno));
            return -1;
        }

        return 0;
    }

    if (S_ISDIR(st.st_mode)) {
        size_t len = strlen(src);
        if (!strncmp(dst, src, len) && dst[len] == '/') {
            error_nonfatal("Bad pathing %s %s\n", src, dst);
            return -1;
        }

        if (mkdir(dst, st.st_mode & 0777) < 0) {
            error_nonfatal("mkdir(%s) failed: %s\n", dst, strerror(errno));
            return -1;
        }

        DIR *dir = opendir(src);
        if (dir == NULL) {
            error_nonfatal("opendir(%s) failed: %s\n", src, strerror(errno));
            return -1;
        }

        struct dirent *ent;
        int result = 0;
        while ((ent = readdir(dir))) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
                continue;
            }
            
            char child_src[PATH_MAX];
            if (path_join(child_src, sizeof(child_src), src, ent->d_name) < 0) {
                error_nonfatal("path_join failed %s and %s\n", src, ent->d_name);
                result = -1;
                break;
            }

            char child_dst[PATH_MAX];
            if (path_join(child_dst, sizeof(child_dst), dst, ent->d_name) < 0) {
                error_nonfatal("path_join failed %s and %s\n", dst, ent->d_name);
                result = -1;
                break;
            }

            if (copy_tree(child_src, child_dst) < 0) {
                result = -1;
                break;
            }
        }

        closedir(dir);
        return result;
    }

    if (!S_ISREG(st.st_mode)) {
        error_nonfatal("Unknown file type on file %s\n", src);
        return -1;
    }

    // Copy the file
    int in = open(src, O_RDONLY);
    if (in < 0) {
        error_nonfatal("open %s: %s\n", src, strerror(errno));
        return -1;
    }
    
    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL, st.st_mode & 0777);
    if (out < 0) {
        error_nonfatal("open %s: %s", dst, strerror(errno));
        close(in);
        return -1;
    }

    char buf[16384];
    ssize_t count;
    while ((count = read(in, buf, sizeof(buf))) != 0) {
        if (count < 0) {
            error_nonfatal("read: %s", strerror(errno));
            close(in);
            close(out);
            unlink(dst);
            return -1;
        }

        ssize_t n = write(out, buf, (size_t)(count));
        if (n != count) {
            error_nonfatal("write: %s", strerror(errno));
            unlink(dst);
            close(in);
            close(out);
            return -1;
        }
    }

    return 0;
}

static int remove_tree(const char *path) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        error_nonfatal("lstat(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) < 0) {
            error_nonfatal("Deleting %s failed: %s\n", path, strerror(errno));
            return -1;
        }

        return 0;
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        TRIGGER_ERROR_PERROR("opendir");
        return -1;
    }

    struct dirent *ent;
    int result = 0;
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
            continue;
        }

        char child[PATH_MAX];
        path_join(child, PATH_MAX, path, ent->d_name);
        if (remove_tree(child) < 0) {
            closedir(dir);
            return -1;
        }   
    }

    closedir(dir);
    if (rmdir(path) < 0) {
        error_nonfatal("Failed to remove directory: %s", strerror(errno));
        return -1;
    }


    return 0;
}

static void free_paths(char **paths, size_t count) {
    for (size_t i = 0; i < count; i++) free(paths[i]);
    free(paths);
}

static void add_selected_icon(nt_icon_entry_t *icon, void *data) {
    fb_paths_t *list = data;
    fb_entry_t *entry = icon->priv;

    char path[PATH_MAX];
    if (path_join(path, PATH_MAX, file_browser_path, entry->d_name) < 0) return;

    char **next = realloc(list->paths, (list->count + 1) * sizeof(char *));
    list->paths = next;

    list->paths[list->count] = strdup(path);
    if (list->paths[list->count]) list->count++;
}

static fb_paths_t selected_paths(void) {
    fb_paths_t list = {0};
    if (current_view && file_browser_view_mode == VIEW_MODE_ICON) {
        nt_icon_view_foreach_selected(current_view, add_selected_icon, &list);
    } else if (selected_entry) {
        // Column view doesn't yet support multi-selection
        char path[PATH_MAX];
        if (path_join(path, sizeof(path), file_browser_path, selected_entry->d_name) == 0) {
            list.paths = malloc(sizeof(char *));
            if (list.paths) {
                list.paths[0] = strdup(path);
                if (list.paths[0]) list.count = 1;
            }
        }
    }
    return list;
}

static void history_add(const char *path) {
    if (history_pos >= 0 && !strcmp(history[history_pos], path)) {
        return;
    }
    
    for (int i = history_pos + 1; i < history_count; i++) {
        free(history[i]);
    }
    
    history_count = history_pos + 1;
    if (history_count == FB_HISTORY_MAX) {
        free(history[0]);
        memmove(history, history + 1, (FB_HISTORY_MAX - 1) * sizeof(char *));
        history_count--;
    }

    history[history_count] = strdup(path);
    if (history[history_count]) history_pos = history_count++;
}

static int navigate_to(const char *path, bool add_history) {
    if (chdir(path) < 0) {
        error_nonfatal("Cannot open %s: %s", path, strerror(errno));
        return -1;
    }

    if (!getcwd(file_browser_path, sizeof(file_browser_path))) {
        error_nonfatal("Cannot read current directory: %s", strerror(errno));
        return -1;
    }

    if (add_history) history_add(file_browser_path);
    update_everything();
    return 0;
}


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
        
        if (lstat(ent->d_name, &fent->st) < 0) {
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
        navigate_to(ent->d_name, true);
        return;
    }
    if (!(ent->st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        error_nonfatal("No application is associated with %s", ent->d_name);
        return;
    }
    char path[PATH_MAX];
    if (path_join(path, sizeof(path), file_browser_path, ent->d_name) < 0) {
        TRIGGER_ERROR_PERROR("open");
        return;
    }
    pid_t cpid = fork();
    if (cpid < 0) { TRIGGER_ERROR_PERROR("fork"); return; }
    if (cpid == 0) {
        char *argv[] = { path, NULL };
        execv(path, argv);
        _exit(127);
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
    nt_signal_connect(new_folder_btn, "pressed", new_folder_pressed, NULL);
    nt_signal_connect(exit_btn, "pressed", exit_pressed, NULL);
    nt_menu_add_item(file_menu, new_folder_btn);
    nt_menu_add_item(file_menu, nt_menu_item_create_button_cb("Pin/unpin Current Folder", pin_pressed, NULL));
    nt_menu_add_item(file_menu, nt_menu_item_create_button_cb("Open in Terminal", _context_menu_open_terminal, NULL));
    nt_menu_add_item(file_menu, nt_menu_item_create_separator());
    nt_menu_add_item(file_menu, exit_btn);
    
    // Create edit menu
    nt_widget_t *edit_menu = nt_menu_create();
    nt_widget_t *copy_btn = nt_menu_item_create_button("Copy");
    nt_widget_t *paste_btn = nt_menu_item_create_button("Paste");
    nt_signal_connect(copy_btn, "pressed", copy_pressed, NULL);
    nt_signal_connect(paste_btn, "pressed", paste_pressed, NULL);
    nt_menu_add_item(edit_menu, copy_btn);
    nt_menu_add_item(edit_menu, paste_btn);
    nt_menu_add_item(edit_menu, nt_menu_item_create_button_cb("Delete", delete_pressed, NULL));

    // Create view menu
    nt_widget_t *view_menu = nt_menu_create();
    nt_widget_t *column_view_btn = nt_menu_item_create_button_icon("Column View", NULL);
    nt_widget_t *icon_view_btn = nt_menu_item_create_button_icon("Icon View", NULL);
    
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
    nt_signal_connect(path_input, "enter-pressed", path_enter_pressed, NULL);

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
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon_cb("New Folder", nt_icon_get("folder", NULL, 16), new_folder_pressed, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Paste", paste_pressed, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon_cb("Open in Terminal", nt_icon_get("terminal", NULL, 16), _context_menu_open_terminal, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Pin/Unpin Current Folder", pin_pressed, NULL));
    return ctx;
}

nt_widget_t *create_context_menu_item(fb_entry_t *entry) {
    nt_widget_t *ctx = nt_menu_create();
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Open", _context_menu_open, entry));
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon_cb("Open in Terminal", nt_icon_get("terminal", NULL, 16), _context_menu_open_terminal, entry));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Copy", copy_pressed, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Paste", paste_pressed, NULL));
    nt_menu_add_item(ctx, nt_menu_item_create_separator());
    nt_menu_add_item(ctx, nt_menu_item_create_button_icon_cb("Delete", nt_icon_get("ethereal", NULL, 16), delete_pressed, NULL));
    if (S_ISDIR(entry->st.st_mode))
        nt_menu_add_item(ctx, nt_menu_item_create_button_cb("Pin/Unpin This Folder", pin_pressed, entry));
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
            nt_widget_set_context_menu(file_wdgt, create_context_menu_item(ent));
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

    nt_signal_connect(icon_view, "icon-selection-changed", icon_selection_changed, NULL);
    nt_signal_connect(icon_view, "icon-double-clicked", double_click_event_icon, NULL);

    int item_index = 0;
    NT_ITERATE_CHILDREN(icon_view) {
        if (item_index < fb_ent_count)
            nt_widget_set_context_menu(child, create_context_menu_item(&fb_ent_list[item_index++]));
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

nt_widget_t *create_sidebar_entry(const char *name, nt_image_t *icon) {
    nt_widget_t *box = nt_box_create_horizontal();
    nt_style_set_suggested_width(&box->style, 100);
    nt_style_set_bg_color(&box->style, NT_COLOR(0,0,0,0));
    nt_style_set_margin_all(&box->style, 0);
    nt_widget_set_expansion(box, NT_EXPAND_HORIZONTAL);
    nt_widget_t *img = nt_image_create_from_image(icon);
    nt_widget_set_expansion(img, NT_EXPAND_VERTICAL);
    nt_style_set_margin_all(&img->style, 2);
    nt_box_append(box, img);
    nt_widget_t *lbl = nt_label_create((char *)name);
    nt_widget_set_expansion(lbl, NT_EXPAND_HORIZONTAL | NT_EXPAND_VERTICAL);
    nt_style_set_font(&lbl->style, NT_SANS_12);
    nt_label_set_ellipsize(lbl, 120);
    nt_style_set_margin_all(&lbl->style, 2);
    nt_style_set_bg_color(&lbl->style, NT_COLOR(0,0,0,0));
    nt_style_set_bg_color(&img->style, NT_COLOR(0,0,0,0));
    nt_box_append(box, lbl);
    return box;
}


nt_widget_t *create_sidebar() {
    sidebar = nt_list_view_create();
    nt_widget_set_expansion(sidebar, NT_EXPAND_VERTICAL);
    nt_style_set_suggested_width(&sidebar->style, 160);
    nt_style_set_margin_all(&sidebar->style, 0);
    rebuild_sidebar();
    return sidebar;
}

static void load_pins(void) {
    return;
}

static int save_pins(void) {
    // TODO
    return 0;
}

static bool sidebar_row_event(nt_widget_t *w, nt_event_t *event) {
    if (event->type == NT_EVENT_MOUSE_DOWN) {
        navigate_to((const char *)w->priv, true);
    }

    return true;
}

static void sidebar_add(const char *name, const char *path, nt_image_t *icon) {
    nt_widget_t *entry = create_sidebar_entry(name, icon);
    nt_list_view_append(sidebar, entry);
    nt_widget_set_selected(entry, !strcmp(path, file_browser_path));
    entry->parent->priv = (void *)path;
    nt_event_set_handler(entry->parent, NT_EVENT_MOUSE_DOWN, sidebar_row_event);
}

static void rebuild_sidebar(void) {
    while (sidebar->children) {
        nt_widget_free(sidebar->children);
    }
    
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    sidebar_add("Home", home, folder_icon);
    sidebar_add("Filesystem", "/", folder_icon);

    for (int i = 0; i < pin_count; i++) {
        const char *name = path_name(pins[i]);
        sidebar_add(*name ? name : "/", pins[i], folder_icon);
    }
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

    if (sidebar) {
        NT_ITERATE_CHILDREN(sidebar) {
            nt_list_item_t *row = (nt_list_item_t *)child;
            nt_widget_set_selected(row->child, !strcmp((const char *)child->priv, file_browser_path));
            nt_widget_invalidate(child);
        }
    }

    nt_input_set_text(path_input, file_browser_path);
    update_status_lbl_dir();
}

static void capture_folder_name(nt_widget_t *w, nt_signal_t *sig, void *data) {
    fb_prompt_t *prompt = data;
    prompt->name = strdup(nt_input_get_text(prompt->input));

    if (w->type == NT_WIDGET_INPUT) {
        nt_dialog_finish(prompt->dlg, NT_DIALOG_BTN_OK);
    }
}

void new_folder_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_dialog_t *dlg = nt_dialog_create_message(file_browser_window, "New Folder", "Folder name:", NT_DIALOG_MSG_TYPE_QUESTION, NT_DIALOG_MSG_BTNS_NONE);
    fb_prompt_t prompt = { NULL, NULL, dlg };

    nt_widget_t *input = nt_input_create("Name");
    prompt.input = input;

    nt_signal_connect(input, "enter-pressed", capture_folder_name, &prompt);
    nt_widget_set_expansion(input, NT_EXPAND_HORIZONTAL);
    nt_style_set_margin_all(&input->style, 4);
    nt_box_append(dlg->content_box, input);
    nt_window_set_focus(dlg->win, input);

    nt_widget_t *ok = nt_dialog_add_button(dlg, "Create", NT_DIALOG_BTN_OK);
    nt_signal_connect(ok, "pressed", capture_folder_name, &prompt);
    nt_dialog_add_button(dlg, "Cancel", NT_DIALOG_BTN_CANCEL);

    unsigned char result = nt_dialog_open(dlg);
    if (result != NT_DIALOG_BTN_OK) {
        free(prompt.name);
        return;
    }

    if (!prompt.name || !*prompt.name) {
        error_nonfatal("Enter a folder name");
        goto _error;
    }

    char path[PATH_MAX];
    if (path_join(path, sizeof(path), file_browser_path, prompt.name) < 0) {
        error_nonfatal("Cannot create folder: %s", strerror(errno));
        goto _error;
    }

    if (mkdir(path, 0755) < 0) {
        error_nonfatal("Cannot create folder: %s", strerror(errno));
        goto _error;
    }

    update_everything();
_error:
    free(prompt.name);
}

void copy_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    fb_paths_t list = selected_paths();
    if (!list.count) {
        free_paths(list.paths, list.count);
        error_nonfatal("Select an item to copy.");
        return;
    }

    free_paths(clipboard, clipboard_count);
    clipboard = list.paths;
    clipboard_count = list.count;
}

// calculate the name of a file for paste (copy %d)
static int paste_destination(char *out, size_t cap, const char *directory, const char *name) {
    if (path_join(out, cap, directory, name) < 0) {
        return -1;
    }

    struct stat st;
    if (lstat(out, &st) < 0) {
        if (errno != ENOENT) {
            return -1;
        } else {
            return 0;
        }
    }

    for (int i = 1; i < 10000; i++) {
        char candidate[PATH_MAX];
        if (i == 1) {
            snprintf(candidate, sizeof(candidate), "%s (copy)", name);
        } else {
            snprintf(candidate, sizeof(candidate), "%s (copy %d)", name, i);
        }

        if (path_join(out, cap, directory, candidate) < 0) {
            return -1;
        }

        if (lstat(out, &st) < 0) {
            if (errno != ENOENT) {
                return -1;
            } else {
                return 0;
            }
        }
    }

    return -1;
}

void paste_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (!clipboard_count) {
        error_nonfatal("Copy an item first before pasting, silly");
        return;
    }

    for (size_t i = 0; i < clipboard_count; i++) {
        char dest[PATH_MAX];
        if (paste_destination(dest, sizeof(dest), file_browser_path, path_name(clipboard[i])) < 0) {
            error_nonfatal("paste_destination failed, too many copies?\n");
            continue;
        }

        copy_tree(clipboard[i], dest);
    }

    update_everything();
}

void delete_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    fb_paths_t list = selected_paths();
    if (!list.count) {
        free_paths(list.paths, list.count);
        error_nonfatal("Select an item to delete.");
        return;
    }

    char message[160];
    snprintf(message, sizeof(message), "Permanently delete %zu selected item(s)?", list.count);
    nt_dialog_t *dlg = nt_dialog_create_message(file_browser_window, "Delete", message, NT_DIALOG_MSG_TYPE_WARNING, NT_DIALOG_MSG_BTNS_YES_NO);
    if (nt_dialog_open(dlg) != NT_DIALOG_BTN_YES) {
        free_paths(list.paths, list.count);
        return;
    }

    for (size_t i = 0; i < list.count; i++) {
        if (remove_tree(list.paths[i]) < 0) {
            continue;
        }
    }

    free_paths(list.paths, list.count);
    update_everything();
}

void pin_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char path[PATH_MAX];

    const char *target = file_browser_path;
    if (data) {
        fb_entry_t *entry = data;
        if (!S_ISDIR(entry->st.st_mode)) return;
        if (path_join(path, sizeof(path), file_browser_path, entry->d_name) < 0) {
            TRIGGER_ERROR_PERROR("pin"); return;
        }

        target = path;
    }

    for (int i = 0; i < pin_count; i++) {
        if (!strcmp(pins[i], target)) {
            free(pins[i]);
            memmove(&pins[i], &pins[i + 1], (size_t)(pin_count - i - 1) * sizeof(char *));
            pin_count--;
            if (save_pins() < 0) TRIGGER_ERROR_PERROR("save pins");
            rebuild_sidebar();
            return;
        }
    }

    if (pin_count == FB_PIN_MAX) {
        error_nonfatal("The sidebar is full.");
        return;
    }

    pins[pin_count] = strdup(target);
    pin_count++;
    if (save_pins() < 0) TRIGGER_ERROR_PERROR("save pins");

    rebuild_sidebar();
}

void double_click_event(nt_widget_t *w, nt_signal_t *sig, void *data) {
    fb_entry_t *ent = (fb_entry_t*)data;
    open_application(ent);
}

void home_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char *home = getenv("HOME");
    if (!home) home = "/root/";
    navigate_to(home, true);
}

void up_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    navigate_to("..", true);
}

void back_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (history_pos > 0 && navigate_to(history[history_pos - 1], false) == 0) history_pos--;
}

void forward_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (history_pos + 1 < history_count && navigate_to(history[history_pos + 1], false) == 0) history_pos++;
}

void path_enter_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char *entered = strdup(nt_input_get_text(w));
    if (!entered) return;
    navigate_to(entered, true);
    nt_input_set_text(path_input, file_browser_path);
    free(entered);
}

void exit_pressed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_window_close(file_browser_window);
}


void item_selected(nt_widget_t *w, nt_signal_t *sig, void *data) {
    selected_entry = (fb_entry_t*)data;
    update_status_lbl_selected((fb_entry_t*)data);
}

void item_deselected(nt_widget_t *w, nt_signal_t *sig, void *data) {
    if (selected_entry == data) selected_entry = NULL;
    update_status_lbl_dir();
}


void icon_selection_changed(nt_widget_t *w, nt_signal_t *sig, void *data) {
    int count = nt_icon_view_selected_count(w);
    selected_entry = NULL;
    if (count == 1) {
        nt_icon_entry_t *entry = nt_icon_view_first_selected(w);
        selected_entry = entry ? entry->priv : NULL;
        if (selected_entry) update_status_lbl_selected(selected_entry);
    } else if (count > 1) {
        char status[64];
        snprintf(status, sizeof(status), "%d items selected", count);
        nt_label_set_text(status_lbl, status);
    } else {
        update_status_lbl_dir();
    }
}

void double_click_event_icon(nt_widget_t *w, nt_signal_t *sig, void *data) {
    nt_icon_entry_t *ent = NT_SIGNAL_DATA(sig);
    fb_entry_t *fbent = ent->priv;
    open_application(fbent);
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
    fb_entry_t *entry = data ? data : selected_entry;
    if (entry) {
        open_application(entry);
    }
}
 
void _context_menu_open_terminal(nt_widget_t *w, nt_signal_t *sig, void *data) {
    char target[PATH_MAX];
    const char *dir = file_browser_path;

    fb_entry_t *entry = data;
    if (entry && S_ISDIR(entry->st.st_mode)) {
        path_join(target, PATH_MAX, file_browser_path, entry->d_name);
        dir = target;
    }

    pid_t child = fork();
    if (child < 0) { TRIGGER_ERROR_PERROR("fork"); return; }

    if (child == 0) {
        if (chdir(dir) < 0) _exit(127);

    #ifdef __ETHEREAL__
        char *term_name = "termemu";
    #else
        char *term_name = "xterm";
    #endif

        char *temp_argv[] = { term_name, NULL };
        execvp(term_name, (char *const*)temp_argv);
        _exit(127);
    }
}

void usage() {
    printf("Usage: file-browser [-h] [-v] [PATH]\n");
    printf("File browser built on Neutron\n\n");
    printf(" -h, --help         Display this help message\n");
    printf(" -v, --version      Print the version of file-browser\n\n");
    exit(1);
}

void version() {
    printf("file-browser version 1.0.0\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hv", options, NULL)) != -1) {
        switch (opt) {
            case 'v':
                version();
                break;
            case 'h':
            default:    
                usage();
                break;
        }
    }

    if (argc-optind > 0) {
        char *target_path = argv[optind];
        if (chdir(target_path) < 0) {
            fprintf(stderr, "file-browser: Cannot change directory to \"%s\": %s\n", target_path, strerror(errno));
            return 1;
        }
    }

    if (!getcwd(file_browser_path, sizeof(file_browser_path))) {
        fprintf(stderr, "file-browser: Cannot get current directory: %s\n", strerror(errno));
        return 1;
    }

    history_add(file_browser_path);

    load_pins();

    nt_init();
    load_icons();

    file_browser_window = nt_window_create("File Browser", 800, 600);
    nt_window_set_icon(file_browser_window, "folder");

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
    nt_style_set_border_color(&status_lbl->style, NT_COLOR(0xde,0xde,0xde,0xff));
    nt_style_set_border_thickness(&status_lbl->style, 5);
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
