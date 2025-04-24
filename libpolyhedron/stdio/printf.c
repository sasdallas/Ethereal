/**
 * @file libpolyhedron/stdio/printf.c
 * @brief printf() implementation
 * 
 * @todo This file needs a cleanup and rewrite. I didn't make it!!
 * 
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2011-2021 K. Lange
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



#define OUT(c) do { callback(userData, (c)); written++; } while (0)
static size_t print_dec(unsigned long long value, unsigned int width, int (*callback)(void*,char), void * userData, int fill_zero, int align_right, int precision) {
	size_t written = 0;
	unsigned long long n_width = 1;
	unsigned long long i = 9;
	if (precision == -1) precision = 1;

	if (value == 0) {
		n_width = 0;
	} else {
		unsigned long long val = value;
		while (val >= 10UL) {
			val /= 10UL;
			n_width++;
		}
	}

	if (n_width < (unsigned long long)precision) n_width = precision;

	int printed = 0;
	if (align_right) {
		while (n_width + printed < width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}

		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
	} else {
		i = n_width;
		char tmp[100];
		while (i > 0) {
			unsigned long long n = value / 10;
			long long r = value % 10;
			tmp[i - 1] = r + '0';
			i--;
			value = n;
			printed++;
		}
		while (i < n_width) {
			OUT(tmp[i]);
			i++;
		}
		while (printed < (long long)width) {
			OUT(fill_zero ? '0' : ' ');
			printed += 1;
		}
	}

	return written;
}

/*
 * Hexadecimal to string
 */
static size_t print_hex(unsigned long long value, unsigned int width, int (*callback)(void*,char), void* userData, int fill_zero, int alt, int caps, int align) {
	size_t written = 0;
	long long i = width;

	unsigned long long n_width = 1;
	unsigned long long j = 0x0F;
	while (value > j && j < UINT64_MAX) {
		n_width += 1;
		j *= 0x10;
		j += 0x0F;
	}

	if (!fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	if (alt) {
		OUT('0');
		OUT(caps ? 'X' : 'x');
	}

	if (fill_zero && align == 1) {
		while (i > (long long)n_width + 2*!!alt) {
			OUT('0');
			i--;
		}
	}

	i = (long long)n_width;
	while (i-- > 0) {
		char c = (caps ? "0123456789ABCDEF" : "0123456789abcdef")[(value>>(i*4))&0xF];
		OUT(c);
	}

	if (align == 0) {
		i = width;
		while (i > (long long)n_width + 2*!!alt) {
			OUT(' ');
			i--;
		}
	}

	return written;
}

/*
 * vasprintf()
 */
size_t xvasprintf(int (*callback)(void *, char), void * userData, const char * fmt, va_list args) {
	if (!fmt || !callback || !args) return 0;
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

				written += print_dec(dec, width, callback, userData, (padding == '0'), !justification, precision);
				break;
			
			case 'p': ;
				// %p: Pointer 
				unsigned long long ptr;
				if (sizeof(void*) == sizeof(long long)) {
					ptr = (unsigned long long)(va_arg(args, unsigned long long));
				} else {
					ptr = (unsigned int)(va_arg(args, unsigned int));
				}
				written += print_hex(ptr, width, callback, userData, (padding == '0'), 1, isupper(*f), !justification);
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

				written += print_hex(hex, width, callback, userData, (padding == '0'), add_special_chars, isupper(*f), !justification);
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

				written += print_dec(uns, width, callback, userData, (padding == '0'), !justification, precision);
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

			default: ;
				OUT(*f);
				break;
		}

		// Next character
		f++;
	}
	
	return written;
}

struct CBData {
	char * str;
	size_t size;
	size_t written;
};

static int cb_sprintf(void * user, char c) {
	struct CBData * data = user;
	if (data->size > data->written + 1) {
		data->str[data->written] = c;
		data->written++;
		if (data->written < data->size) {
			data->str[data->written] = '\0';
		}
	}
	return 0;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	struct CBData data = {str,size,0};
	int out = xvasprintf(cb_sprintf, &data, format, ap);
	cb_sprintf(&data, '\0');
	return out;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
	struct CBData data = {str, 0, 0};
	int out = xvasprintf(cb_sprintf, &data, fmt, ap);
	cb_sprintf(&data, '\0');
	return out;
}


int snprintf(char * str, size_t size, const char * format, ...) {
	struct CBData data = {str,size,0};
	va_list args;
	va_start(args, format);
	int out = xvasprintf(cb_sprintf, &data, format, args);
	va_end(args);
	cb_sprintf(&data, '\0');
	return out;
}

static int cb_sxprintf(void * user, char c) {
	struct CBData * data = user;
	data->str[data->written] = c;
	data->written++;
	return 0;
}


int sprintf(char * str, const char * format, ...) {
	struct CBData data = {str,0,0};
	va_list args;
	va_start(args, format);
	int out = xvasprintf(cb_sxprintf, &data, format, args);
	va_end(args);
	cb_sxprintf(&data, '\0');
	return out;
}

#ifdef __LIBK
static int cb_printf(void * user, char c) {
	// Terminal printing!
	// TODO: Replace with changeable thing?
	extern int terminal_print(void *user, char c);
	return terminal_print(user, c);
	return 0;
}
#endif

int printf(const char * fmt, ...) {
 	va_list args;
	va_start(args, fmt);

#ifdef __LIBK
	int out = xvasprintf(cb_printf, (void*)NULL, fmt, args);
#else
	int out = vprintf(fmt, args);
#endif	

	va_end(args);
	return out;
}
