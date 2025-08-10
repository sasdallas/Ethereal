/**
 * @file libpolyhedron/time/timezone.c
 * @brief timezone functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <time.h>

int daylight;
long timezone;
char* tzname[2];

void tzset() {
	// Reset to default
	daylight = 0;
	timezone = 0;
	tzname[0] = "UTC";
	tzname[1] = "UTC";
}

