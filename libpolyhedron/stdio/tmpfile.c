/**
 * @file libpolyhedron/stdio/tmpfile.c
 * @brief tmpfile
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#include <stdio.h>
#include <unistd.h>

/* file number */
static int __tmpfile_num = 0;

FILE *tmpfile() {
	char path[128];
	snprintf(path, 128, "/tmp/tmp%d.%d", getpid(), __tmpfile_num);

	// Create the file
	FILE *f = fopen(path, "w");
	if (f) __tmpfile_num++;
	return f;
}
