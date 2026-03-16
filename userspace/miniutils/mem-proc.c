/**
 * @file userspace/miniutils/mem-proc.c
 * @brief show all processes memory usage
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    DIR *d = opendir("/system/processes");
    if (!d) return 1;

    // read some kernel information
    FILE *f = fopen("/system/memory/pmm", "r");
    if (!f) return 1;
    uintptr_t total_phys_memory;
    uintptr_t used_phys_memory;
    fscanf(f,    
        "TotalPhysBlocks:%*d\n"
        "TotalPhysMemory:%zu kB\n"
        "UsedPhysMemory:%zu kB\n"
        "FreePhysMemory:%*zu kB\n"
        "PhysMemoryBytes:%*zu\n", &total_phys_memory, &used_phys_memory);
    fclose(f);

    printf("process name    virt     resident   anon   file     pct\n");

    struct dirent *ent = readdir(d);
    while (ent) {
        if (ent->d_name[0] == '.' || !strcmp(ent->d_name, "self")) {
            ent = readdir(d);
            continue;
        }

        // read the process name
        char tmp[256];
        snprintf(tmp, 256, "/system/processes/%s/status", ent->d_name);
        f = fopen(tmp, "r");
        if (!f) return 1;

        char process_name[80];
        fscanf(f, "ProcessName:%s\n", process_name);
        fclose(f);

        snprintf(tmp, 256, "/system/processes/%s/mem_usage", ent->d_name);
        f = fopen(tmp, "r");
        if (!f) return 1;

        uintptr_t virt;
        uintptr_t real;
        uintptr_t anon_total;
        uintptr_t anon_res;
        uintptr_t file_total;
        uintptr_t file_res;
        fscanf(f,
            "TotalMemoryUsage:%d kB\n"
            "TotalMemoryResident:%d kB\n"
            "AnonUsage:%d kB\n"
            "AnonResident:%d kB\n"
            "FileUsage:%d kB\n"
            "FileResident:%d kB\n",
            &virt, &real, &anon_total, &anon_res, &file_total, &file_res
        );

        fclose(f);

        printf("%-15s %6dkB %6dkB %6dkB %6dkB %d%%\n", process_name, virt, real, anon_total, file_total, (int)(((double)real / (double)total_phys_memory) * 100.0));

        ent = readdir(d);
    }

    return 0;

}