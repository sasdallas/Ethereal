/**
 * @file libpolyhedron/include/stdio.h
 * @brief Standard I/O header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>
_Begin_C_Header

#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/**** TYPES ****/

// Type definitions for xvas_callback
typedef int (*xvas_callback)(void *, char);


/**** DEFINITIONS ****/

/* stdin/stdout/stderr file descriptors */
#define STDIN_FILE_DESCRIPTOR       0
#define STDOUT_FILE_DESCRIPTOR      1
#define STDERR_FILE_DESCRIPTOR      2

/* POSIX */
#define STDIN_FILENO                STDIN_FILE_DESCRIPTOR
#define STDOUT_FILENO               STDOUT_FILE_DESCRIPTOR
#define STDERR_FILENO               STDERR_FILE_DESCRIPTOR

/* libc configuration option */
#define READ_BUFFER_SIZE        8192
#define WRITE_BUFFER_SIZE       8192

/* Seek settings */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Modes for setvbuf */
#define _IONBF      0
#define _IOLBF      1
#define _IOFBF      2       

/* EOF */
#define EOF (-1)


/**** TYPES ****/

/* No, this doesn't match GLIBC's version */
typedef struct _iobuf {
    int fd;                 // File descriptor number

    char *rbuf;             // Read buffer
    size_t rbufsz;          // Read buffer size 
    int eof;                // End of file


    char *wbuf;             // File write buffer
    size_t wbuflen;         // Length of buffer contents
    size_t wbufsz;          // Total buffer size
} FILE;


/* External files */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/**** FUNCTIONS ****/

int putchar(int ch);
int printf(const char * __restrict, ...);
int puts(const char *);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int snprintf(char * str, size_t size, const char * format, ...);
int sprintf(char * str, const char * format, ...);
size_t xvasprintf(xvas_callback callback, void * userData, const char * fmt, va_list args);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int fprintf(FILE *f, const char *fmt, ...);

int getc(FILE *stream);
int getchar();

FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int fflush(FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
long ftell(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);

void setbuf(FILE *stream, char *buf);
void setbuffer(FILE *stream, char *buf, size_t size);
void setlinebuf(FILE *stream);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

void perror(const char *s);

int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *str, const char *format, ...);

#endif

_End_C_Header