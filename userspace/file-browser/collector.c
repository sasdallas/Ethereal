/**
 * @file userspace/file-browser/collector.c
 * @brief Provides file collection API
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "file-browser.h"
#include <structs/list.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>

file_entry_t **file_list = NULL;
size_t file_list_length = 0;

void collector_destroyExisting() {
    file_entry_t **i = file_list;
    while (*i) free(*i++);
    free(file_list);
}


int sort_fn(const void *s1, const void *s2) {
    file_entry_t *f1 = (file_entry_t*)s1;
    file_entry_t *f2 = (file_entry_t*)s2;

    return strcmp(f1->file_name, f2->file_name);
}

void collector_collectFiles() {
    printf("Trace; collector_collectFiles\n");
    if (file_list) collector_destroyExisting();

    file_list = malloc(1 * sizeof(file_entry_t*));
    size_t len = 0;

    // Open current directory
    char cwd[1024];
    assert(getcwd(cwd, 1024));
    
    DIR *dirp = opendir(cwd);
    if (!dirp) PRINT_ERROR_AND_DIE(cwd);

    struct dirent *ent = readdir(dirp);
    while (ent) {
        // Create a new file entry
        if (ent->d_name[0] == '.') {
            // Skip hidden files
            ent = readdir(dirp);
            continue;
        }

        file_entry_t *fent = malloc(sizeof(file_entry_t));
        strncpy(fent->file_name, ent->d_name, 256);;
        if (stat(fent->file_name, &fent->st) < 0) PRINT_ERROR_AND_DIE("stat");

        len++;
        file_list = realloc(file_list, (len+1) * sizeof(file_entry_t*));
        file_list[len-1] = fent;
        file_list[len] = NULL;
        ent = readdir(dirp);
    }

    closedir(dirp);

    qsort(file_list, len, sizeof(file_entry_t*), sort_fn);
    file_list_length = len;
}

file_entry_t **collector_getFileList() {
    assert(file_list);
    return file_list;
}

file_entry_t *collector_get(int index) {
    if (index >= (int)file_list_length || index < 0) return NULL;
    return file_list[index]; 
}