/**
 * @file userspace/miniutils/uname.c
 * @brief uname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal operating system.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEFAULT_UNAME_OPTION    "-s"

struct utsname buf;

void help() {
    printf("Usage: uname <option>\n");
    printf("\t-a, --all                 print all information in the following order\n");
    printf("\t-s, --kernel-name         print the kernel name\n");
    printf("\t-n, --nodename            print the nodename\n");
    printf("\t-r, --kernel-release      print the kernel release\n");
    printf("\t-v, --kernel-version      print the kernel version\n");
    printf("\t-m, --machine             print the machine hardware name\n");
    printf("\t-p, --processor           print the processor type\n");
    printf("\t-i, --hardware-platform   print the hardware platform\n");
    printf("\t-o, --operating-system    print the operating system\n");
    printf("\t--help                    display this help and exit\n");
    printf("\t--version                 print the version and exit\n");
    exit(0);
}

int uname_process_option(char *option) {
    // Ugly...
    if (!strcmp(option, "--help")) {
        help();
    } else if (!strcmp(option, "--version")) {
        printf("uname (Ethereal miniutils) 1.00\n");
        printf("Copyright (C) 2025 The Ethereal Development Team\n");
        exit(0);
    } else if (!strcmp(option, "-a") || !strcmp(option, "--all")) {
        printf("%s %s %s %s %s ", buf.sysname, buf.nodename, buf.release, buf.version, buf.machine);
    } else if (!strcmp(option, "-s") || !strcmp(option, "--kernel-name")) {
        printf("%s ", buf.sysname);
    } else if (!strcmp(option, "-n") || !strcmp(option, "--nodename")) {
        printf("%s ", buf.nodename);
    } else if (!strcmp(option, "-r") || !strcmp(option, "--kernel-release")) {
        printf("%s ", buf.release);
    } else if (!strcmp(option, "-v") || !strcmp(option, "--kernel-version")) {
        printf("%s ", buf.version);
    } else if (!strcmp(option, "-m") || !strcmp(option, "--machine")) {
        printf("%s ", buf.machine);
    } else if (!strcmp(option, "-p") || !strcmp(option, "--processor")) {
        printf("%s ", buf.machine);
    } else if (!strcmp(option, "-i") || !strcmp(option, "--hardware-platform")) {
        printf("%s ", buf.machine);
    } else if (!strcmp(option, "-o") || !strcmp(option, "--oeprating-system")) {
        printf("%s ", buf.sysname);
    } else {
        printf("uname: invalid option -- '%s'\n", option);
        printf("Try 'uname --help' for more information.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
} 

int main(int argc, char **argv) {
    // Get uname inforamtion
    if (uname(&buf) < 0) {
        printf("Could not get kernel information: %s\n", strerror(errno));
        return 1;
    }

    if (argc < 1) {
        uname_process_option(DEFAULT_UNAME_OPTION);
        putchar('\n');
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        uname_process_option(argv[i]);
    }

    putchar('\n');
    return 0;
}