/**
 * @file userspace/miniutils/usage.c
 * @brief demo utility to test processor usage
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

uint64_t get_ns() {
    struct timespec ts;

    // Get current time with nanosecond precision
    clock_gettime(CLOCK_REALTIME, &ts);
    
    // Convert seconds and nanoseconds to total milliseconds
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

int main(int argc, char *argv[]) {
    // Open kernel times
    uint64_t cpus[32][4];
    uint64_t cpus_after[32][4];
    int nr_cpus = 0;

    uint64_t ns_start = get_ns();

    FILE *f = fopen("/system/times", "r");
    if (!f) {
        perror("/system/times");
        return 1;
    }

    fscanf(f, "# cpu,user,idle,sys,irq\n");
    
    long start_offset = ftell(f);

    while (1) {
        if (fscanf(f, "cpu%*d %llu %llu %llu %llu\n", &cpus[nr_cpus][0], &cpus[nr_cpus][1], &cpus[nr_cpus][2], &cpus[nr_cpus][3]) != 4) break;
        nr_cpus++;
    }

    fclose(f);

    // Sleep for 1 second
    sleep(1);

    // Re-open
    f = fopen("/system/times", "r");
    if (!f) {
        perror("/system/times");
        return 1;
    }

    fseek(f, start_offset, SEEK_SET);

    // Re-measure
    uint64_t ns_passed = get_ns() - ns_start;
    for (int i = 0; i < nr_cpus; i++) {
        fscanf(f, "cpu%*d %llu %llu %llu %llu\n", &cpus_after[i][0], &cpus_after[i][1], &cpus_after[i][2], &cpus_after[i][3]);
        printf("CPU%d: ", i);

        print_time("user", cpus[i][0], cpus_after[i][0]);
        print_time("idle", cpus[i][1], cpus_after[i][1]);
        print_time("sys", cpus[i][2], cpus_after[i][2]);
        print_time("irq", cpus[i][3], cpus_after[i][3]);
        
        uint64_t itime = cpus_after[i][1] - cpus[i][1];
        double idle_pct = ((double)itime / (double)ns_passed) * 100.0;
        printf(" %.2f%%\n", 100.0 - idle_pct);
    }


    fclose(f);
    
    return 0;
}
