/**
 * @file libpolyhedron/include/getopt.h
 * @brief getopt
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GETOPT_H
#define _GETOPT_H

/**** TYPES ****/

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

/**** DEFINITIONS ****/
#define no_argument 0
#define required_argument 1
#define optional_argument 2

/**** VARIABLES ****/

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;

/**** FUNCTIONS ****/

int getopt_long(int argc, char *argv[],
    const char *optstring,
    const struct option *longopts, int *longindex);

int getopt_long_only(int argc, char *argv[],
    const char *optstring,
    const struct option *longopts, int *longindex);


#endif