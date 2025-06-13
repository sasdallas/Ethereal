/**
 * @file hexahedron/arch/aarch64/bootstubs/qemu_virt/main.c
 * @brief QEMU Virt machine bootstub
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel/arch/aarch64/mem.h>

/**** DEFINITIONS ****/
#define QEMU_VIRT_UART_ADDRESS      (volatile uint8_t*)0x09000000
#define QEMU_VIRT_LOAD_BASE         (uintptr_t)0x40000000 // DTB is also positioned here
#define QEMU_VIRT_BOOTSTUB_BASE     (uintptr_t)0x41000000
#define QEMU_VIRT_PHYSMEM_REGION    (uintptr_t)0xffffff8000000000

/* SCTLR */
#define SCTLR_M                     (1 << 0)    // MMU enable
#define SCTLR_C                     (1 << 2)    // Cache
#define SCTLR_ITD                   (1 << 7)    // IT disable
#define SCTLR_I                     (1 << 12)   // Instruction cacheability
#define SCTLR_SPAN                  (1 << 23)   // Set Privileged Access Never
#define SCTLR_NTLSMD                (1 << 28)   // No Trap Load Multiple and Store Multiple
#define SCTLR_LSMAOE                (1 << 29)   // Load Multiple and Store Multiple Atomicity and Ordering Enable

/* TCR */
#define TCR_T0SZ_48_BIT             16          // Size offset of memory region in TTBR0_EL1 (48-bit)
#define TCR_IRGN0                   (1 << 8)    // Normal memory, inner WT, RA, no WA, C
#define TCR_ORGN0                   (1 << 10)   // Normal memory, outer WB, RA, WA, C
#define TCR_SH0_INNER_SHAREABLE     (3 << 12)   // Inner shareable
#define TCR_TG0_4KB                 0           // 4KB granule size
#define TCR_T1SZ_48_BIT             (16 << 16)  // Size offset of memory region in TTBR1_EL1 (48-bit)
#define TCR_IRGN1                   (1 << 24)   // Normal memory, inner WT, RA, no WA, C
#define TCR_ORGN1                   (1 << 26)   // Normal memory, outer WB, RA, WA, C
#define TCR_SH1_INNER_SHAREABLE     (3 << 28)   // Inner shareable
#define TCR_TG1_4KB                 (2 << 30)   // 4KB granule
#define TCR_IPS_4TB                 (3UL << 32) // 4TB intermediate physical address size

/* FDT token types */
#define FDT_BEGIN_NODE              0x00000001
#define FDT_END_NODE                0x00000002
#define FDT_PROP                    0x00000003
#define FDT_NOP                     0x00000004
#define FDT_END                     0x00000009

/* Magic */
#define FDT_MAGIC                   0xd00dfeed

/* Firmware config shenanigans */
#define QEMU_FWCFG_SELECTOR_OFFSET  8
#define QEMU_FWCFG_DATA_OFFSET      0

#define FW_CFG_SIGNATURE            0x0000
#define FW_CFG_FILE_DIR             0x0019

#define FW_CFG_CONTROL_ERROR        0x01
#define FW_CFG_CONTROL_READ         0x02
#define FW_CFG_CONTROL_SKIP         0x04
#define FW_CFG_CONTROL_SELECT       0x08
#define FW_CFG_CONTROL_WRITE        0x10
#define FW_CFG_CONTROL_SELECT_SHIFT 16

#define ETHEREAL_KERNEL_NAME        "opt/org.ethereal.kernel"


/* ELF file types */
#define ET_NONE     0       // No file type
#define ET_REL      1       // Relocatable
#define ET_EXEC     2       // Executable file

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

/**** CODE ****/

/* MMU tables */
page_t mmu_pml[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_high_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_low_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };
page_t mmu_kernel_gbs[512] __attribute__((aligned(PAGE_SIZE))) = { 0 };

int use_physmap = 0;

extern uintptr_t __kernel_tmp_load_address;

/* DTB header */
struct fdt_header {
	uint32_t magic;
	uint32_t totalsize;
	uint32_t off_dt_struct;
	uint32_t off_dt_strings;
	uint32_t off_mem_rsvmap;
	uint32_t version;
	uint32_t last_comp_version;
	uint32_t boot_cpuid_phys;
	uint32_t size_dt_strings;
	uint32_t size_dt_struct;
};

/* DTB property */
struct fdt_prop {
    uint32_t len;
    uint32_t nameoff;
} __attribute__((packed));


/* qemu firmware config file */
struct qemu_fw_cfg_file {
    uint32_t size;
    uint16_t select;
    uint16_t reserved;
    char name[56];
};

/* QEMU firmware config files */
struct qemu_fw_cfg_files {
    uint32_t count;
    struct qemu_fw_cfg_file f[];
};

/* QEMU firmware config DMA request */
struct qemu_fw_cfg_dma {
    volatile uint32_t control;
    volatile uint32_t length;
    volatile uint64_t address;
};

/* ELF types */
typedef uint64_t    Elf64_Addr;
typedef uint16_t    Elf64_Half;
typedef int16_t     Elf64_SHalf;
typedef uint64_t    Elf64_Off;
typedef int32_t     Elf64_Sword;
typedef uint32_t    Elf64_Word;
typedef uint64_t    Elf64_Xword;
typedef int64_t     Elf64_Sxword;

/* EHDR (64-bit) */
typedef struct {
    unsigned char   e_ident[16];
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



/**
 * @brief Swap bits (64-bit number)
 * @param value The value to swap bits of
 */
uint64_t bswap64(uint64_t value) {
    uint8_t p1 = (uint8_t)(value);
    uint8_t p2 = (uint8_t)(value >> 8);
    uint8_t p3 = (uint8_t)(value >> 16);
    uint8_t p4 = (uint8_t)(value >> 24);
    uint8_t p5 = (uint8_t)(value >> 32);
    uint8_t p6 = (uint8_t)(value >> 40);
    uint8_t p7 = (uint8_t)(value >> 48);
    uint8_t p8 = (uint8_t)(value >> 56);
    
    return ((uint64_t)p1 << 56) | ((uint64_t)p2 << 48) | ((uint64_t)p3 << 40) | ((uint64_t)p4 << 32) | ((uint64_t)p5 << 24) | ((uint64_t)p6 << 16) | ((uint64_t)p7 << 8) | ((uint64_t)p8);
}

/**
 * @brief Swap bits (32-bit number)
 * @param value The value to swap bits of
 */
uint32_t bswap32(uint32_t value) {
    return ((uint8_t)value << 24) | ((uint8_t)(value >> 8) << 16) | ((uint8_t)(value >> 16) << 8) | ((uint8_t)(value >> 24));
}

/**
 * @brief Swap bits (16-bit number)
 * @param value The value to swap bits of
 */
uint16_t bswap16(uint16_t value) {
	uint8_t a = value >> 8;
	uint8_t b = value;
	return (b << 8) | (a);
}

/**
 * @brief Print method to the QEMU_VIRT address
 */
int terminal_print(void *user, char c) {
    if (c == '\n') terminal_print(NULL, '\r');
    if (!use_physmap) *QEMU_VIRT_UART_ADDRESS = c;
    else *(volatile uint8_t*)(QEMU_VIRT_PHYSMEM_REGION | (uintptr_t)QEMU_VIRT_UART_ADDRESS) = c;

    return 0;
}

/**
 * @brief Debug printf method
 */
void dprintf(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    xvasprintf(terminal_print, NULL, fmt, ap);
    va_end(ap);
}

/**
 * @brief Initialize the MMU, build default kernel tables and set them in TTBR
 * @returns 0 on success
 */
int bootstub_initializeMMU() {
    // Initial PML mappings of the kernel 
    dprintf("bootstub: mmu: Map bootstub at %016llX\n", QEMU_VIRT_LOAD_BASE);
    dprintf("bootstub: mmu: Make physical memory mappings at %016llX\n", QEMU_VIRT_PHYSMEM_REGION);

    // Create low and high memory mappings
    mmu_pml[0].data = (uintptr_t)&mmu_low_gbs;
    mmu_pml[0].bits.present = 1;
    mmu_pml[0].bits.table = 1;
    mmu_pml[0].bits.af = 1;

    mmu_pml[511].data = (uintptr_t)&mmu_high_gbs;
    mmu_pml[511].bits.present = 1;
    mmu_pml[511].bits.table = 1;
    mmu_pml[511].bits.af = 1;

    // Make a mapping for us, the bootstub
    mmu_low_gbs[1].data = QEMU_VIRT_LOAD_BASE;
    mmu_low_gbs[1].bits.present = 1;
    mmu_low_gbs[1].bits.af = 1;
    mmu_low_gbs[1].bits.sh = 2;
    mmu_low_gbs[1].bits.indx = 1;

    // Physical memory mapping
    for (size_t i = 0; i < 64; i++) {
        mmu_high_gbs[i].data = (i << 30);
        mmu_high_gbs[i].bits.present = 1;
        mmu_high_gbs[i].bits.af = 1;
        mmu_high_gbs[i].bits.sh = 2;
        mmu_high_gbs[i].bits.indx = 1;
    }

    // Create mapping of the kernel
    mmu_pml[480].data = (uintptr_t)&mmu_kernel_gbs;
    mmu_pml[480].bits.present = 1;
    mmu_pml[480].bits.table = 1;
    mmu_pml[480].bits.af = 1;

    // Create kernel mappings
    for (size_t i = 0; i < 512; i++) {
        mmu_kernel_gbs[i].data = (QEMU_VIRT_LOAD_BASE + (i << 21));
        mmu_kernel_gbs[i].bits.present = 1;
        mmu_kernel_gbs[i].bits.af = 1;
        mmu_kernel_gbs[i].bits.sh = 2;
        mmu_kernel_gbs[i].bits.indx = 1;
    }



    // SCTLR
	uint64_t sctlr = SCTLR_M | SCTLR_C | SCTLR_ITD | SCTLR_I | SCTLR_SPAN | SCTLR_NTLSMD | SCTLR_LSMAOE;

    // TCR
	uint64_t tcr = TCR_TG0_4KB | TCR_TG1_4KB |
                    TCR_IRGN0 | TCR_IRGN1 | TCR_ORGN0 | TCR_ORGN1 |
                    TCR_SH0_INNER_SHAREABLE | TCR_SH1_INNER_SHAREABLE |
                    TCR_T0SZ_48_BIT | TCR_T1SZ_48_BIT |
                    TCR_IPS_4TB;

	// MAIR setup
	uint64_t mair  = (0x000000000044ff00);
	asm volatile ("msr MAIR_EL1,%0" :: "r"(mair));

	// Configure TTBR0 and TTBR1
	dprintf("bootstub: mmu: PML @ %016llX\n", &mmu_pml);
	asm volatile ("msr TTBR0_EL1,%0" : : "r"(&mmu_pml));
	asm volatile ("msr TTBR1_EL1,%0" : : "r"(&mmu_pml));
	asm volatile ("msr TCR_EL1,%0" : : "r"(tcr));
	
    // Sync
	asm volatile ("dsb ishst\n"
                    "tlbi vmalle1is\n"
                    "dsb ish\n"
                    "isb\n" ::: "memory");
    
    // Set SCTLR       
    dprintf("bootstub: mmu: Enabling MMU\n");
	asm volatile ("msr SCTLR_EL1, %0" :: "r"(sctlr));
	asm volatile ("isb" ::: "memory");

    use_physmap = 1;
    dprintf("bootstub: mmu: MMU enabled successfully\n");

    return 0;
}

/**
 * @brief Recursive method to find a DTB node by its prefix
 * @param node The starting node to use
 * @param name The name that is being looked for
 * @param node_out Internal usage
 * @returns NULL if the node could not be found or the node
 */
char *bootstub_findDTBNode(uint32_t *node, char *name, uint32_t **node_out) {
    while (bswap32(*node) == FDT_NOP) node++; // Skip past all NOPs
    if (bswap32(*node) != FDT_BEGIN_NODE) return NULL;

    node++; // Skip over the FDT_BEGIN_NODE
    dprintf("bootstub: dtb: found node %s\n", node);

    // Check the names
    if (!memcmp(node, name, strlen(name))) {
        if (node_out) *node_out = NULL;
        return (char*)node;
    }

    // Skip over the name
	while ((*node & 0xFF000000) && (*node & 0xFF0000) && (*node & 0xFF00) && (*node & 0xFF)) node++;
	node++;

    // Back to lexical parsing
    while (1) {
        // Skip over any NOPs again
        while (bswap32(*node) == FDT_NOP) node++;
        
        // If we're at the end of a node, it means that we should just go on :D
        if (bswap32(*node) == FDT_END_NODE) {
            if (!node_out) return NULL;
            *node_out = node+1; // Yes, this is hacky..
            return NULL;
        }

        // Are we at a property?
        if (bswap32(*node) == FDT_PROP) {
            struct fdt_prop *prop = (struct fdt_prop*)(node+1);
            node += 3;
            node += (bswap32(prop->len) + 3) / 4;
        } 
        
        // Are we at a node beginning?
        if (bswap32(*node) == FDT_BEGIN_NODE) {
            uint32_t *new_node = NULL;
            char *r = bootstub_findDTBNode(node, name, &new_node);
            if (!new_node) return r;
            node = new_node;
        }
    }   
}

/**
 * @brief Find a property by name
 * @param node The node to dump properties of
 * @param dtb The DTB
 * @param name The name of the property
 * @param node_out Internal
 */
uint32_t *bootstub_findProperty(uint32_t *node, struct fdt_header *dtb, char *name, uint32_t **node_out) {
    // Skip over the name
	while ((*node & 0xFF000000) && (*node & 0xFF0000) && (*node & 0xFF00) && (*node & 0xFF)) node++;
	node++;

    while (1) {
        // Skip over any NOPs again
        while (bswap32(*node) == FDT_NOP) node++;
        
        // If we're at the end of a node, it means that we should just go on :D
        if (bswap32(*node) == FDT_END_NODE) {
            return NULL;
        }

        // Are we at a property?
        if (bswap32(*node) == FDT_PROP) {
            struct fdt_prop *prop = (struct fdt_prop*)(node+1);

            char *strings = (char*)((uintptr_t)dtb + bswap32(dtb->off_dt_strings));
            dprintf("bootstub: dtb: \tProperty %s\n", strings + bswap32(prop->nameoff));

            if (!strcmp(strings + bswap32(prop->nameoff), name)) {
                if (node_out) *node_out = NULL;
                return (uint32_t*)prop;
            }

            node += 3;
            node += (bswap32(prop->len) + 3) / 4;
        } 

        if (bswap32(*node) == FDT_BEGIN_NODE) {
            uint32_t *new_node = NULL;
            uint32_t *r = bootstub_findProperty(node, dtb, name, &new_node);
            if (!new_node) return r;
            node = new_node;
        }
        
    }   
}

/**
 * @brief Process the DTB and find the fw_cfg nodes that we need
 * @param dtb The DTB to parse
 */
uintptr_t bootstub_parseDTB(uintptr_t dtb) {
    struct fdt_header* hdr = (struct fdt_header*)dtb;
    if (bswap32(hdr->magic) != FDT_MAGIC) {
        dprintf("FATAL: Invalid DTB magic %08x\n", bswap32(hdr->magic));
        while (1) asm volatile ("wfi");
    }

    dprintf("bootstub: dtb: magic=%08x version=%08x\n", bswap32(hdr->magic), bswap32(hdr->version));

    // We're looking for a fw-cfg node
    char *n = bootstub_findDTBNode((uint32_t*)((uintptr_t)hdr + bswap32(hdr->off_dt_struct)), "fw-cfg", NULL);
    if (!n) {
        dprintf("FATAL: Firmware configuration not found\n");
        while (1) asm volatile ("wfi");
    }

    dprintf("bootstub: dtb: Firmware configuration available @ %p\n", n);

    // Get the reg property
    uint32_t *reg = bootstub_findProperty((uint32_t*)n, hdr, "reg", NULL);
    if (!reg) {
        dprintf("FATAL: Reg property not found in fw-cfg\n");
        while (1) asm volatile ("wfi");
    }

    dprintf("bootstub: dtb: reg property available at %p\n", reg);

    // Get the firmware configuration address
    volatile uint8_t *qemu_fw_cfg = (volatile uint8_t*)(QEMU_VIRT_PHYSMEM_REGION | bswap32(reg[3]));

    dprintf("bootstub: fw-cfg: Firmware configuration registers at %p\n", qemu_fw_cfg);


    // Check response to signature
    volatile uint16_t *qemu_selector = (volatile uint16_t*)(qemu_fw_cfg + QEMU_FWCFG_SELECTOR_OFFSET);
    *qemu_selector = bswap16(FW_CFG_SIGNATURE);
    uint64_t sig = ((volatile uint64_t*)(qemu_fw_cfg))[0];

    dprintf("bootstub: fw-cfg: Got a signature from the firmware configuration: %c%c%c%c\n",
        (char)(sig >> 0),
        (char)(sig >> 8),
        (char)(sig >> 16),
        (char)(sig >> 24));

    *qemu_selector = bswap16(FW_CFG_FILE_DIR);

    // Get the files
    uint32_t count = bswap32(((volatile uint32_t*)qemu_fw_cfg)[0]);
    dprintf("bootstub: fw-cfg: %d entries were found\n", count);
    if (count < 1) {
        dprintf("FATAL: Require a kernel image to boot (no entries found in firmware config)\n");
        while (1) asm volatile ("wfi");
    }

    // Find the kernel entry
    struct qemu_fw_cfg_file f;
    for (unsigned int i = 0; i < count; i++) {
        // Read into the structure
        for (unsigned int j = 0; j < sizeof(struct qemu_fw_cfg_file); j++) ((uint8_t*)&f)[j] = qemu_fw_cfg[0];
        
        f.select = bswap16(f.select);
        f.size = bswap32(f.size);
        dprintf("bootstub: fw-cfg: Found file \"%s\" of size %d with select %04x\n", f.name, f.size, f.select);

        // We are looking for ETHEREAL_KERNEL_NAME
        if (!strcmp(f.name, ETHEREAL_KERNEL_NAME)) {
            // Found it! Let's DMA it to __kernel_temp_load_address
            dprintf("bootstub: kernel: Found the kernel, loading it to %016llX\n", &__kernel_tmp_load_address);
            struct qemu_fw_cfg_dma dma;

            dma.control = bswap32((f.select << 16) | FW_CFG_CONTROL_READ | FW_CFG_CONTROL_SELECT);
            dma.length = bswap32(f.size);
            dma.address = bswap64((uint64_t)(uint8_t*)&__kernel_tmp_load_address);

            ((volatile uint64_t*)qemu_fw_cfg)[2] = bswap64((uint64_t)&dma);
            return (uintptr_t)&__kernel_tmp_load_address;
        }

    }


    dprintf("FATAL: Require a kernel image to boot (no entries matching found in firmware configuration)\n");
    while (1) asm volatile ("wfi");
    __builtin_unreachable();
}

/**
 * @brief Process the kernel ELF file
 * @param kaddr The kernel address in memory
 */
void bootstub_loadELF(uintptr_t kaddr) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)kaddr;
    if (ehdr->e_ident[0] != 0x7f && ehdr->e_ident[1] != 'E' && ehdr->e_ident[2] != 'L' && ehdr->e_ident[3] != 'F') {
        dprintf("FATAL: Invalid ELF header signature\n");
        while (1) asm volatile ("wfi");
    }

    if (ehdr->e_ident[4] != 2) {
        dprintf("FATAL: Invalid ELF file class\n");
        while (1) asm volatile ("wfi");
    }

    if (ehdr->e_ident[5] != 1) {
        dprintf("FATAL: Invalid ELF file data order\n");
        while (1) asm volatile ("wfi");
    }

    if (ehdr->e_machine != 183) {
        dprintf("FATAL: Invalid ELF machine\n");
        while (1) asm volatile ("wfi");
    }

    if (ehdr->e_type != ET_EXEC) {
        dprintf("FATAL: Not an executable\n");
        while (1) asm volatile ("wfi");
    }

    // Go through each PHDR
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr*)((uintptr_t)ehdr + ehdr->e_phoff + ehdr->e_phentsize * i);
        
        switch (phdr->p_type) {
            case PT_NULL:
                break;
            
            case PT_LOAD:
            case PT_TLS:
                // Copy it into memory
                dprintf("bootstub: elf: PHDR #%d - OFFSET 0x%x VADDR %p PADDR %p FILESIZE %d MEMSIZE %d\n",  i, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz);
            
                memcpy((void*)phdr->p_vaddr, (void*)((uintptr_t)ehdr + phdr->p_offset), phdr->p_filesz);
                
                // Zero remainder
                if (phdr->p_memsz > phdr->p_filesz) {
                    memset((void*)phdr->p_vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
                }
                break;

            default:
                dprintf("FATAL: Failed to load PHDR #%d - unimplemented type 0x%x\n", i, phdr->p_type);
                while (1) asm volatile ("wfi");
        }
    }

    dprintf("bootstub: elf: load complete, changing to kernel now\n");
    dprintf("bootstub: elf: Jumping to %016llX\n", ehdr->e_entry);
    dprintf("================= BOOTSTUB EXITING =================\n");

typedef void (*kernel_entry_t)(uintptr_t dtb, uintptr_t phys);

    kernel_entry_t entry = (kernel_entry_t)ehdr->e_entry;
    entry(QEMU_VIRT_LOAD_BASE, QEMU_VIRT_BOOTSTUB_BASE);
}

/**
 * @brief Main function of bootstub
 */
void bootstub_main() {
    dprintf("================= BOOTSTUB RUNNING =================\n");
    dprintf("bootstub: Compiled on %s %s for the QEMU Virt machine\n", __DATE__, __TIME__);

    dprintf("bootstub: Initializing MMU\n");
    bootstub_initializeMMU();

    dprintf("bootstub: Looking up kernel\n");
    uintptr_t kaddr = bootstub_parseDTB(QEMU_VIRT_LOAD_BASE);
    dprintf("bootstub: Kernel loaded to %p\n", kaddr);

    bootstub_loadELF(kaddr);

    dprintf("FATAL: ELF load failed\n");
}