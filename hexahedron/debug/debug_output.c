/**
 * @file hexahedron/debug/debug_output.c
 * @brief Debug log interface
 * 
 * @note The dprintf() macro is defined in debug.h
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/debug.h>
#include <kernel/drivers/clock.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/alloc.h>
#include <kernel/gfx/term.h>
#include <kernel/mem/mem.h>
#include <kernel/misc/args.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* TODO: This should be replaced with a VFS node */
static log_putchar_method_t debug_putchar_method = NULL; 

static fs_node_t debug_node = {
    .name = "kconsole",
    .flags = VFS_CHARDEVICE,
    .read = debug_read,
    .write = debug_write
};

/* Spinlock */
static spinlock_t debug_lock = { 0 };

/* Debug buffer */
char *debug_buffer = NULL;
size_t debug_buffer_size = 0;
size_t debug_buffer_index = 0; 

/* Push character into debug buffer */
#define DEBUG_PUSH(ch) ({ debug_buffer[debug_buffer_index++] = (ch); if (debug_buffer_index >= debug_buffer_size) { debug_buffer = krealloc(debug_buffer, debug_buffer_size + PAGE_SIZE); debug_buffer_size += PAGE_SIZE; }; debug_buffer[debug_buffer_index] = 0; })

/**
 * @brief Write to buffer
 * @todo Use storage buffer like stdio?
 */
static int debug_write_buffer(size_t length, char *buffer) {
    for (size_t i = 0; i < length; i++) {
        debug_print(NULL, buffer[i]);
    }

    return 0;
}

/**
 * @brief Function to print debug string
 */
int debug_print(void *user, char ch) {
extern int kernel_in_panic_state;
    if (kernel_in_panic_state) {
        terminal_print(NULL, ch);
    }

    /* First, write the character to debug buffer */
    if (debug_buffer) {
        if (ch == '\n') DEBUG_PUSH('\r');
        DEBUG_PUSH(ch);
    }

    if (!debug_putchar_method) return 0; // Log not yet initialized or failed to initialize correctly.

    /* Account for CRLF. It doesn't hurt any terminals (that I know of) */
    if (ch == '\n') debug_putchar_method(NULL, '\r');

    return debug_putchar_method(NULL, ch);
}


/**
 * @brief Read function for debug console node
 * @param node The node
 * @param offset The offset
 * @param size The size of how much to read
 * @param buffer The buffer
 */
ssize_t debug_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!debug_buffer) return -EINVAL;
    if (offset > (off_t)debug_buffer_index) return 0;
    if (offset + size > (size_t)debug_buffer_index) size = debug_buffer_index - (size_t)offset;

    spinlock_acquire(&debug_lock);
    memcpy(buffer, debug_buffer + offset, size);
    spinlock_release(&debug_lock);

    return size;
}

/**
 * @brief Write function for debug console node
 * @param node The node
 * @param offset The offset
 * @param size The size of how much to write
 * @param buffer The buffer
 */
ssize_t debug_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    spinlock_acquire(&debug_lock);
    ssize_t r = debug_write_buffer(size, (char*)buffer);
    debug_node.length = debug_buffer_index;
    spinlock_release(&debug_lock);
    return r;
}


/**
 * @brief dprintf that accepts va_args instead
 */
int dprintf_va(char *module, DEBUG_LOG_TYPE status, char *format, va_list ap) {
    if (!debug_putchar_method) return 0;

    // Wait for our lock
    // !!!: This should not be using a lock. System will deadlock.
    if (debug_lock.cpu != arch_current_cpu()) {
        spinlock_acquire(&debug_lock);
    }

    if (status == NOHEADER) goto _write_format;

    // Get the header we want to use
    char header_prefix[5]; // strncpy?
    switch (status) {
        case INFO:
            strncpy(header_prefix, "INFO", 4);
            break;
        case WARN:
            strncpy(header_prefix, "WARN", 4);
            break;
        case ERR:
            strncpy(header_prefix, "ERR ", 4);
            break;
        case DEBUG:
            strncpy(header_prefix, "DBG ", 4);
            break;
        default:
            strncpy(header_prefix, "??? ", 4);
    }

    header_prefix[4] = 0;

    // If the clock driver isn't ready we just print a blank header.
    char header[129];
    size_t header_length = 0;
    if (!clock_isReady()) {
        if (module) {
            header_length = snprintf(header, 128, "[no clock ready] [%s] [%s] ", header_prefix, module);
        } else {
            header_length = snprintf(header, 128, "[no clock ready] [%s] ", header_prefix);
        }
    } else {
        time_t rawtime;
        time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);
        
        if (module) {
            header_length = snprintf(header, 128, "[%s] [CPU%i] [%s] [%s] ", asctime(timeinfo), arch_current_cpu(), header_prefix, module);
        } else {
            header_length = snprintf(header, 128, "[%s] [CPU%i] [%s] ", asctime(timeinfo), arch_current_cpu(), header_prefix);
        }
    }

    debug_write_buffer(header_length, header);

_write_format: ;
    int returnValue = xvasprintf(debug_print, NULL, format, ap);

    spinlock_release(&debug_lock);
    return returnValue;
}

/**
 * @brief Internal function to print to debug line.
 * 
 * You can call this but it's not recommended. Use dprintf().
 */
int dprintf_internal(char *module, DEBUG_LOG_TYPE status, char *format, ...) {
    if (!debug_getOutput()) return 0;
    va_list ap;
    va_start(ap, format);
    int returnValue = dprintf_va(module, status, format, ap);
    va_end(ap);
    return returnValue;
}

/**
 * @brief Set the debug putchar method
 * @param logMethod A type of putchar method. Returns an integer and takes a character.
 */
void debug_setOutput(log_putchar_method_t logMethod) {
    debug_putchar_method = logMethod;    
}

/**
 * @brief Get the debug putchar method.
 */
log_putchar_method_t debug_getOutput() {
    return debug_putchar_method;
}

/**
 * @brief Mount the debug node onto the VFS
 */
void debug_mountNode() {
    // /* Allocate the debug buffer */
    // if (!kargs_has("--no-store-debug")) {
    //     debug_buffer = kmalloc(PAGE_SIZE);
    //     debug_buffer_size = PAGE_SIZE;
    //     debug_buffer_index = 0;
    // }

    vfs_mount(&debug_node, DEBUG_CONSOLE_PATH);

    dprintf(INFO, "Debug buffer initialized - all content is being stored in memory.\n");
}