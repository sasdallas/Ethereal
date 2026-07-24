/**
 * @file userspace/miniutils/proc-usage.c
 * @brief Process usage
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>

struct task {
    char name[256];
    char path[128];
    uint64_t time_ns_start;
    uint64_t time_start;
};

uint64_t get_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + (ts.tv_nsec);
}

void print_time(char *name, uint64_t before, uint64_t after) {
    uint64_t ns_diff = after - before;
    char *prefixes[] = {"ns", "us", "ms", "s"};
    int i = 0;
    double ns = (double)ns_diff;

    while (ns >= 1000 && i < 3) {
        ns = ns / 1000.0;
        i++;
    }

    printf("%s %.2f%s ", name, ns, prefixes[i]);
}

void add_task(struct task *out, char *pid) {
    char path[128];
    snprintf(path, 128, "%s/status", pid);

    FILE *f = fopen(path, "r");
    if (!f) { return; }

    fscanf(f, "ProcessName:%s\n", out->name);
    fclose(f);

    snprintf(path, 128, "%s/times", pid);
    f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(1); }

    fscanf(f, "#thr_id,time_usr,time_sys\n");
    out->time_ns_start = get_ns();
    out->time_start = 0;

    // sum up all times
    while (1) {
        uint64_t time_sys, time_usr;
        int r = fscanf(f, "%*d %llu %llu\n", &time_usr, &time_sys);
        if (r != 2) break;
        out->time_start += time_sys + time_usr;
    }
    
    fclose(f);

    strncpy(out->path, path, 128);
}

int main(int argc, char *argv[]) {
    int nr_tasks = 0;
    struct task *task_array = NULL;

    if (chdir("/system/processes/") < 0) {
        perror("chdir");
        return 1;
    }

    DIR *dirp = opendir(".");
    if (!dirp) {
        perror("opendir");
        return 1;
    }

    struct dirent *ent = NULL;
    while ((ent = readdir(dirp)) != NULL) {
        if (ent->d_name[0] == '.' || !strcmp(ent->d_name, "self")) {
            continue;
        }

        task_array = realloc(task_array, ++nr_tasks * sizeof(struct task));
        add_task(&task_array[nr_tasks-1], ent->d_name);
    }

    closedir(dirp);

    usleep(500000);

    for (int i = 0; i < nr_tasks; i++) {
        struct task *tsk = &task_array[i];
        
        FILE *f = fopen(tsk->path, "r");
        if (!f) { continue; } // task must have exited
        
        // todo verify its actually the same task
        fscanf(f, "#thr_id,time_usr,time_sys\n");
        uint64_t time_end = get_ns();
        uint64_t time_after = 0;

        // sum up all times
        while (1) {
            uint64_t time_sys, time_usr;
            int r = fscanf(f, "%*d %llu %llu\n", &time_usr, &time_sys);
            if (r != 2) break;
            time_after += time_sys + time_usr;
        }

        fclose(f);

        printf("%s ", tsk->name);
        print_time("time", tsk->time_start, time_after);
        
        double usage_time = ((double)(time_after - tsk->time_start) / (double)(time_end - tsk->time_ns_start)) * 100.0;
        printf("%.2f%%\n", usage_time);
    }

    free(task_array);
    return 0;
}