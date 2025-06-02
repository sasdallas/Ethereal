/**
 * @file userspace/miniutils/dns-resolve.c
 * @brief dns-resolve
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: dns-resolve <hostname>\n");
        return 1;
    }

    struct hostent *ent = gethostbyname(argv[1]);
    if (!ent) {
        printf("dns-resolve: %s: not found by DNS\n", argv[1]);
        return 1;
    }

    int i = 0;
    while (ent->h_addr_list[i] != NULL) {
        struct in_addr *in = (struct in_addr*)ent->h_addr_list[i];
        printf("%s\n", inet_ntoa(*in));
        i++;
    }

    return 0;
}