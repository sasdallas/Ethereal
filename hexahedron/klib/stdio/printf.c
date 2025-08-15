/**
 * @file hexahedron/klib/stdio/printf.c
 * @brief printf and friends
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 * 
 * @copyright
 * Note that portions of this file may be sourced from ToaruOS which uses the NCSA license.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

/* Temporary */
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* Output macro */
#define OUT(c) ({ callback(user, (c)); written++; })

/* Hexadecimal character array */
static char __hex_char_arr[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

/**
 * @brief Print a decimal number
 * @param callback xvasprintf callback
 * @param user Data for xvasprintf callback
 * @param value The value pulled from the printf system
 * @param width Width of the number
 * @param padding Padding to use
 * @param justification 0 right, 1 left
 * @param precision Precision
 */
static size_t __printf_decimal(int (*callback)(void*,char), void *user, uint64_t value, unsigned int width,  char padding, int justification, int precision) {
	size_t written = 0;
	if (precision == -1) precision = 1;

	// We have a total width but we need to calculate integer width
	size_t int_width = 1; // Default to 1 for the case where value is 0
	uint64_t tmp = value;
	while (tmp >= 10) {
		tmp /= 10;
		int_width++;
	}

	// Cool, should we left-pad?
	if (width && !justification) {
		// Yes, do the padding.
		for (int i = 0; i < (int)(width - int_width); i++) OUT(padding);
	}

	// Do we need to pad with leading zeoes?
	if (int_width < (size_t)precision) {
		for (size_t i = int_width; i < (size_t)precision; i++) {
			OUT('0');
		}
	}

	// Now we can print out the integer
	// Because of the way we do this we have to reverse the buffer as well
	char rev_buf[int_width];

	unsigned idx = 0;
	while (idx < int_width) {
		unsigned char ch = ((char)((value % 10)) + '0');
		rev_buf[int_width-idx-1] = ch;
		value /= 10;
		idx++;
	}

	for (unsigned i = 0; i < int_width; i++) OUT(rev_buf[i]);


	// Cool, should we right-pad?
	if (width && justification) {
		// Yes, do the padding.
		for (unsigned i = 0; i < width - int_width; i++) OUT(padding);
	}

	return written;
}

/**
 * @brief Hexadecimal print
 * @param callback xvasprintf callback
 * @param user Data for xvasprintf callback
 * @param value The value pulled from the printf system
 * @param width Width of the number
 * @param padding Padding to use
 * @param prefix Whether to use 0x prefix
 * @param upper Uppercase
 * @param justification Justification
 */
static size_t __printf_hexadecimal(int (*callback)(void*,char), void* user, uint64_t value, unsigned int width, char padding, int prefix, int upper, int justification) {
	size_t written = 0;

	// We have a total width but we need to calculate integer width
	size_t int_width = 1; // Default to 1 for the case where value is 0
	uint64_t tmp = value;
	while (tmp >= 16) {
		tmp /= 16;
		int_width++;
	}

	if (prefix && !justification && padding == '0') {
		OUT('0');
		OUT(upper ? 'X' : 'x');
	}

	// Cool, should we left-pad?
	if (width && !justification) {
		// Yes, do the padding.
		for (int i = 0; i < (int)(width - int_width); i++) OUT(padding);
	}

	if (prefix && !justification && padding == ' ') {
		OUT('0');
		OUT(upper ? 'X' : 'x');
	}


	unsigned idx = 0;
	while (idx < int_width) {
		unsigned char ch = __hex_char_arr[(value >> ((int_width-idx-1)*4)) & 0xF];
		OUT(upper ? toupper(ch) : ch);
		idx++;

	}

	
	// Cool, should we right-pad?
	if (width && justification) {
		// Yes, do the padding.
		for (int i = 0; i < (int)(width - int_width); i++) OUT(padding);
	}


	return written;
}

size_t __xvasprintf(int (*callback)(void *, char), void *user, const char * fmt, va_list args) {
	if (!fmt || !callback) return 0;
	size_t written = 0;

	// Start looping through the format string
	char *f = (char*)fmt;
	while (*f) {
		// If it doesn't have a percentage sign, just print the character
		if (*f != '%') {
			OUT(*f);
			f++;
			continue;
		}

		// We found a percentage sign. Skip past it.
		f++;

		// Real quick - is this another percentage sign? We'll still have a case to catch this on format specifiers, but this will be faster
		if (*f == '%') {
			// Yes, just print that and go back
			OUT(*f);
			f++;
			continue;
		}

		// From now on, the printf argument should be in the form:
		// %[flags][width][.precision][length][specifier]
		// For more information, please see: https://cplusplus.com/reference/cstdio/printf/#google_vignette

		#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

		// Start parsing flags
		int justification = 0;		// 0 = right, 1 = left
		int sign_type = 0;			// 0 = sign only on negative, 1 = sign always, 2 = sign never
		char padding = ' ';			// Default padding is with spaces
		int add_special_chars = 0;	// Depends on specifier
		while (*f) {
			int finished_reading = 0;

			switch (*f) {
				case '-':
					// Left-justify width. This means that we will pad after the value, instead of before. 
					justification = 1;
					break;

				case '+':
					// Always sign
					sign_type = 1;
					break;

				case ' ':
					// Never sign
					sign_type = 2;
					break;

				case '0':
					// Left-pad with zeroes instead of spaces when padding is specified in width.
					padding = '0';
					break;

				case '#':
					// This is a special case. If used with %o or %x it adds a 0/0x - if used with %a/%e/%f/%g it forces decimal points
					add_special_chars = 1;
					break;

				default:
					// Do nothing
					finished_reading = 1;
					break;
			}

			if (finished_reading) break;
			f++;
		}

		// Width should be read indefinitely as long as the characters are available
		// TODO:  ... better way?
		int width = 0;

		// Special case: Width is provided in va_args
		if (*f == '*') {
			width = va_arg(args, int);
			f++;
		} else  {
			while (isdigit(*f)) {
				// Add this to the width
				width *= 10;
				width += (int)((*f) - '0');
				f++;
 			}
		}

		// Do we need to process precision?
		int precision = -1; // Precision will be auto-updated when we handle
		if (*f == '.') {
			// Yes we do!
			precision = 0;
			f++;

			// For integer specifiers (d, i, o, u, x) precision specifies the minimum number of digits to be written.
			// If it shorter than this, it is padded with leading zeroes.
			// For %a/%e/%f this is the number of digits to printed AFTER the decimal point (default 6)
			// For %g this is the maximum number of significant digits
			// For %s this is the maximum number of characters to print

			// If it is '*' grab it from VA arguments
			if (*f == '*') {
				precision = va_arg(args, int);
				f++;
			} else {
				// Keep reading the precision
				while (isdigit(*f)) {
					// Add this to the width
					precision *= 10;
					precision += (int)((*f) - '0');
					f++;
				}
			}
		}

		// Length modifies the length of the data type being interpreted by VA arguments.
		// There's a nice table available at https://cplusplus.com/reference/cstdio/printf/.
		// Each specifier interprets length differently, so I'll use a value to show that
		
		/* Starting from 0, length represents: (none), h, hh, l, ll, j, z, t, L */
		int length = 0;		// By default, 0 is none
		switch (*f) {
			case 'h':
				length = 1;
				f++;
				if (*f == 'h') {
					length = 2;
					f++;
				}
				break;
			case 'l':
				length = 3;
				f++;
				if (*f == 'l') {
					length = 4;
					f++;
				}
				break;
			case 'j':
				length = 5;
				f++;
				break;
			case 'z':
				length = 6;
				f++;
				break;
			case 't':
				length = 7;
				f++;
				break;
			case 'L':
				length = 8;
				f++;
				break;
			default:
				break;
		}

		// Now we're on to the format specifier.
		// TODO: Support for %g, %f
		switch (*f) {
			case 'd':
			case 'i': ;
				// %d / %i: Signed decimal integer
				long long dec;

				// Get value
				if (length == 7) {
					dec = (ptrdiff_t)(va_arg(args, ptrdiff_t));
				} else if (length == 6) {
					dec = (size_t)(va_arg(args, size_t));
				} else if (length == 5) {
					dec = (intmax_t)(va_arg(args, intmax_t));
				} else if (length == 4) {
					dec = (long long int)(va_arg(args, long long int));
				} else if (length == 3) {
					dec = (long int)(va_arg(args, long int));
				} else {
					// The remaining specifiers are all promoted to integers regardless
					dec = (int)(va_arg(args, int));
				}

				if (dec < 0) {
					// Handle negative sign
					if (sign_type != 2) {
						OUT('-');
					} 

					// Invert
					dec = -dec;
				} else if (sign_type == 1) {
					// Always sign
					OUT('+');
				}

				written += __printf_decimal(callback, user, dec, width, padding, justification, precision);
				break;
			
			case 'p': ;
				// %p: Pointer 
				unsigned long long ptr;
				if (sizeof(void*) == sizeof(long long)) {
					ptr = (unsigned long long)(va_arg(args, unsigned long long));
				} else {
					ptr = (unsigned int)(va_arg(args, unsigned int));
				}
				written += __printf_hexadecimal(callback, user, ptr, width, padding, 1, isupper(*f), justification);
				break;
			
			case 'x':
			case 'X': ;
				// %x: Hexadecimal format
				unsigned long long hex;
				if (length == 7) {
					hex = (ptrdiff_t)(va_arg(args, ptrdiff_t));
				} else if (length == 6) {
					hex = (size_t)(va_arg(args, size_t));
				} else if (length == 5) {
					hex = (uintmax_t)(va_arg(args, uintmax_t));
				} else if (length == 4) {
					hex = (unsigned long long int)(va_arg(args, unsigned long long int));
				} else if (length == 3) {
					hex = (unsigned long int)(va_arg(args, unsigned long int));
				} else {
					// The remaining specifiers are all promoted to integers regardless
					hex = (unsigned int)(va_arg(args, unsigned int));
				}

				written += __printf_hexadecimal(callback, user, hex, width, padding, add_special_chars, isupper(*f), justification);
				break;

			case 'u': ;
				// %u: Unsigned
				unsigned long long uns;
				if (length == 7) {
					uns = (ptrdiff_t)(va_arg(args, ptrdiff_t));
				} else if (length == 6) {
					uns = (size_t)(va_arg(args, size_t));
				} else if (length == 5) {
					uns = (uintmax_t)(va_arg(args, uintmax_t));
				} else if (length == 4) {
					uns = (unsigned long long int)(va_arg(args, unsigned long long int));
				} else if (length == 3) {
					uns = (unsigned long int)(va_arg(args, unsigned long int));
				} else {
					// The remaining specifiers are all promoted to integers regardless
					uns = (unsigned int)(va_arg(args, unsigned int));
				}

				written += __printf_decimal(callback, user, uns, width, padding, justification, precision);
				break;

			case 's': ;
				// %s: String
				char *str = (char*)(va_arg(args, char*));
				if (str == NULL) {
					// Nice try
					str = "(NULL)";
				}

				// Padding applies
				int chars_printed = 0;

				// Does it have precision?
				if (precision >= 0) {
					for (int i = 0; i < precision && *str; i++) {
						OUT(*str);
						str++;
						chars_printed++;
					}
				} else {
					while (*str) {
						OUT(*str);
						str++;
						chars_printed++;
					}
				}

				// Does it have width?
				if (chars_printed < width) {
					for (int i = chars_printed; i < width; i++) {
						// Add some padding
						OUT(padding);
					}
				}
				break;
			
			case 'c': ;
				// %c: Singled character
				OUT((char)(va_arg(args, int)));
				break;

		#ifndef __LIBK
			case 'g':
				fprintf(stderr, "xvasprintf: %%g format is not supported");
				break;

			case 'f':
				fprintf(stderr, "xvasprintf: %%f format is not supported");
				break;
		#endif

			default: ;
				OUT(*f);
				break;
		}

		// Next character
		f++;
	}
	
	return written;
}

/* This is an alias to fix building binutils */
/* libiberty provides its own xvasprintf, but the kernel uses it too so we can just use this */
size_t __attribute__((weak, alias("__xvasprintf"))) xvasprintf(int (*callback)(void *, char), void *user, const char *fmt, va_list ap);

/* We will pass this to our __vsnprintf_callback function */
struct __vsnprintf_data {
	char *string;
	int index;
	int n;
};

int __vsnprintf_callback(void *u, char ch) {
	struct __vsnprintf_data *d = (struct __vsnprintf_data*)u;

	if (!(d->n == -1) && (d->n-1) - d->index) {
		// (v)snprintf
		d->string[d->index] = ch;
		d->index++;
		if (d->index < d->n) d->string[d->index] = 0; // Null terminate if we have enough room
	} else if (d->n == -1) {
		// sprintf 
		d->string[d->index++] = ch;
	}

	return 0;
}

int vsnprintf(char *s, size_t n, const char *format, va_list ap) {
	struct __vsnprintf_data dat = {
		.string = s,
		.index = 0,
		.n = n
	};

	int ret = __xvasprintf(__vsnprintf_callback, &dat, format, ap);

	// Usually the NULL termination is left out
	__vsnprintf_callback(&dat, 0);
	return ret;
}

int snprintf(char *str, size_t size, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	return ret;
}

int sprintf(char *str, const char *format, ...) {
	va_list ap;
	va_start(ap, format);

	struct __vsnprintf_data dat = {
		.string = str,
		.index = 0,
		.n = -1,
	};

	int ret = __xvasprintf(__vsnprintf_callback, &dat, format, ap);
	va_end(ap);

	// Usually the NULL termination is left out
	__vsnprintf_callback(&dat, 0);
	return ret;
}

#ifdef __LIBK

int __printf_callback(void *user, char ch) {
extern int kernel_in_panic_state;
	if (kernel_in_panic_state) return 0;

extern int terminal_print(void *user, char ch);
	return terminal_print(user, ch);
}

int printf(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int ret = __xvasprintf(__printf_callback, NULL, format, ap);
	va_end(ap);
	return ret;
}

#else

int printf(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int ret = vprintf(format, ap);
	va_end(ap);
	return ret;
}

#endif