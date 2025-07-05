/**
 * @file userspace/miniutils/whatarewe.c
 * @brief Minor neofetch-ish clone
 * 
 * Note that this is pretty hacked together.
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdlib.h>

char *ascii_small[] = {"\033[1m\n",
"                       :::::       ",
"                    :        .     ",
"                 :           ..    ",
"            ::::::::---      ...   ",
"         :.......:::--==+=   ...   ",
"       ::.   ...::---==+===  ..    ",
"      ::.    ..:----=++=====...    ",
"     :-::....:---===+++====-...    ",
"    ::::-::---==+++++++++==...     ",
"    :::----===+++++++***--...+     ",
"    -::--====+++++*****#=...+*     ",
"     ::---====++**##**#:..:++*     ",
"    =-:--===+++****+*#...-=+*      ",
"   == =---==+++***++-:::+=+*       ",
"   ==   +=+++======:--=++**        ",
"   =+     +**+++=---=++*           ",
"   =+         -====                ",
"    +**   +++++==                  ",
"     +==++++=                      "};

char *info_lines[] = {"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n",
"\n"
};
char *prog_lines[] = {NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
	};
	

/* CPUID requests */
enum cpuid_requests {
    CPUID_GETVENDORSTRING,
    CPUID_GETFEATURES,
    CPUID_GETTLB,
    CPUID_GETSERIAL,

    CPUID_INTELEXTENDED = 0x80000000,
    CPUID_INTELFEATURES,
    CPUID_INTELBRANDSTRING,
    CPUID_INTELBRANDSTRINGMORE,
    CPUID_INTELBRANDSTRINGEND,

    CPUID_INTELADDRSIZE = 0x80000008,
};

char *cpu_getBrandString() {
    char brand[48];
    snprintf(brand, 10, "Unknown");

	// Get using CPUID
#if defined(__ARCH_X86_64__) || defined(__ARCH_I386__)
	#define __cpuid(code, a, b, c, d) asm volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(code))
    uint32_t eax, unused;
    __cpuid(CPUID_INTELEXTENDED, eax, unused, unused, unused);
    if (eax >= CPUID_INTELBRANDSTRINGEND) {
        // Supported!
        uint32_t brand_data[12];
		__cpuid(0x80000002, brand_data[0], brand_data[1], brand_data[2], brand_data[3]);
		__cpuid(0x80000003, brand_data[4], brand_data[5], brand_data[6], brand_data[7]);
		__cpuid(0x80000004, brand_data[8], brand_data[9], brand_data[10], brand_data[11]);
        memcpy(brand, brand_data, 48);
    }
#endif

    return strdup(brand);
}


#define INFO_PUSH_LINE(...) { snprintf(tmp, 128, __VA_ARGS__); info_lines[i] = strdup(tmp); i++; }
#define COLOR_OFF "\033[0m"
#define COLOR_PURPLE "\033[0;35m"

void collect_info(int start) {
	int i = start;
	char tmp[128];

	struct utsname utsname;
	if (uname(&utsname) < 0) {
		perror("uname");
		exit(0);
	}

	INFO_PUSH_LINE("root" COLOR_OFF "@" COLOR_PURPLE "ethereal\n"); // lack of hostname support :(
	INFO_PUSH_LINE("-------------\n");
	INFO_PUSH_LINE("OS: " COLOR_OFF "Ethereal x86_64\n");
	INFO_PUSH_LINE("Kernel: " COLOR_OFF "%s %s\n", utsname.sysname, utsname.release);
	prog_lines[i] = "essence --version";
	INFO_PUSH_LINE("Shell: " COLOR_OFF);
	INFO_PUSH_LINE("CPU: " COLOR_OFF "%s\n", cpu_getBrandString());
	INFO_PUSH_LINE("\n");
	
	// Put ANSI color codes
	INFO_PUSH_LINE("\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m\033[0;35m\n")
}

int main(int argc, char** argv) {
	// Collect info
	collect_info(6);

	// Set color code
	printf("\033[0;35m");

	// Print them out
    for (size_t i = 0; i < sizeof(ascii_small) / sizeof(char*); i++) {
      	printf(COLOR_PURPLE "%s%s", ascii_small[i], info_lines[i]);
		if (prog_lines[i]) {
			fflush(stdout);
			system(prog_lines[i]);
		}
	}

	printf("\033[0m\n");

    return 0; 
}