/**
 * @file userspace/miniutils/top.c
 * @brief top clone
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <pwd.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/utsname.h>

/* Terminal width and height */
int terminal_width = 0;
int terminal_height = 0;

/* CPUs */
struct cpu_data {
    uint64_t idle_last;
    uint64_t idle_time;
    uint64_t total_last;
    uint64_t total_time;
};

int cpu_count = 0;
struct cpu_data *cpus = NULL;

uint64_t sys_time_last = 0;
uint64_t sys_time = 0;

/* Memory information */
uint64_t mem_total;
uint64_t mem_used;
uint64_t mem_cache;

/* Uptime in seconds */
uint64_t uptime;

/* Termios */
struct termios tios_original;
struct termios tios_new;

/* Kernel information */
struct utsname kname;

/* Number of columns */
#define NCOLUMN 8

/* Process information */
struct process_info {
    pid_t pid;
    uid_t uid;
    uint64_t time_last;
    uint64_t time;
    double pct_cpu;
    double pct_mem;
    uint64_t virt;
    uint64_t res;
    char state;
    char *cmdline;
};

struct process_info *processes = NULL;
int process_arr_size = 0;
int nprocesses = 0;
int nrunning = 0;
int nthread = 0;

/* Column data */
struct top_column {
    char *name;
    int max_width;
    void (*printer)(struct top_column *col, struct process_info *p, char *buffer);
    long data; // offset in struct for generic printers
};


static void decimal_printer(struct top_column *col, struct process_info *p, char *buffer);
static void memory_printer(struct top_column *col, struct process_info *p, char *buffer);
static void user_printer(struct top_column *col, struct process_info *p, char *buffer);
static void state_printer(struct top_column *col, struct process_info *p, char *buffer);
static void cmd_printer(struct top_column *col, struct process_info *p, char *buffer);
static void percent_printer(struct top_column *col, struct process_info *p, char *buffer);
static void cpu_printer(struct top_column *col, struct process_info *p, char *buffer);
struct top_column column_data[NCOLUMN] = {
    { "PID", 0, decimal_printer, __builtin_offsetof(struct process_info, pid) },
    { "USER", 0, user_printer, 0 },
    { "VIRT", 0, memory_printer, __builtin_offsetof(struct process_info, virt) },
    { "RES", 0, memory_printer, __builtin_offsetof(struct process_info, res) },
    { "S", 0, state_printer, 0 },
    { "CPU%", 0, percent_printer, __builtin_offsetof(struct process_info, pct_cpu) },
    { "MEM%", 0, percent_printer, __builtin_offsetof(struct process_info, pct_mem) },
    { "Command", 0, cmd_printer, 0 }
};

/* Remaining lines for print */
int remaining_lines = 0;

/* Bar information */
#define BARS_PER_LINE   4
#define BAR_WIDTH       (terminal_width/BARS_PER_LINE)
#define BAR_WIDTH_LONG  (BAR_WIDTH*2) // You may wonder why not terminal_width / 2, and that is because this is to prevent rounding issues

/* ANSI stuff */
#define C_PURPLE    "\033[0;35m"
#define C_GRAY      "\033[0;90m"
#define C_GREEN     "\033[0;32m"
#define C_OFF       "\033[0m"
#define C_PURPLE_BG "\033[0;45m"
#define B_ON        "\033[1m"
#define B_OFF       "\033[22m"
#define L_CLR       "\033[K"

#define C_PRIMARY       C_PURPLE
#define C_PRIMARY_BG    C_PURPLE_BG

// map utility
static long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Collect process information
#ifdef __ETHEREAL__
void count_cpus() {
    // !!! stupid, need nproc command or something
    DIR *d = opendir("/system/cpus");
    if (!d) { perror("opendir"); exit(EXIT_FAILURE); }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] != '.') cpu_count++;
    }
    closedir(d);
}


struct process_info *find_process(pid_t pid) {
    // TODO THIS ARRAY MUST SHRINK!
    if (processes) {
        for (int i = 0; i < process_arr_size; i++) {
            if (processes[i].pid == pid) {
                return &processes[i];
            }
        }
    }

    processes = realloc(processes, sizeof(struct process_info) * (process_arr_size+1));
    struct process_info *p = &processes[process_arr_size];
    memset(p, 0, sizeof(struct process_info));
    process_arr_size++;
    return p;
}

int collect_process_info(pid_t pid) {
    struct process_info *p = find_process(pid);
    p->pid = pid;
    p->time_last = p->time;

    char tmp[256];
    char path_buf[256];

    snprintf(path_buf, sizeof(path_buf), "/system/processes/%d/status", pid);
    FILE *status = fopen(path_buf, "r");
    snprintf(path_buf, sizeof(path_buf), "/system/processes/%d/mem_usage", pid);
    FILE *mem_usage = fopen(path_buf, "r");
    snprintf(path_buf, sizeof(path_buf), "/system/processes/%d/times", pid);
    FILE *times = fopen(path_buf, "r");
    snprintf(path_buf, sizeof(path_buf), "/system/processes/%d/cmdline", pid);
    FILE *cmdline = fopen(path_buf, "r");

    if (!status || !mem_usage || !times || !cmdline) {
        if (status) fclose(status);
        if (mem_usage) fclose(mem_usage);
        if (times) fclose(times);
        if (cmdline) fclose(cmdline);
        return 1;
    }

    while (fgets(tmp, sizeof(tmp), status)) {
        if (strstr(tmp, "Uid:") == tmp) {
            sscanf(tmp, "Uid:%d", &p->uid);
        } else if (strstr(tmp, "State:") == tmp) {
            sscanf(tmp, "State:%c", &p->state);
        }
    }

    if (p->state == 'R') {
        nrunning += 1;
    }

    fclose(status);

    while (fgets(tmp, sizeof(tmp), mem_usage)) {
        if (strstr(tmp, "TotalMemoryUsage:") == tmp) {
            sscanf(tmp, "TotalMemoryUsage:%lld kB", &p->virt);
            p->virt = p->virt * 1024;
        } else if (strstr(tmp, "TotalMemoryResident:") == tmp) {
            sscanf(tmp, "TotalMemoryResident:%lld kB", &p->res);
            p->res = p->res * 1024;
        }
    }
    fclose(mem_usage);

    p->time = 0;
    fgets(tmp, sizeof(tmp), times);
    while (fgets(tmp, sizeof(tmp), times)) {
        unsigned long long utime = 0, stime = 0;
        if (sscanf(tmp, "%*d %llu %llu", &utime, &stime) == 2) {
            p->time += (utime + stime);
            nthread++;
        }
    }
    fclose(times);

    size_t read = fread(tmp, 1, 255, cmdline);
    if (read == 0) {
        p->cmdline = strdup("???");
    } else {
        size_t i = 0;
        while (i < read - 1) {
            if (tmp[i] == 0) {
                tmp[i] = ' ';   
            }
            i++;
        }
        tmp[read] = 0;
        p->cmdline = strdup(tmp);
    }
    fclose(cmdline);

    if (p->time_last) {
        unsigned long p_delta = (p->time - p->time_last);
        unsigned long s_delta = (sys_time - sys_time_last);
        
        if (s_delta > 0) {
            p->pct_cpu = ((double)p_delta / (double)s_delta) * 100.0 * cpu_count;
        } else {
            p->pct_cpu = 0.00;
        }
    } else {
        p->pct_cpu = 0.00;
    }

    if (mem_total > 0) {
        p->pct_mem = ((double)p->res / (double)mem_total) * 100.0;
    } else {
        p->pct_mem = 0.00;
    }

    return 0;
}

void collect_processes() {
    nprocesses = 0;
    nrunning = 0;
    nthread = 0;

    DIR *dp = opendir("/system/processes/");
    if (!dp) return;
    
    struct dirent *ent;
    while ((ent = readdir(dp))) {
        if (isdigit(ent->d_name[0])) {
            pid_t p = strtol(ent->d_name, NULL, 10);
            if (collect_process_info(p) == 0) {
                nprocesses++;
            } 
        }
    }

    closedir(dp);
}

void read_memory_info() {
    char tmp[256];
    
    FILE *f_pmm = fopen("/system/memory/pmm", "r");
    assert(f_pmm);

    while (fgets(tmp, sizeof(tmp), f_pmm)) {
        if (strstr(tmp, "TotalPhysMemory:") == tmp) {
            sscanf(tmp, "TotalPhysMemory:%llu kB", &mem_total);
        } else if (strstr(tmp, "UsedPhysMemory:") == tmp) {
            sscanf(tmp, "UsedPhysMemory:%llu kB", &mem_used);
        }
    }
    fclose(f_pmm);

    mem_total = mem_total * 1024;
    mem_used = mem_used * 1024;

    FILE *f_cache = fopen("/system/memory/cache", "r");
    assert(f_cache);

    unsigned long long active = 0, dirty = 0;
    while (fgets(tmp, sizeof(tmp), f_cache)) {
        if (strstr(tmp, "Active:") == tmp) {
            sscanf(tmp, "Active:%llu", &active);
        } else if (strstr(tmp, "Dirty:") == tmp) {
            sscanf(tmp, "Dirty:%llu", &dirty);
        } 
    }
    fclose(f_cache);

    mem_cache = (active + dirty) * 4096;
}

void read_times() {
    // save
    sys_time_last = sys_time;
    sys_time = 0;

    FILE *f = fopen("/system/times", "r");
    assert(f);

    char line[256];
    fgets(line, sizeof(line), f);

    int i = 0;
    while (fgets(line, sizeof(line), f) && i < cpu_count) {
        int cpu = -1;
        unsigned long long user = 0, idle = 0, sys = 0, irq = 0;

        if (sscanf(line, "cpu%d %llu %llu %llu %llu", &cpu, &user, &idle, &sys, &irq) == 5) {
            cpus[i].idle_last = cpus[i].idle_time;
            cpus[i].idle_time = idle;
            
            cpus[i].total_last = cpus[i].total_time;
            cpus[i].total_time = user + idle + sys + irq;
            sys_time += cpus[i].total_time;
            
            i++;
        }
    }

    fclose(f);
}

void read_uptime() {
    FILE *f = fopen("/system/uptime", "r");
    assert(f);
    fscanf(f, "%lu.%*lu", &uptime);
    fclose(f);
}

#else
void count_cpus() {
    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
}

struct process_info *find_process(pid_t pid) {
    // TODO THIS ARRAY MUST SHRINK!
    if (processes) {
        for (int i = 0; i < process_arr_size; i++) {
            if (processes[i].pid == pid) {
                return &processes[i];
            }
        }
    }

    processes = realloc(processes, sizeof(struct process_info) * (process_arr_size+1));
    struct process_info *p = &processes[process_arr_size];
    memset(p, 0, sizeof(struct process_info));
    process_arr_size++;
    return p;
}

int collect_process_info(pid_t pid) {
    struct process_info *p = find_process(pid);
    p->pid = pid;
    p->time_last = p->time;

    char tmp[256];
    snprintf(tmp, 256, "/proc/%d/status", pid);
    FILE *status = fopen(tmp, "r");
    snprintf(tmp, 256, "/proc/%d/cmdline", pid);
    FILE *cmdline = fopen(tmp, "r");
    snprintf(tmp, 256, "/proc/%d/stat", pid);
    FILE *stat = fopen(tmp, "r");
    snprintf(tmp, 256, "/proc/%d/comm", pid);
    FILE *comm = fopen(tmp, "r");

    if (!status || !cmdline || !stat || !comm) {
        // fprintf(stderr, "Task read error on PID %d: %s\n", pid, strerror(errno));
        if (status) fclose(status);
        if (cmdline) fclose(cmdline);
        if (stat) fclose(stat);
        if (comm) fclose(comm);
        return 1;
    }

    while (fgets(tmp, 256, status)) {
        if (strstr(tmp, "Uid:") == tmp) {
            sscanf(tmp, "Uid: %d", &p->uid);
        } else if (strstr(tmp, "VmSize") == tmp) {
            sscanf(tmp, "VmSize: %lld kB", &p->virt);
            p->virt = p->virt * 1024;
        } else if (strstr(tmp, "VmRSS") == tmp) {
            sscanf(tmp, "VmRSS: %lld kB", &p->res);
            p->res = p->res * 1024;
        } else if (strstr(tmp, "State") == tmp) {
            sscanf(tmp, "State: %c", &p->state);
        }
    }

    fclose(status);


    if (p->state == 'R') {
        nrunning += 1;
    }
    
    size_t read = fread(tmp, 1, 255, cmdline);
    if (read == 0){
        // we need to check comm
        size_t read = fread(tmp, 1, 255, comm);
        if (read == 0) {
            p->cmdline = strdup("???");
        } else {
            *(strchrnul(tmp, '\n')) = 0;
            p->cmdline = strdup(tmp);
        }
    } else {
        size_t i = 0;
        while (i < read-1) {
            if (tmp[i] == 0) {
                tmp[i] = ' ';   
            }
            i++;
        }

        tmp[read] = 0; 

        p->cmdline = strdup(tmp);
    }

    fclose(cmdline);
    fclose(comm);

    // stupid hack
    char stat_buf[512];
    if (fgets(stat_buf, sizeof(stat_buf), stat)) {
        char *p_close = strrchr(stat_buf, ')');
        if (p_close) {
            unsigned long utime = 0, stime = 0;
            sscanf(p_close + 2, "%*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu", &utime, &stime);
            p->time = utime + stime;
        }
    }
    fclose(stat);

    // Calculate the percentage of CPU/MEM in use
    if (p->time_last) {
        unsigned long p_delta = (p->time - p->time_last);
        unsigned long s_delta = (sys_time - sys_time_last);
        // unsigned long s_delta = cpu_count * sysconf(_SC_CLK_TCK);

        
        p->pct_cpu = ((double)p_delta / (double)s_delta) * 100.0 * cpu_count;
    } else {
        p->pct_cpu = 0.00;
    }

    p->pct_mem = ((double)p->res / (double)mem_total) * 100.0;

    return 0;
}

void collect_processes() {
    nprocesses = 0;

    DIR *dp = opendir("/proc/");
    struct dirent *ent;
    while ((ent = readdir(dp))) {
        if (ent->d_type == DT_DIR && isdigit(ent->d_name[0])) {
            pid_t p = strtol(ent->d_name, NULL, 10);
            if (collect_process_info(p) == 0) {
                nprocesses++;
            } 
        }
    }

    closedir(dp);
}

void read_memory_info() {
    FILE *f = fopen("/proc/meminfo", "r");
    assert(f);

    char tmp[256];
    while (fgets(tmp, 256, f)) {
        if (strstr(tmp, "MemTotal") == tmp) {
            sscanf(tmp, "MemTotal: %llu kB", &mem_total);
        } else if (strstr(tmp, "MemAvailable") == tmp) {
            uint64_t mem_free;
            sscanf(tmp, "MemAvailable: %llu kB", &mem_free);
            mem_used = mem_total - mem_free;
        } else if (strstr(tmp, "Cached") == tmp) {
            sscanf(tmp, "Cached: %llu kB", &mem_cache);
        } 
    }

    fclose(f);

    mem_total = mem_total * 1024;
    mem_cache = mem_cache * 1024;
    mem_used = mem_used * 1024;
}

void read_times() {
    // save
    sys_time_last = sys_time;

    // Read next measurements
    FILE *f = fopen("/proc/stat", "r");
    assert(f);

    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    user = nice = system = idle = iowait = irq = softirq = steal = 0;
    fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %*llu %*llu\n", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    sys_time = user + nice + system + idle + iowait + irq + softirq + steal;

    int i = 0;
    while (i < cpu_count) {
        user = nice = system = idle = iowait = irq = softirq = steal = 0;
        int cpu = -1;
        fscanf(f, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %*llu %*llu\n", &cpu, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        if (cpu == -1) break;

        cpus[i].idle_last = cpus[i].idle_time;
        cpus[i].idle_time = idle + iowait;
        cpus[i].total_last = cpus[i].total_time;
        cpus[i].total_time = user + nice + system + idle + iowait + irq + softirq + steal;
        i++;
    }

    fclose(f);
}

void read_uptime() {
    FILE *f = fopen("/proc/uptime", "r");
    assert(f);
    fscanf(f, "%lu", &uptime);
    fclose(f);
}

#endif


void setup_termios() {
    tcgetattr(STDOUT_FILENO, &tios_original);
    tios_new = tios_original;
    tios_new.c_lflag &= ~(ICANON | ECHO);
    tios_new.c_iflag &= ~(ICRNL | IXON);
    tcsetattr(STDOUT_FILENO, TCSANOW, &tios_new);
}




static void decimal_printer(struct top_column *col, struct process_info *p, char *buffer) {
    int d = *(int*)((uintptr_t)p + col->data);
    snprintf(buffer, 128, "%d", d);
}

static void memory_printer(struct top_column *col, struct process_info *p, char *buffer) {
    unsigned long d = *(unsigned long*)((uintptr_t)p + col->data);
    
    const char *suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    int i = 0;
    double double_bytes = (double)d;

    while (double_bytes >= 1024 && i < 6) {
        double_bytes /= 1024.0;
        i++;
    }
    
    snprintf(buffer, 128, "%.1f%s", double_bytes, suffixes[i]);
}

static void user_printer(struct top_column *col, struct process_info *p, char *buffer) {
    struct passwd *pw = getpwuid(p->uid);
    if (pw) {
        snprintf(buffer, 128, "%-8.8s", pw->pw_name);
    } else {
        snprintf(buffer, 128, "unknown");
    }
}


static void state_printer(struct top_column *col, struct process_info *p, char *buffer) { snprintf(buffer, 128, "%c", p->state);}
static void cmd_printer(struct top_column *col, struct process_info *p, char *buffer) {
    snprintf(buffer, 128, "%s", p->cmdline);
}

static void percent_printer(struct top_column *col, struct process_info *p, char *buffer) {
    double pct = *(double*)((uintptr_t)p + col->data);
    snprintf(buffer, 128, "%.1f%%", pct);    
}

static void cpu_printer(struct top_column *col, struct process_info *p, char *buffer) {
    snprintf(buffer, 128, "%lld %lld", p->time, p->time_last);
}

void print_bar(char *title, unsigned long frac, unsigned long total, void (*unit_formatter)(unsigned long, unsigned long, char *), bool is_long) {
    printf(B_ON C_PRIMARY "%s" C_OFF B_ON "[" C_OFF, title);

    char unit[128];
    if (unit_formatter) {
        unit_formatter(frac, total, unit);
    } else {
        // double pct = ((double)frac / (double)total) * 100;
        double pct = map(frac, 0, total, 0, 100);
        snprintf(unit, 128, "%3.1f%%", pct);
    }


    long width = ((is_long)?BAR_WIDTH_LONG:BAR_WIDTH) - strlen(title) - 2 - strlen(unit);

    long frac_map = map(frac, 0, total, 0, width);

    for (int i = 0; i < width; i++) {
        if (i < frac_map) putchar('|');
        else putchar(' ');
    }

    printf(C_GRAY B_ON "%s" C_OFF B_ON "]" C_OFF, unit);
}

void print_cpu_info() {
    for (int i = 0; i < cpu_count; i++) {
        char tmp[5];
        snprintf(tmp, 5, "%4d", i+1);
        
        
        uint64_t delta_total = cpus[i].total_time - cpus[i].total_last;
        uint64_t delta_idle = cpus[i].idle_time - cpus[i].idle_last;
        print_bar(tmp, delta_total-delta_idle, delta_total, NULL, false);

        if ((i % BARS_PER_LINE) == (BARS_PER_LINE)-1) {
            printf(L_CLR "\n");
            remaining_lines--;
        }
    }

    if (cpu_count % BARS_PER_LINE) {
        printf(L_CLR "\n");
        remaining_lines--;
    }

    print_bar(" Mem", mem_used, mem_total, NULL, true);

    // Now begins the information lines
    printf(C_PRIMARY "  Processes:" B_ON " %d" B_OFF ", " C_GREEN B_ON "%d" C_OFF C_PRIMARY " thr; " C_GREEN B_ON "%d" B_OFF C_PRIMARY " running" C_OFF L_CLR "\n" L_CLR, nprocesses, nthread, nrunning);
    remaining_lines--;
    
    // Print cached memory
    print_bar("Cach", mem_cache, mem_total, NULL, true);

    // Uptime
    int hours = uptime / (60 * 60); uptime = (uptime - (hours * (60 * 60)));
    int minutes = uptime / 60; uptime = (uptime - (minutes*60));
    printf(C_PRIMARY "  Uptime: " B_ON "%02d:%02d:%02d" L_CLR "\n" C_OFF, hours, minutes, uptime);
    remaining_lines--;

    // Fake bar
    for (int i = 0; i < BAR_WIDTH_LONG; i++) putchar(' ');

    
    printf(C_PRIMARY "  Kernel: " B_ON "%s %s" L_CLR "\n" L_CLR "\n" L_CLR C_OFF, kname.sysname, kname.release);
    remaining_lines--;
}

void print_proc_info() {
    // Padding
    remaining_lines--;
    int header_start_x = (terminal_height-remaining_lines);
    remaining_lines--;
    if (remaining_lines <= 0) return;

    // Probe
    for (int i = 0; i < NCOLUMN; i++) {
        column_data[i].max_width = 0;
        
        for (int j = 0; j < nprocesses; j++) {
            char tmp[128];
            column_data[i].printer(&column_data[i], &processes[j], tmp);

            int w = strlen(tmp);
            if (w > column_data[i].max_width) {
                column_data[i].max_width = w;
            }
        }

        if (strlen(column_data[i].name) > column_data[i].max_width) {
            column_data[i].max_width = strlen(column_data[i].name);
        }
    }

    // Print the initial header
    int x = 1;
    printf(C_PRIMARY_BG " ");
    for (int i = 0; i < NCOLUMN; i++) {
        if (x >= terminal_width-1) break;
        int w_to_do = (terminal_width - x - 1);
        if (w_to_do > column_data[i].max_width) {
            w_to_do = column_data[i].max_width;
        }
        x += printf("%-*.*s ", w_to_do, w_to_do, column_data[i].name); 
    }

    while (x < terminal_width-1) {
        putchar(' ');
        x++;
    }
    if (remaining_lines > 1) {
        printf(C_OFF L_CLR "\n");
    }
    
    // Print each process' data
    int i = 0;
    while (remaining_lines && i < nprocesses) {
        printf(" ");
        x = 1;

        for (int j = 0; j < NCOLUMN; j++) {
            char tmp[128];
            column_data[j].printer(&column_data[j], &processes[i], tmp);

            int w_to_do = (terminal_width - x - 1);
            if (w_to_do > column_data[j].max_width) {
                w_to_do = column_data[j].max_width;
            }

            x += printf("%-*.*s ", w_to_do, w_to_do, tmp);
        }
        
        if (remaining_lines > 1) printf(L_CLR "\n");

        remaining_lines--;
        i++;
    }
    

    while (remaining_lines>0) {
        if (remaining_lines > 1) {
            printf(L_CLR "\n");
        } else {
            printf(L_CLR);
            fflush(stdout);
        }
        remaining_lines--;
    }
}

int sort_fn(const void *_a, const void *_b) {
    const struct process_info *a = _a;
    const struct process_info *b = _b;
    
    // TODO: Custom sorters
    if (a->pct_cpu < b->pct_cpu) return 1;
    if (a->pct_cpu > b->pct_cpu) return -1;
    return 0;
}

int top_init() {
    // Get terminal window size
    struct winsize winsize;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) < 0) {
        perror("TIOCGWINSZ");
        return 1;
    }

    terminal_width = winsize.ws_col;
    terminal_height = winsize.ws_row;

    // Disable cursor and clear screen
	printf("\033[?1049h\033[?25l\033[H");

    // Get kernel info
    uname(&kname);

    return 0;
}

void top_loop() {
    for (;;) {
        read_times();
        read_memory_info();
        read_uptime();
        collect_processes();

        qsort(processes, nprocesses, sizeof(struct process_info), sort_fn);

        printf("\033[H");
        remaining_lines = terminal_height;

        print_cpu_info();
        print_proc_info();
        
        sleep(1);
    }
}

void sigwinch_handler(int signum) {
    top_init();
}

void restore_termios() {
    tcsetattr(STDOUT_FILENO, TCSANOW, &tios_original);
}

int main(int argc, char *argv[]) {
    // SIGWINCH can't do this in top_init
    setup_termios();
    atexit(restore_termios);

    // Count the CPUs
    count_cpus();
    cpus = malloc(sizeof(struct cpu_data) * cpu_count);
    memset(cpus, 0, sizeof(struct cpu_data) * cpu_count);

    signal(SIGWINCH, sigwinch_handler);

    if (top_init()) return 1;
    top_loop();
    return 0;
}
