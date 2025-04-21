/**
 * @file hexahedron/include/kernel/config.h
 * @brief Kernel configuration header
 * 
 * @note THIS FILE DOES NOT CONTAIN CONFIGURATION DATA. SEE CONFIG.C
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H


/* This just exposes things exposed in config.c, which is remade every build. */
extern const char *__kernel_version_format;
extern const char *__kernel_version_codename;
extern const char *__kernel_build_date, *__kernel_build_time;
extern const char *__kernel_build_configuration;
extern const char *__kernel_architecture; 
extern const char *__kernel_compiler;
extern const char *__kernel_ascii_art_formatted;


// Versioning information
extern const int __kernel_version_major;
extern const int __kernel_version_minor;
extern const int __kernel_version_lower;

// Debug settings
extern const int __debug_output_com_port, __debug_output_baud_rate;
extern const int __debugger_com_port, __debugger_baud_rate, __debugger_enabled, __debugger_wait_time;



#endif