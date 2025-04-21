/**
 * @file libpolyhedron/stdlib/strtoll.c
 * @brief strtoll
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static int isvalid(int base, int ch) {
    if (tolower(ch) >= 'a' && tolower(ch) < 'a' + (base - 10)) return 1;
    if (ch >= '0' && ch <= '9') return 1;

    return 0;
}

// NOTE: Most implementations, specifically of strtoull/strtoul/strtol/strtoll are exactly the same,
// except for a few areas.
long long int strtoll(const char *str, char **endptr, int base) {
    // First step, if the base is invalid skip past it
    if (base < 0 || base == 1 || base > 36) return LLONG_MAX; // TODO: Set errno?

    // Discard whitespace characters
    char *ptr = (char*)str;
    while (*ptr && isspace(*ptr)) ptr++;

    // Check the sign (+ or -)
    int sign = 1; // + assumed
    if (*ptr == '-') {
        sign = -1;
        ptr++;
    } else if (*ptr == '+') {
        ptr++; // already set
    }

    // Now that we're past the sign, if the base is 16 we should skip past that prefix
    if (base == 16) {
        // Check for an '0x' or '0X' prefix
        if (*ptr == '0') {
            // This 0 doesn't matter anyways, skip past it.
            ptr++;
            if (*ptr == 'x' || *ptr == 'X') {
                ptr++; // Again skip past it
            }
        }
    }

    // Some base == 0 shenanigans that strtol is supposed to do.
    // https://cplusplus.com/reference/cstdlib/strtol/

    if (base == 0) {
        // We've already handled the sign character. We just have two things left to check
        // The first is for the octal/hexadecimal prefix.
        if (*ptr == '0') {
            // We know it might be octal
            base = 8;
            ptr++;

            if (*ptr == 'x' || *ptr == 'X') {
                // Hexadecimal
                base = 16;
                ptr++;
            }
        } else {
            base = 10; // Assume decimal
        }
    }

    // VARIES
    long long int output = 0;

    // Now we should start going over each character in the pointer
    // Invalid conversions are handled fine
    while (isvalid(base, *ptr)) {
        // If it's a number then we just subtract 0, else we'll just subtract 'a' and add 10.
        output *= base;
        if (isdigit(*ptr)) {
            output += (*ptr - '0');
        } else {
            output += (tolower(*ptr) - 'a' + 10);
        }

        ptr++;
    }

    // Handle endptr
    if (endptr) *endptr = ptr;

    // Handle sign
    if (sign == -1) return -output;
    return output;
}

