/*
 * Hexahedron ACPICA header 
 */

#ifndef __ACHEXAHEDRON_H__
#define __ACHEXAHEDRON_H__

#include <kernel/misc/spinlock.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#define ACPI_SINGLE_THREADED // TODO: Uncomment

// Use the system's C library
#define ACPI_USE_SYSTEM_CLIBRARY



#define ACPI_USE_DO_WHILE_0

#define ACPI_USE_NATIVE_DIVIDE

// Use local ACPICA cache
#define ACPI_CACHE_T                ACPI_MEMORY_LIST
#define ACPI_USE_LOCAL_CACHE        1

#if defined(__ARCH_X86_64__)
#define ACPI_MACHINE_WIDTH          64
#elif defined(__ARCH_I386__)
#define ACPI_MACHINE_WIDTH          32
#else
#error "Unknown architecture"
#endif


// Spinlock/uintptr_t
#define ACPI_SPINLOCK               spinlock_t*
#define ACPI_UINTPTR_T              uintptr_t

// Error messages to conform to our debug log system (in the most scuffed way possible due to lack of clock)
// [ACPICA:INT] is for INTERNAL
#define ACPI_MSG_ERROR          "[EXTERNAL                ] [CPU0] [ERR ] [ACPICA:INT] ACPI Error: "
#define ACPI_MSG_EXCEPTION      "[EXTERNAL                ] [CPU0] [ERR ] [ACPICA:INT] ACPI Exception: "
#define ACPI_MSG_WARNING        "[EXTERNAL                ] [CPU0] [WARN] [ACPICA:INT] ACPI Warning: "
#define ACPI_MSG_INFO           "[EXTERNAL                ] [CPU0] [INFO] [ACPICA:INT] ACPI: "
#define ACPI_MSG_BIOS_ERROR     "[EXTERNAL                ] [CPU0] [ERR ] [ACPICA:INT] ACPI BIOS Error: "
#define ACPI_MSG_BIOS_WARNING   "[EXTERNAL                ] [CPU0] [WARN] [ACPICA:INT] ACPI BIOS Warning: "

// Certain compilation errors occur of multiple sprintf/snprintf/vsnprintf definitions
#define sprintf _sprintf
#define snprintf _snprintf
#define vsnprintf _vsnprintf

// CPU cache suppot not yet
#define ACPI_FLUSH_CPU_CACHE() 

#include "acgcc.h"

#endif /* __ACHEXAHEDRON_H__ */