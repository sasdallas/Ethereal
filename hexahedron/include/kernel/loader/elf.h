/**
 * @file hexahedron/include/kernel/loader/elf.h
 * @brief ELF header (NOTE: This does not contain any loading functions, but rather ELF information)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_LOADER_ELF_H
#define KERNEL_LOADER_ELF_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* ELF file types */
#define ET_NONE     0       // No file type
#define ET_REL      1       // Relocatable
#define ET_EXEC     2       // Executable file
#define ET_DYN      3       // Shared object file
#define ET_CORE     4       // Core file
#define ET_LOOS     0xFE00  // OS specific
#define ET_HIOS     0xFEFF  // OS specific
#define ET_LOPROC   0xFF00  // CPU specific  
#define ET_HIPROC   0xFFFF  // CPU specific

/* Taken from linux/include/uapi/linux/elf-em.h */
#define EM_NONE		0
#define EM_M32		1
#define EM_SPARC	2
#define EM_386		3   /* Intel 386 */
#define EM_68K		4
#define EM_88K		5
#define EM_486		6	/* Perhaps disused */
#define EM_860		7
#define EM_MIPS		8	/* MIPS R3000 (officially, big-endian only) */
#define EM_PARISC	15	/* HPPA */
#define EM_SPARC32PLUS	18	/* Sun's "v8plus" */
#define EM_PPC		20	/* PowerPC */
#define EM_PPC64	21	 /* PowerPC64 */
#define EM_SPU		23	/* Cell BE SPU */
#define EM_ARM		40	/* ARM 32 bit */
#define EM_SH		42	/* SuperH */
#define EM_SPARCV9	43	/* SPARC v9 64-bit */
#define EM_H8_300	46	/* Renesas H8/300 */
#define EM_IA_64	50	/* HP/Intel IA-64 */
#define EM_X86_64	62	/* AMD x86-64 */
#define EM_S390		22	/* IBM S/390 */
#define EM_CRIS		76	/* Axis Communications 32-bit embedded processor */
#define EM_M32R		88	/* Renesas M32R */
#define EM_MN10300	89	/* Panasonic/MEI MN10300, AM33 */
#define EM_OPENRISC     92     /* OpenRISC 32-bit embedded processor */
#define EM_ARCOMPACT	93	/* ARCompact processor */
#define EM_XTENSA	94	/* Tensilica Xtensa Architecture */
#define EM_BLACKFIN     106     /* ADI Blackfin Processor */
#define EM_UNICORE	110	/* UniCore-32 */
#define EM_ALTERA_NIOS2	113	/* Altera Nios II soft-core processor */
#define EM_TI_C6000	140	/* TI C6X DSPs */
#define EM_HEXAGON	164	/* QUALCOMM Hexagon */
#define EM_NDS32	167	/* Andes Technology compact code size embedded RISC processor family */
#define EM_AARCH64	183	/* ARM 64 bit */
#define EM_TILEPRO	188	/* Tilera TILEPro */
#define EM_MICROBLAZE	189	/* Xilinx MicroBlaze */
#define EM_TILEGX	191	/* Tilera TILE-Gx */
#define EM_ARCV2	195	/* ARCv2 Cores */
#define EM_RISCV	243	/* RISC-V */
#define EM_BPF		247	/* Linux BPF - in-kernel virtual machine */
#define EM_CSKY		252	/* C-SKY */
#define EM_LOONGARCH	258	/* LoongArch */
#define EM_FRV		0x5441	/* Fujitsu FR-V */

/* ELF file versions */
#define EV_NONE     0       // Invalid version
#define EV_CURRENT  1       // Current version

/* e_ident indexes */
#define EI_NIDENT 16
#define EI_MAG0         0       // File ID
#define EI_MAG1         1       // File ID
#define EI_MAG2         2       // File ID
#define EI_MAG3         3       // File ID
#define EI_CLASS        4       // File class
#define EI_DATA         5       // Data encoding
#define EI_VERSION      6       // File version
#define EI_OSABI        7       // Operating system/ABI identification
#define EI_ABIVERSION   8       // ABI version
#define EI_PAD          9       // Start of padding bytes

/* EI_MAG values */
#define ELFMAG0         0x7F    // e_ident[EI_MAG0]
#define ELFMAG1         'E'     // e_ident[EI_MAG1]
#define ELFMAG2         'L'     // e_ident[EI_MAG2]
#define ELFMAG3         'F'     // e_ident[EI_MAG3]

/* EI_CLASS values */
#define ELFCLASSNONE    0       // Invalid class
#define ELFCLASS32      1       // 32-bit objects
#define ELFCLASS64      2       // 64-bit objects

/* EI_DATA values */
#define ELFDATANONE     0       // Invalid data encoding
#define ELFDATA2LSB     1       // Least-significant byte
#define ELFDATA2MSB     2       // Most-significant byte

/* EI_OSABI values */
#define ELFOSABI_NONE 	    0 	// No extensions or unspecified
#define ELFOSABI_HPUX 	    1 	// Hewlett-Packard HP-UX
#define ELFOSABI_NETBSD 	2 	// NetBSD
#define ELFOSABI_LINUX 	    3 	// Linux
#define ELFOSABI_SOLARIS 	6 	// Sun Solaris
#define ELFOSABI_AIX 	    7 	// AIX
#define ELFOSABI_IRIX 	    8 	// IRIX
#define ELFOSABI_FREEBSD 	9 	// FreeBSD
#define ELFOSABI_TRU64 	    10 	// Compaq TRU64 UNIX
#define ELFOSABI_MODESTO 	11 	// Novell Modesto
#define ELFOSABI_OPENBSD 	12 	// Open BSD
#define ELFOSABI_OPENVMS 	13 	// Open VMS
#define ELFOSABI_NSK 	    14 	// Hewlett-Packard Non-Stop Kernel

/* Special section indexes */
#define SHN_UNDEF 	    0
#define SHN_LORESERVE 	0xff00
#define SHN_LOPROC 	    0xff00
#define SHN_HIPROC 	    0xff1f
#define SHN_LOOS 	    0xff20
#define SHN_HIOS 	    0xff3f
#define SHN_ABS 	    0xfff1
#define SHN_COMMON 	    0xfff2
#define SHN_XINDEX 	    0xffff
#define SHN_HIRESERVE 	0xffff

/* Section attribute flagss */
#define SHF_WRITE 	            0x1
#define SHF_ALLOC 	            0x2
#define SHF_EXECINSTR 	        0x4
#define SHF_MERGE 	            0x10
#define SHF_STRINGS 	        0x20
#define SHF_INFO_LINK 	        0x40
#define SHF_LINK_ORDER 	        0x80
#define SHF_OS_NONCONFORMING 	0x100
#define SHF_GROUP 	            0x200
#define SHF_TLS 	            0x400
#define SHF_MASKOS 	            0x0ff00000
#define SHF_MASKPROC 	        0xf0000000

/* Section group flags */
#define GRP_COMDAT 	    0x1
#define GRP_MASKOS 	    0x0ff00000
#define GRP_MASKPROC 	0xf0000000

/* Section header types */
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2   
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_NOBITS      8
#define SHT_REL         9

/* Symbol binding */
#define STB_LOCAL 	0
#define STB_GLOBAL 	1
#define STB_WEAK 	2
#define STB_LOOS 	10
#define STB_HIOS 	12
#define STB_LOPROC 	13
#define STB_HIPROC 	15

/* Symbol types */
#define STT_NOTYPE 	0
#define STT_OBJECT 	1
#define STT_FUNC 	2
#define STT_SECTION 3
#define STT_FILE 	4
#define STT_COMMON 	5
#define STT_TLS 	6
#define STT_LOOS 	10
#define STT_HIOS 	12
#define STT_LOPROC 	13
#define STT_HIPROC 	15

/* Symbol visibility */
#define STV_DEFAULT 	0
#define STV_INTERNAL 	1
#define STV_HIDDEN 	    2
#define STV_PROTECTED 	3

/* Auxiliary vectors */
#define AT_NULL     0
#define AT_IGNORE   1
#define AT_EXECFD   2
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_NOTELF	10
#define AT_UID		11
#define AT_EUID		12
#define AT_GID		13
#define AT_EGID		14

/* Segment types (p_type) */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7               // Thread local storage segment
#define PT_LOOS    0x60000000      // OS-specific
#define PT_HIOS    0x6fffffff      // OS-specific
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff
#define PT_GNU_EH_FRAME	(PT_LOOS + 0x474e550)
#define PT_GNU_STACK	(PT_LOOS + 0x474e551)
#define PT_GNU_RELRO	(PT_LOOS + 0x474e552)
#define PT_GNU_PROPERTY	(PT_LOOS + 0x474e553)

/* Segment flag bits (p_flags) */
#define PF_X 	        0x1 	    // Execute
#define PF_W 	        0x2 	    // Write
#define PF_R 	        0x4 	    // Read
#define PF_MASKOS 	    0x0ff00000 	// Unspecified
#define PF_MASKPROC 	0xf0000000 	// Unspecified

/* Dynamic array tags */
#define DT_NULL				0
#define DT_NEEDED			1
#define DT_PLTRELSZ			2
#define DT_PLTGOT			3
#define DT_HASH				4
#define DT_STRTAB			5
#define DT_SYMTAB			6
#define DT_RELA				7
#define DT_RELASZ			8
#define DT_RELAENT			9
#define DT_STRSZ			10
#define DT_SYMENT			11
#define DT_INIT				12
#define DT_FINI				13
#define DT_SONAME			14
#define DT_RPATH			15
#define DT_SYMBOLIC			16
#define DT_REL				17
#define DT_RELSZ			18
#define DT_RELENT			19
#define DT_PLTREL			20
#define DT_DEBUG			21
#define DT_TEXTREL			22
#define DT_JMPREL			23
#define DT_BIND_NOW			24
#define DT_INIT_ARRAY		25
#define DT_FINI_ARRAY		26
#define DT_INIT_ARRAYSZ		27
#define DT_FINI_ARRAYSZ		28
#define DT_RUNPATH			29
#define DT_FLAGS			30
#define DT_ENCODING			32
#define DT_PREINIT_ARRAY 	32
#define DT_PREINIT_ARRAYSZ	33
#define DT_LOOS				0x6000000D
#define DT_HIOS				0x6FFFF000
#define DT_LOPROC			0x70000000
#define DT_HIPROC			0x7FFFFFFF

/* Relocation types for I386 */
#define R_386_NONE                  0       // No relocation 
#define R_386_32                    1       // S + A
#define R_386_PC32                  2       // S + A - P

/* Relocation types for x86_64 */
#define R_X86_64_NONE             	0		// N/A
#define R_X86_64_64               	1 	// S + A
#define R_X86_64_PC32             	2  	// S + A - P
#define R_X86_64_GOT32            	3  	// G + A
#define R_X86_64_PLT32            	4  	// L + A - P
#define R_X86_64_COPY             	5 	// N/A 
#define R_X86_64_GLOB_DAT         	6  	// S
#define R_X86_64_JUMP_SLOT        	7  	// S
#define R_X86_64_RELATIVE         	8  	// B + A
#define R_X86_64_GOTPCREL         	9 	// G + GOT + A - P
#define R_X86_64_32               	10 	// S + A (32)
#define R_X86_64_32S              	11 	// S + A (32)

/* Dynamic flags (DT_FLAGS) */
#define DF_ORIGIN					0x1
#define DF_SYMBOLIC					0x2
#define DF_TEXTREL					0x4
#define DF_BIND_NOW					0x8

/**** TYPES ****/

/* Types for ELF variables */

typedef uint32_t    Elf32_Addr;
typedef uint16_t    Elf32_Half;
typedef uint32_t    Elf32_Off;
typedef int32_t     Elf32_Sword;
typedef uint32_t    Elf32_Word;

typedef uint64_t    Elf64_Addr;
typedef uint16_t    Elf64_Half;
typedef int16_t     Elf64_SHalf;
typedef uint64_t    Elf64_Off;
typedef int32_t     Elf64_Sword;
typedef uint32_t    Elf64_Word;
typedef uint64_t    Elf64_Xword;
typedef int64_t     Elf64_Sxword;


/* EHDR (32-bit) */
typedef struct {
        unsigned char   e_ident[EI_NIDENT];
        Elf32_Half      e_type;
        Elf32_Half      e_machine;
        Elf32_Word      e_version;
        Elf32_Addr      e_entry;
        Elf32_Off       e_phoff;
        Elf32_Off       e_shoff;
        Elf32_Word      e_flags;
        Elf32_Half      e_ehsize;
        Elf32_Half      e_phentsize;
        Elf32_Half      e_phnum;
        Elf32_Half      e_shentsize;
        Elf32_Half      e_shnum;
        Elf32_Half      e_shstrndx;
} Elf32_Ehdr;

/* EHDR (64-bit) */
typedef struct {
        unsigned char   e_ident[EI_NIDENT];
        Elf64_Half      e_type;
        Elf64_Half      e_machine;
        Elf64_Word      e_version;
        Elf64_Addr      e_entry;
        Elf64_Off       e_phoff;
        Elf64_Off       e_shoff;
        Elf64_Word      e_flags;
        Elf64_Half      e_ehsize;
        Elf64_Half      e_phentsize;
        Elf64_Half      e_phnum;
        Elf64_Half      e_shentsize;
        Elf64_Half      e_shnum;
        Elf64_Half      e_shstrndx;
} Elf64_Ehdr;

/* SHDR (32-bit) */
typedef struct {
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
} Elf32_Shdr;

/* SHDR (64-bit) */
typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

/* Symbol table entry (32-bit) */
typedef struct {
	Elf32_Word	st_name;
	Elf32_Addr	st_value;
	Elf32_Word	st_size;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf32_Half	st_shndx;
} Elf32_Sym;

/* Symbol table entry (64-bit) */
typedef struct {
	Elf64_Word	st_name;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf64_Half	st_shndx;
	Elf64_Addr	st_value;
	Elf64_Xword	st_size;
} Elf64_Sym;

/* Relocation entry (32-bit) */
typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
} Elf32_Rel;

/* Relocation entry (with added - 32-bit) */
typedef struct {
	Elf32_Addr	r_offset;
	Elf32_Word	r_info;
	Elf32_Sword	r_addend;
} Elf32_Rela;

typedef struct {
	Elf32_Sword d_tag;
	union {
			Elf32_Word      d_val;
			Elf32_Addr      d_ptr;
			Elf32_Off       d_off;
	} d_un;
} Elf32_Dyn;

/* Relocation entry (64-bit) */
typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
} Elf64_Rel;

/* Relocation entry (with added - 64-bit) */
typedef struct {
	Elf64_Addr	r_offset;
	Elf64_Xword	r_info;
	Elf64_Sxword	r_addend;
} Elf64_Rela;

/* Program header (32-bit) */
typedef struct {
	Elf32_Word	p_type;
	Elf32_Off	p_offset;
	Elf32_Addr	p_vaddr;
	Elf32_Addr	p_paddr;
	Elf32_Word	p_filesz;
	Elf32_Word	p_memsz;
	Elf32_Word	p_flags;
	Elf32_Word	p_align;
} Elf32_Phdr;


/* Program header (64-bit) */
typedef struct {
	Elf64_Word	p_type;
	Elf64_Word	p_flags;
	Elf64_Off	p_offset;
	Elf64_Addr	p_vaddr;
	Elf64_Addr	p_paddr;
	Elf64_Xword	p_filesz;
	Elf64_Xword	p_memsz;
	Elf64_Xword	p_align;
} Elf64_Phdr;

typedef struct {
	Elf64_Xword d_tag;
	union {
			Elf64_Xword     d_val;
			Elf64_Addr      d_ptr;
	} d_un;
} Elf64_Dyn;

/**** MACROS ****/

/* Manipulation macros for st_info */
#define ELF32_ST_BIND(i)   ((i)>>4)
#define ELF32_ST_TYPE(i)   ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

#define ELF64_ST_BIND(i)   ((i)>>4)
#define ELF64_ST_TYPE(i)   ((i)&0xf)
#define ELF64_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

/* Manipulation macros for st_other */
#define ELF32_ST_VISIBILITY(o) ((o)&0x3)
#define ELF64_ST_VISIBILITY(o) ((o)&0x3)

/* Manipulation macros for r_info */
#define ELF32_R_SYM(i)	((i)>>8)
#define ELF32_R_TYPE(i)   ((unsigned char)(i))
#define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))

#define ELF64_R_SYM(i)    ((i)>>32)
#define ELF64_R_TYPE(i)   ((i)&0xffffffffL)
#define ELF64_R_INFO(s,t) (((s)<<32)+((t)&0xffffffffL))

/* Macros for handling relocation */

#define RELOCATE_386_32(S, A) ((S) + (A))
#define RELOCATE_386_PC32(S, A, P) ((S) + (A) - (P))

#define RELOCATE_X86_64_3264(S, A) ((S) + (A))
#define RELOCATE_X86_64_PC32(S, A, P) ((S) + (A) - (P))

#endif