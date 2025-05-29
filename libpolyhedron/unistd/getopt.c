/**
 * @file libpolyhedron/unistd/getopt.c
 * @brief getopt, getopt_long
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>

/* Option variables */
char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

/* Resume searching pointer */
static char *r = NULL;

int getopt_long(int argc, char *argv[], const char *optstring, const struct option *longopts, int *longindex) {
    if (optind >= argc) return -1;

    // Does this argument matter?
    char *p = argv[optind];
    
    if (r) {
        p = r;
        goto _resume_continue;
    }
    
    // Start checking
    r = p;
    if (*p != '-') return -1;  // Nope
    
    p++;
    r++;
    if (*p == '-') {
        if (!(*(p+1))) {
            // We have a lonely "--", end of arguments
            optind++;
            return -1;
        }

        // Long argument detected, let's find the corresponding longopt
        if (longopts) {
            p++;

            // Copy to a temporary buffer
            char tmp[strlen(p)+1];
            strcpy(tmp, p);

            // Locate the equals sign if it has one
            char *eq = strchr(tmp, '=');
            if (eq) {
                // We have an equal sign!
                optarg = p + ((eq+1)-tmp);
            
                // Null the equal sign as well
                *eq = 0;
            } else {
                // No, can we get it later?
                optarg = NULL;
            }

            // Find the long option corresponding to our argument
            int i = 0;
            int found = 0;
            while (longopts[i].name) {
                if (!strcmp(longopts[i].name, tmp)) {
                    found = 1;
                    break;
                }
                i++;
            }

            if (!found) {
                optind++;
                optopt = 0;
                r = NULL;
                if (longindex) *longindex = -1;

                // Print error message
                if (opterr) {
                    fprintf(stderr, "Unknown argument: %s\n", tmp);
                }

                return '?';
            }

            // Does it have an argument?
            if (longopts[i].has_arg && !optarg && optind+1 < argc) {
                // Get next optarg
                optarg = argv[optind+1];
                optind++;
            }

            // Do we need to set longindex?
            if (longindex) *longindex = i;

            r = NULL;
            optind++;

            if (!longopts[i].flag) return longopts[i].val;
            *longopts[i].flag = longopts[i].val;
            return 0;
        }

        // I guess just fall through?
    }

_resume_continue:

    // Is there anything beyond this?
    if (*p == 0) {
        // No, recurse I guess
        r = NULL;
        optind++;
        return getopt_long(argc, argv, optstring, longopts, longindex);
    }

    // We have a character, is it valid?
    if (!(isalpha(*p) || *p == '?')) {
        // No, its not valid.
        if (opterr) fprintf(stderr, "Invalid option: %c\n", *p);
        optopt = *p;
        optind++;
        r = p+1;
        return '?';
    }

    // Try and find the character in optstring
    char *c = strchr(optstring, *p);
    if (!c) {
        // It's not valid.
        if (opterr) fprintf(stderr, "Invalid option: %c\n", *p);
        optopt = *p;
        optind++;
        r = p+1;
        return '?';
    }

    // TODO: Support other parts of optstring

    // Do we need another argument?
    if (*(c+1) == ':') {
        // Yes, but will we find it here?
        if (*(p+1) == 0) {
            // Nope, probably the next argument?
            if (optind+1 < argc) {
                optarg = argv[optind+1];
                optind += 2;
            }
        } else {
            optarg = p + 1;
            optind++;
        }

        r = NULL;
    } else {
        r = p+1;
    }

    return *p;
}

int getopt(int argc, char * const argv[], const char *optstring) {
    return getopt_long(argc, (char**)argv, optstring, NULL, 0);
}