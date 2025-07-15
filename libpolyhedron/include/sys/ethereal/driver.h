/**
 * @file libpolyhedron/include/sys/ethereal/driver.h
 * @brief Ethereal driver API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
* Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_ETHEREAL_DRIVER_H
#define _SYS_ETHEREAL_DRIVER_H

/**** INCLUDES ****/
#include <kernel/loader/driver.h>

/**** TYPES ****/

typedef struct ethereal_driver {
    pid_t id;                   // Driver ID
    char *filename;             // Exact filename of the driver
    uintptr_t base;             // Base address in memory
    size_t size;                // Size in memory
    driver_metadata_t metadata; // Exact metadata copy
} ethereal_driver_t;

/**** FUNCTIONS ****/

/**
 * @brief Load a new driver into the Hexahedron kernel
 * @param filename The filename of the driver to load
 * @param priority The priority of the driver to load
 * @param argv The NULL-terminated argument list to the driver
 * @returns Driver ID on success, -1 on error (errno is set)
 */
pid_t ethereal_loadDriver(char *filename, int priority, char **argv);

/**
 * @brief Unload a driver from the Hexahedron kernel
 * @param id The ID of the driver to unload
 * @returns 0 on success, -1 on error
 */
int ethereal_unloadDriver(pid_t id);

/**
 * @brief Get driver information
 * @param id The ID of the driver to get information on
 * @returns Driver information on success, NULL with errno set on error
 */
ethereal_driver_t *ethereal_getDriver(pid_t id);

#endif

_End_C_Header