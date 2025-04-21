/**
 * @file hexahedron/include/kernel/drivers/x86/minacpi.h
 * @brief Mini ACPI driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_X86_MINACPI_H
#define DRIVERS_X86_MINACPI_H

/**** INCLUDES ****/
#include <stdint.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/smp.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/smp.h>
#endif

/**** TYPES ****/

typedef struct acpi_rsdp {
    char signature[8];      // "RSD PTR ", not null terminated
    uint8_t checksum;       // Checksum validation
    char oemid[6];          // OEM string
    uint8_t revision;       // Revision of the RSDP. If not 0, cast this to an XSDP.
    uint32_t rsdt_address;  // RSDT address
} acpi_rsdp_t;

typedef struct acpi_xsdp {
    acpi_rsdp_t rsdp;       // RSDP fields

    uint32_t length;        // Length
    uint64_t xsdt_address;  // XSDT address
    uint8_t checksum_ext;   // Checksum extended
    uint8_t reserved[3];    // Reserved
} acpi_xsdp_t;

typedef struct acpi_table_header_t {
    char signature[4];      // Depends on each table
    uint32_t length;        // Length of the table including the header
    uint8_t revision;       // Revision
    uint8_t checksum;       // Checksum of the table
    char oemid[6];          // OEM ID
    char oemtableid[8];     // OEM table ID
    uint32_t oem_revision;  // OEM revision
    uint32_t creator_id;    // Creator ID
    uint32_t creator_rev;   // Creator revision
} acpi_table_header_t;

typedef struct acpi_rsdt {
    acpi_table_header_t header;     // RSDT header
    uint32_t tables[];              // List of tables
} acpi_rsdt_t;

typedef struct acpi_xsdt {
    acpi_table_header_t header;     // XSDT header
    uint64_t tables[];              // List of tables
} acpi_xsdt_t;

typedef struct acpi_madt {
    acpi_table_header_t header;     // MADT header
    uint32_t local_apic_address;    // Local APIC address
    uint32_t flags;                 // Flags (1 = Dual 8259 Legacy PICs installed)
} acpi_madt_t;  

// Header of each entry
typedef struct acpi_madt_entry {
    uint8_t type;
    uint8_t length;
} acpi_madt_entry_t;

typedef struct acpi_madt_lapic {
    acpi_madt_entry_t entry;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} acpi_madt_lapic_t;

typedef struct acpi_madt_io_apic_entry {
    acpi_madt_entry_t entry;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t global_irq_base;
} acpi_madt_io_apic_entry_t;

typedef struct acpi_madt_io_apic_override {
    acpi_madt_entry_t entry;
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} acpi_madt_io_apic_override_t;

typedef struct acpi_madt_io_apic_nmi {
    acpi_madt_entry_t entry;
    uint8_t nmi_source;
    uint8_t reserved;
    uint16_t flags;
    uint32_t gsi;
} acpi_madt_io_apic_nmi_t;

typedef struct acpi_madt_lapic_nmi {
    acpi_madt_entry_t entry;
    uint8_t processor_id;   // 0xFF for all processors
    uint16_t flags;
    uint8_t lint;
} acpi_madt_lapic_nmi_t;

typedef struct acpi_lapic_address {
    acpi_madt_entry_t entry;
    uintptr_t phys;         // 64-bit only
} acpi_lapic_address_t;

typedef struct acpi_madt_x2apic {
    acpi_madt_entry_t entry;
    uint16_t reserved;
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t acpi_id;
} acpi_madt_x2apic_t;

/**** DEFINITIONS ****/

#define MADT_LOCAL_APIC             0   // Single local processor
#define MADT_IO_APIC                1   // I/O APIC
#define MADT_IO_APIC_INT_OVERRIDE   2   // I/O APIC interrupt source override
#define MADT_IO_APIC_NMI            3   // I/O APIC non-maskable interrupt source
#define MADT_LOCAL_APIC_NMI         4   // Local APIC non-maskable interrupts
#define MADT_LOCAL_APIC_ADDRESS     5   // Local APIC address override
#define MADT_LOCAL_X2_APIC          9   // Local x2APIC


/**** FUNCTIONS ****/

/**
 * @brief Initialize the mini ACPI system, finding the RSDP and parsing it
 * @returns 0 on success, anything else is failure.
 */
int minacpi_initialize();

/**
 * @brief Find and parse the MADT for SMP information
 * @returns NULL on failure
 */
smp_info_t *minacpi_parseMADT();

#endif