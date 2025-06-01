/**
 * @file userspace/miniutils/setip.c
 * @brief A small utility to set the IP address of a NIC in Ethereal
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
#include <sys/ioctl.h>
#include <kernel/drivers/nicdev.h>
#include <stdlib.h>
#include <getopt.h>
#include <arpa/inet.h>

void help() {
    printf("Usage: setip [-g] [-s] [-c] [NIC] [IPv4]\n");
    printf("Set the IP address of a NIC\n");
    exit(EXIT_SUCCESS);
}

void version() {
    printf("setip (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    char c;
    int gateway = 0;
    int subnet = 0;
    int print_info = 0;

    while ((c = getopt(argc, argv, "gsch")) != -1) {
        switch (c) {
            case 'g':
                gateway = 1;
                break;
            case 's':
                subnet = 1;
                break;
            case 'c':
                print_info = 1;
                break;
            case 'v':
                version();
                break;
            case 'h':
            default:
                help();
                break;
        }
    }

    // Check
    if (argc - optind < 2 && !(print_info && argc - optind)) {
        help();
    }

    // Get NIC
    char nic_path[512];
    snprintf(nic_path, 512, "/device/%s", argv[optind]);
    int nic = open(nic_path, O_RDONLY);
    if (nic < 0) {
        perror("open");
        return 1;
    }

    // Perform ioctl
    nic_info_t info;
    if (ioctl(nic, IO_NIC_GET_INFO, &info) < 0) {
        perror("IO_NIC_GET_INFO");
        return 1;
    }

    // Print info if user wants it
    if (print_info) {
        printf("%s state UP mtu %d\n", info.nic_name, info.nic_mtu);
        printf("\tlink/ether %02x:%02x:%02x:%02x:%02x:%02x\n", info.nic_mac[0], info.nic_mac[1], info.nic_mac[2], info.nic_mac[3], info.nic_mac[4], info.nic_mac[5]);
        
        struct in_addr in;
        in.s_addr = info.nic_ipv4_addr;
        char *in_name = inet_ntoa(in);
        printf("\tinet %s", in_name);

        // This was the best way I could figure out how to get the CIDR notation
        // Kill me
        int cidr = 0;
        while (info.nic_ipv4_subnet & (1 << cidr)) cidr++;
        
        in.s_addr = info.nic_ipv4_gateway;
        char *in_gateway = inet_ntoa(in);

        printf("/%d gateway %s\n", cidr, in_gateway);
        return 0;
    }

    // Convert to address
    in_addr_t addr = inet_addr(argv[optind+1]);

    if (!gateway && !subnet) {
        info.nic_ipv4_addr = addr;
    } else {
        if (gateway) info.nic_ipv4_gateway = addr;
        if (subnet) info.nic_ipv4_subnet = addr;
    }

    // Request they set the parameters
    if (ioctl(nic, IO_NIC_SET_INFO, &info) < 0) {
        perror("IO_NIC_SET_INFO");
        return 1;
    }

    return 0;
}