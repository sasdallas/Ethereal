/**
 * @file userspace/lib/include/ethereal/log.h
 * @brief Stub for proper logging framework
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef ETHEREAL_LOG_H
#define ETHEREAL_LOG_H

/**** INCLUDES ****/
#include <stdio.h>


/**** MACROS ****/

#ifndef APP_NAME
#define APP_NAME "n/a"
#endif

// TODO: probably something more than this
#ifndef DISABLE_LOG
#define TRACE_INFO(fmt, ...) ({ fprintf(stderr, "(" APP_NAME ") %s:%03d: \033[34minfo:\033[0m " fmt, __FILE__, __LINE__, ## __VA_ARGS__); })
#define TRACE_WARN(fmt, ...) ({ fprintf(stderr, "(" APP_NAME ") %s:%03d: \033[1;33mwarning:\033[0m " fmt, __FILE__, __LINE__, ## __VA_ARGS__); })
#define TRACE_ERROR(fmt, ...) ({ fprintf(stderr, "(" APP_NAME ") %s:%03d: \033[1;31merror:\033[0m " fmt, __FILE__, __LINE__, ## __VA_ARGS__); })
#define TRACE_DEBUG(fmt, ...) ({ fprintf(stderr, "(" APP_NAME ") %s:%03d: \033[0;37mdebug:\033[0m " fmt, __FILE__, __LINE__, ## __VA_ARGS__); })
#else
#define TRACE_INFO(...)
#define TRACE_WARN(...)
#define TRACE_ERROR(...)
#define TRACE_DEBUG(...)
#endif

#endif