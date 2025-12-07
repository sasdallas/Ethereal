/**
 * @file hexahedron/include/kernel/debug.h
 * @brief Debugger header file
 * 
 * Provides interface to kernel debugger and logger. 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <kernel/fs/vfs.h>

/**** TYPES ****/
typedef int (*log_putchar_method_t)(void *user, char ch); // Put character method used by logger

typedef enum {
    NOHEADER = 0,       // Do not use any header, including file/timestamp/etc. This is mainly used for some "cool" formatting.
    INFO = 1,           // Prefix with [INFO]
    WARN = 2,           // Prefix with [WARN]
    ERR = 3,            // Prefix with [ERR ]
    DEBUG = 4,          // Prefix with [DBG ]
} DEBUG_LOG_TYPE;

/**** DEFINITIONS ****/

// NOTE: These colors won't actually be used by dprintf. You have to manually specify them.
#define INFO_COLOR_CODE     "\033[36m\033[36m"
#define WARN_COLOR_CODE     "\033[33m\033[33m"
#define ERR_COLOR_CODE      "\033[31m\033[31m"
#define DEBUG_COLOR_CODE    "\033[37m\033[37m"

// Some other color codes
#define COLOR_CODE_RESET        "\033[0m"
#define COLOR_CODE_RED          "\033[0;31m"
#define COLOR_CODE_RED_BOLD     "\033[1;31m"
#define COLOR_CODE_YELLOW       "\033[0;33m"
#define COLOR_CODE_YELLOW_BOLD  "\033[1;33m"
#define COLOR_CODE_GREEN        "\033[0;32m"
#define COLOR_CODE_GREEN_BOLD   "\033[1;32m"

#define DEBUG_CONSOLE_PATH      "/device/kconsole"


/**** FUNCTIONS ****/

struct fs_node;

size_t xvasprintf(int (*callback)(void*, char), void * userData, const char * fmt, va_list args);

#ifdef __INTELLISENSE__
/**
 * @brief Print something to the debug log
 * 
 * @param status Whether this is debugging information, normal information, errors, or warnings. @see DEBUG_LOG_TYPE
 * @param format The text to print
 */
void dprintf(DEBUG_LOG_TYPE status, char *fmt, ...);

/**
 * @brief Print something to the debug log from a specific module
 * 
 * @param status Whether this is debugging information, normal information, errors, or warnings. @see DEBUG_LOG_TYPE
 * @param module The module this is coming from
 * @param format The text to print
 */
void dprintf_module(DEBUG_LOG_TYPE status, char *module, char *fmt, ...);
#else
#define dprintf(status, format, ...) dprintf_internal(NULL, status, format, ## __VA_ARGS__)
#define dprintf_module(status, module, format, ...) dprintf_internal(module, status, format, ## __VA_ARGS__)
#endif

/**
 * @brief Internal function to print to debug line.
 * 
 * You can call this but it's not recommended. Use dprintf().
 */
int dprintf_internal(char *module, DEBUG_LOG_TYPE status, char *format, ...);

/**
 * @brief dprintf that accepts va_args instead
 */
int dprintf_va(char *module, DEBUG_LOG_TYPE status, char *format, va_list ap);

/**
 * @brief Set the debug putchar method
 * @param logMethod A type of putchar method. Returns an integer and takes a character.
 */
void debug_setOutput(log_putchar_method_t logMethod);

/**
 * @brief Get the debug putchar method.
 */
log_putchar_method_t debug_getOutput();

/**
 * @brief Function to print debug string
 */
int debug_print(void *user, char ch);

/**
 * @brief Write function for debug console node
 * @param node The node
 * @param offset The offset
 * @param size The size of how much to write
 * @param buffer The buffer
 */
ssize_t debug_write(struct fs_node *node, off_t offset, size_t size, uint8_t *buffer);

/**
 * @brief Read function for debug console node
 * @param node The node
 * @param offset The offset
 * @param size The size of how much to read
 * @param buffer The buffer
 */
ssize_t debug_read(struct fs_node *node, off_t offset, size_t size, uint8_t *buffer);


#endif