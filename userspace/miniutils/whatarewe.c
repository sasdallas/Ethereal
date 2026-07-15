/**
 * @file userspace/miniutils/whatarewe.c
 * @brief Minor neofetch-ish clone
 * 
 * Note that this is pretty hacked together.
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifdef __ETHEREAL__
#include <ethereal/network.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>

char *ascii[] = {"\033[1m\n",
"                       :::::       ",
"                    :        .     ",
"                 :           ..    ",
"            ::::::::---      ...   ",
"         :.......:::--==+=   ...   ",
"       ::.   ...::---==+===  ..    ",
"      ::.    ..:----=++=====...    ",
"     :-::....:---===+++====-...    ",
"    ::::-::---==+++++++++==...     ",
"    :::----===+++++++***--...+     ",
"    -::--====+++++*****#=...+*     ",
"     ::---====++**##**#:..:++*     ",
"    =-:--===+++****+*#...-=+*      ",
"   == =---==+++***++-:::+=+*       ",
"   ==   +=+++======:--=++**        ",
"   =+     +**+++=---=++*           ",
"   =+         -====                ",
"    +**   +++++==                  ",
"     +==++++=                      "};

const char *ascii_utf[] = {
    "\x1b[49m                                          \x1b[m",
    "\x1b[49m                           \x1b[38;5;189;49m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;189;48;5;225m\xe2\x96\x84\x1b[38;5;189;49m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;49m\xe2\x96\x84\x1b[49m       \x1b[m",
    "\x1b[49m                       \x1b[38;5;189;49m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;189m\xe2\x96\x80\xe2\x96\x80\xe2\x96\x80\xe2\x96\x80\xe2\x96\x80\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;225;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;49m\xe2\x96\x84\x1b[49m     \x1b[m",
    "\x1b[49m                    \x1b[38;5;189;49m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;189m\xe2\x96\x80\x1b[49m         \x1b[49;38;5;225m\xe2\x96\x80\x1b[38;5;225;48;5;225m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;48;5;15m\xe2\x96\x84\x1b[49m    \x1b[m",
    "\x1b[49m                \x1b[38;5;189;49m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;189m\xe2\x96\x84\x1b[38;5;183;49m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;49m\xe2\x96\x84\x1b[49m         \x1b[38;5;225;48;5;225m\xe2\x96\x84\xe2\x96\x84\x1b[48;5;225m \x1b[49m    \x1b[m",
    "\x1b[49m           \x1b[38;5;189;49m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;225;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;15;48;5;189m\xe2\x96\x84\x1b[38;5;15;48;5;225m\xe2\x96\x84\x1b[38;5;225;48;5;189m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;183m\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;99;49m\xe2\x96\x84\x1b[49m     \x1b[38;5;225;48;5;225m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[49m   \x1b[m",
    "\x1b[49m         \x1b[38;5;189;49m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;225;48;5;189m\xe2\x96\x84\x1b[38;5;15;48;5;225m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;48;5;255m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\x1b[38;5;105;49m\xe2\x96\x84\x1b[49m  \x1b[38;5;225;48;5;15m\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;48;5;15m\xe2\x96\x84\x1b[49m   \x1b[m",
    "\x1b[49m       \x1b[38;5;183;49m\xe2\x96\x84\x1b[38;5;189;48;5;183m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;255;48;5;225m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[48;5;15m  \x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;255;48;5;255m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;75;48;5;69m\xe2\x96\x84\x1b[38;5;111;48;5;75m\xe2\x96\x84\x1b[38;5;141;48;5;105m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;141;49m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\x1b[38;5;15;48;5;225m\xe2\x96\x84\x1b[48;5;225m \x1b[38;5;225;48;5;225m\xe2\x96\x84\x1b[49m    \x1b[m",
    "\x1b[49m      \x1b[38;5;189;49m\xe2\x96\x84\x1b[38;5;189;48;5;183m\xe2\x96\x84\x1b[48;5;189m \x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[48;5;15m   \x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;255m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;183;48;5;189m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;105m\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\x1b[38;5;105;48;5;111m\xe2\x96\x84\x1b[38;5;141;48;5;105m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;141;48;5;105m\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\x1b[38;5;15;48;5;225m\xe2\x96\x84\x1b[49m    \x1b[m",
    "\x1b[49m     \x1b[38;5;254;49m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;183;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;225;48;5;15m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;255;48;5;15m\xe2\x96\x84\x1b[38;5;189;48;5;15m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;147;48;5;189m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;141;48;5;105m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;255;48;5;147m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\x1b[49m     \x1b[m",
    "\x1b[49m     \x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;189;48;5;183m\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;189m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;140;48;5;141m\xe2\x96\x84\x1b[38;5;104;48;5;140m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;141m\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;99m\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;69m\xe2\x96\x84\x1b[38;5;99;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;254;48;5;147m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;225m\xe2\x96\x80\x1b[49m     \x1b[m",
    "\x1b[49m     \x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;140;48;5;141m\xe2\x96\x84\x1b[38;5;104;48;5;140m\xe2\x96\x84\x1b[38;5;98;48;5;104m\xe2\x96\x84\x1b[38;5;62;48;5;98m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;68;48;5;62m\xe2\x96\x84\x1b[38;5;68;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;69m\xe2\x96\x84\x1b[38;5;99;48;5;105m\xe2\x96\x84\x1b[38;5;63;48;5;99m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;147;48;5;141m\xe2\x96\x84\x1b[38;5;147;48;5;105m\xe2\x96\x84\x1b[38;5;254;48;5;147m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;255m\xe2\x96\x84\x1b[38;5;105;48;5;189m\xe2\x96\x84\x1b[49m      \x1b[m",
    "\x1b[49m     \x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;104;48;5;141m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;104;48;5;140m\xe2\x96\x84\x1b[38;5;104;48;5;104m\xe2\x96\x84\x1b[38;5;68;48;5;104m\xe2\x96\x84\x1b[38;5;68;48;5;68m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;98;48;5;68m\xe2\x96\x84\x1b[38;5;61;48;5;62m\xe2\x96\x84\x1b[38;5;61;48;5;98m\xe2\x96\x84\x1b[38;5;62;48;5;99m\xe2\x96\x84\x1b[38;5;63;48;5;63m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\x1b[38;5;255;48;5;189m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;255m\xe2\x96\x84\x1b[38;5;68;48;5;147m\xe2\x96\x84\x1b[38;5;62;48;5;63m\xe2\x96\x84\x1b[49m      \x1b[m",
    "\x1b[49m     \x1b[38;5;189;48;5;189m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;111;48;5;104m\xe2\x96\x84\x1b[38;5;104;48;5;104m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;98;48;5;104m\xe2\x96\x84\x1b[38;5;98;48;5;98m\xe2\x96\x84\x1b[38;5;61;48;5;97m\xe2\x96\x84\x1b[38;5;61;48;5;61m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;61m\xe2\x96\x84\x1b[38;5;63;48;5;62m\xe2\x96\x84\x1b[38;5;63;48;5;63m\xe2\x96\x84\x1b[38;5;62;48;5;63m\xe2\x96\x84\x1b[38;5;61;48;5;63m\xe2\x96\x84\x1b[38;5;104;48;5;62m\xe2\x96\x84\x1b[38;5;255;48;5;251m\xe2\x96\x84\x1b[38;5;15;48;5;15m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;251;48;5;15m\xe2\x96\x84\x1b[38;5;68;48;5;110m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[49m      \x1b[m",
    "\x1b[49m     \x1b[38;5;111;48;5;147m\xe2\x96\x84\x1b[38;5;183;48;5;189m\xe2\x96\x84\x1b[38;5;189;48;5;189m\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;99;48;5;105m\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;99;48;5;98m\xe2\x96\x84\x1b[38;5;63;48;5;98m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;61m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;61;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;61;48;5;61m\xe2\x96\x84\x1b[38;5;140;48;5;61m\xe2\x96\x84\x1b[38;5;225;48;5;252m\xe2\x96\x84\x1b[38;5;225;48;5;15m\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\x1b[38;5;147;48;5;189m\xe2\x96\x84\x1b[38;5;104;48;5;104m\xe2\x96\x84\x1b[38;5;68;48;5;68m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[49m      \x1b[m",
    "\x1b[49m    \x1b[38;5;105;48;5;111m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;141;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;99;48;5;99m\xe2\x96\x84\x1b[38;5;63;48;5;99m\xe2\x96\x84\x1b[38;5;63;48;5;63m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;63;48;5;62m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\x1b[38;5;98;48;5;61m\xe2\x96\x84\x1b[38;5;254;48;5;104m\xe2\x96\x84\x1b[38;5;225;48;5;225m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;189;48;5;225m\xe2\x96\x84\x1b[38;5;146;48;5;189m\xe2\x96\x84\x1b[38;5;110;48;5;110m\xe2\x96\x84\x1b[38;5;104;48;5;104m\xe2\x96\x84\x1b[38;5;62;48;5;68m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\x1b[49m       \x1b[m",
    "\x1b[49m    \x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;105;48;5;111m\xe2\x96\x84\x1b[49;38;5;111m\xe2\x96\x80\x1b[38;5;141;48;5;147m\xe2\x96\x84\x1b[38;5;141;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;147m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;147;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;63;48;5;99m\xe2\x96\x84\x1b[38;5;63;48;5;63m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;104;48;5;62m\xe2\x96\x84\x1b[38;5;182;48;5;62m\xe2\x96\x84\x1b[38;5;225;48;5;182m\xe2\x96\x84\x1b[38;5;183;48;5;225m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;146;48;5;189m\xe2\x96\x84\x1b[38;5;68;48;5;110m\xe2\x96\x84\x1b[38;5;104;48;5;104m\xe2\x96\x84\x1b[38;5;68;48;5;104m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[49m        \x1b[m",
    "\x1b[49m   \x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[49m  \x1b[49;38;5;141m\xe2\x96\x80\x1b[38;5;99;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;105;48;5;141m\xe2\x96\x84\x1b[38;5;105;48;5;147m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;104;48;5;105m\xe2\x96\x84\x1b[38;5;146;48;5;104m\xe2\x96\x84\x1b[38;5;183;48;5;182m\xe2\x96\x84\x1b[38;5;183;48;5;183m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;68;48;5;146m\xe2\x96\x84\x1b[38;5;62;48;5;68m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;62m\xe2\x96\x80\x1b[49m         \x1b[m",
    "\x1b[49m   \x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;68;48;5;105m\xe2\x96\x84\x1b[49m    \x1b[49;38;5;99m\xe2\x96\x80\x1b[38;5;99;48;5;99m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;99m\xe2\x96\x84\x1b[38;5;62;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;69;48;5;69m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;69;48;5;105m\xe2\x96\x84\x1b[38;5;105;48;5;69m\xe2\x96\x84\x1b[38;5;147;48;5;105m\xe2\x96\x84\x1b[38;5;183;48;5;146m\xe2\x96\x84\x1b[38;5;147;48;5;183m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;140;48;5;147m\xe2\x96\x84\x1b[38;5;104;48;5;146m\xe2\x96\x84\x1b[38;5;62;48;5;68m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;62m\xe2\x96\x80\x1b[49m           \x1b[m",
    "\x1b[49m   \x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\x1b[49m      \x1b[49;38;5;62m\xe2\x96\x80\xe2\x96\x80\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;98;48;5;62m\xe2\x96\x84\x1b[38;5;140;48;5;62m\xe2\x96\x84\x1b[38;5;146;48;5;104m\xe2\x96\x84\x1b[38;5;146;48;5;146m\xe2\x96\x84\x1b[38;5;105;48;5;147m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;104;48;5;105m\xe2\x96\x84\x1b[38;5;62;48;5;104m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\x1b[49;38;5;62m\xe2\x96\x80\xe2\x96\x80\x1b[49m              \x1b[m",
    "\x1b[49m   \x1b[49;38;5;105m\xe2\x96\x80\x1b[38;5;99;48;5;105m\xe2\x96\x84\x1b[38;5;63;48;5;62m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\x1b[38;5;62;49m\xe2\x96\x84\x1b[49m     \x1b[38;5;104;49m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;104;48;5;140m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;104m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;111;48;5;105m\xe2\x96\x84\x1b[49;38;5;105m\xe2\x96\x80\x1b[49m                   \x1b[m",
    "\x1b[49m    \x1b[49;38;5;105m\xe2\x96\x80\x1b[38;5;63;48;5;63m\xe2\x96\x84\x1b[38;5;63;48;5;62m\xe2\x96\x84\x1b[38;5;62;48;5;62m\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\xe2\x96\x84\x1b[38;5;62;48;5;68m\xe2\x96\x84\x1b[38;5;68;48;5;68m\xe2\x96\x84\x1b[38;5;105;48;5;104m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;105m\xe2\x96\x80\x1b[49m                      \x1b[m",
    "\x1b[49m      \x1b[49;38;5;99m\xe2\x96\x80\x1b[38;5;63;48;5;105m\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\x1b[38;5;99;48;5;62m\xe2\x96\x84\x1b[38;5;105;48;5;62m\xe2\x96\x84\x1b[38;5;105;48;5;63m\xe2\x96\x84\xe2\x96\x84\x1b[38;5;105;48;5;105m\xe2\x96\x84\xe2\x96\x84\x1b[49;38;5;105m\xe2\x96\x80\xe2\x96\x80\x1b[49m                         \x1b[m",
    "\x1b[49m                                          \x1b[m"
};

char **info_lines = NULL;
char **prog_lines = NULL;
	

#define INFO_PUSH_LINE(...) { snprintf(tmp, 128, __VA_ARGS__); info_lines[i] = strdup(tmp); i++; }
#define COLOR_OFF "\033[0m"
#define COLOR_PURPLE "\033[0;35m"

/* CPUID requests */
enum cpuid_requests {
    CPUID_GETVENDORSTRING,
    CPUID_GETFEATURES,
    CPUID_GETTLB,
    CPUID_GETSERIAL,

    CPUID_INTELEXTENDED = 0x80000000,
    CPUID_INTELFEATURES,
    CPUID_INTELBRANDSTRING,
    CPUID_INTELBRANDSTRINGMORE,
    CPUID_INTELBRANDSTRINGEND,

    CPUID_INTELADDRSIZE = 0x80000008,
};

char *cpu_getBrandString() {
    char brand[48];
    snprintf(brand, 10, "Unknown");

	// Get using CPUID
#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)
	#define __cpuid(code, a, b, c, d) asm volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(code))
    uint32_t eax, unused;
    __cpuid(CPUID_INTELEXTENDED, eax, unused, unused, unused);
    if (eax >= CPUID_INTELBRANDSTRINGEND) {
        // Supported!
        uint32_t brand_data[12];
		__cpuid(0x80000002, brand_data[0], brand_data[1], brand_data[2], brand_data[3]);
		__cpuid(0x80000003, brand_data[4], brand_data[5], brand_data[6], brand_data[7]);
		__cpuid(0x80000004, brand_data[8], brand_data[9], brand_data[10], brand_data[11]);
        memcpy(brand, brand_data, 48);
    }
#endif

    return strdup(brand);
}

void print_ip(int *i_ptr) {
#ifdef __ETHEREAL__
	if (chdir("/device/") < 0) return;

	// Find a networking card
	DIR *d = opendir(".");
	if (!d) return;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strncmp(ent->d_name, "enp", 3)) continue;

		break;
	}

	if (ent == NULL) {
		return;
	}

	// Found one, try it
	int nic = open(ent->d_name, O_RDONLY);
	if (nic < 0) {
		perror("open");
		return;
	}
	
	nic_info_t info;
	if (ioctl(nic, IO_NIC_GET_INFO, &info) < 0) {
		perror("IO_NIC_GET_INFO");
		return;
	}

	// get the IPv4 address
	struct in_addr in;
	in.s_addr = info.nic_ipv4_addr;
	char *in_name = inet_ntoa(in);

	// push it using this garbage
	int i = *i_ptr;
	char tmp[128];
	INFO_PUSH_LINE("Local IP (%s):" COLOR_OFF " %s\n", ent->d_name, in_name);
	*i_ptr = i;

	close(nic);
	closedir(d);
#endif
}

void memory_print_pretty(char *buf, unsigned long long memory) {
	char *unit = "bytes";
	if (memory > 1000) {
		unit = "KB";
		memory /= 1000;

		if (memory > 1000) {
			unit = "MB";
			memory /= 1000;
			
			if (memory > 1000) {
				unit = "GB";
				memory /= 1000;
			}
		}
	}

	sprintf(buf, "%lld %s", memory, unit);
}

void print_memory(int *i_ptr) {
#ifdef __ETHEREAL__
    FILE *kmem = fopen("/system/memory/pmm", "r");
    if (!kmem) {
        perror("/system/memory/pmm");
        return;
    }

    unsigned long long total_memory, used_memory;

    fscanf(kmem,    
        "TotalPhysBlocks:%*d\n"
        "TotalPhysMemory:%zu kB\n"
        "UsedPhysMemory:%zu kB\n"
        "FreePhysMemory:%*zu kB\n"
        "PhysMemoryBytes:%*zu\n", &total_memory, &used_memory);
    fclose(kmem);


    total_memory *= 1024;
    used_memory *= 1024;

	char used[64];
	char tot[64];
	memory_print_pretty(used, used_memory);
	memory_print_pretty(tot, total_memory);
	
	int pct = (int)(((double)used_memory / (double)total_memory) * 100.0);

	char *color = "\033[0;32m";
	if (pct > 50) {
		color = "\033[0;33m";
		if (pct > 75) {
			color = "\033[0;31m";
		}
	}

	int i = *i_ptr;
	char tmp[128];
	INFO_PUSH_LINE("Memory: " COLOR_OFF "%s / %s (%s%d%%" COLOR_OFF ")\n", used, tot, color, pct);
	*i_ptr = i;
#endif
}

void collect_info(int start) {
	int i = start;
	char tmp[128];

	struct utsname utsname;
	if (uname(&utsname) < 0) {
		perror("uname");
		exit(0);
	}

	char hostname[256];
	gethostname(hostname, 256);

    struct passwd *pw = getpwuid(geteuid());

	INFO_PUSH_LINE("%s" COLOR_OFF "@" COLOR_PURPLE "\033[1m%s\n", pw ? pw->pw_name : "unknown", hostname);
	INFO_PUSH_LINE("-------------\n");
	INFO_PUSH_LINE("OS: " COLOR_OFF "Ethereal x86_64\n");
	INFO_PUSH_LINE("Kernel: " COLOR_OFF "%s %s\n", utsname.sysname, utsname.release);
	prog_lines[i] = "essence --version";
	INFO_PUSH_LINE("Shell: " COLOR_OFF);
	INFO_PUSH_LINE("CPU: " COLOR_OFF "%s\n", cpu_getBrandString());

#ifdef __LINUX__
	prog_lines[i] = "uptime -p";
#else
	prog_lines[i] = "uptime";
#endif

	INFO_PUSH_LINE("Uptime: " COLOR_OFF);

	// Get memory
	print_memory(&i);

	// Get IP address
	print_ip(&i);

	// TODO: disk, swap, wm, gpu, etc.

	INFO_PUSH_LINE("\n");
	
	// Put ANSI color codes
	INFO_PUSH_LINE("\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m\033[0;35m\n")
}

int main(int argc, char** argv) {
	int fancy = 1;
	if (argc > 1) fancy = 0;

	size_t arrsize = fancy ? (sizeof(ascii_utf) / sizeof(ascii_utf[0])) : (sizeof(ascii) / sizeof(ascii[0]));
	info_lines = malloc(arrsize * sizeof(char*));
	prog_lines = malloc(arrsize * sizeof(char*));

	for (unsigned i = 0; i < arrsize; i++) {
		info_lines[i] = "\n";
		prog_lines[i] = NULL;
	}


	// Collect info
	collect_info(6);
	

	// Set color code
	printf("\033[0;35m");

	// Print them out
	if (fancy) {
		for (size_t i = 0; i < sizeof(ascii_utf) / sizeof(char*); i++) {
			printf(COLOR_PURPLE "%s\033[0;35m\033[1m%s", ascii_utf[i], info_lines[i]);
			if (prog_lines[i]) {
				fflush(stdout);
				system(prog_lines[i]);
			}
		}
	} else {
		for (size_t i = 0; i < sizeof(ascii) / sizeof(char*); i++) {
			printf(COLOR_PURPLE "%s\033[0;35m\033[1m%s", ascii[i], info_lines[i]);
			if (prog_lines[i]) {
				fflush(stdout);
				system(prog_lines[i]);
			}
		}
	}

	printf("\033[0m\n");

    return 0; 
}