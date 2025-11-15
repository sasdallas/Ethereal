/**
 * @file hexahedron/drivers/x86/minacpi.c
 * @brief Mini ACPI driver to replace ACPICA
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/x86/minacpi.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/misc/args.h>
#include <kernel/panic.h>
#include <kernel/debug.h>
#include <string.h>

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/hal.h>
#include <kernel/arch/i386/smp.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/hal.h>
#include <kernel/arch/x86_64/smp.h>
#endif

/* GCC */
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

/* Root system descriptor pointer */
uintptr_t rsdp_ptr = 0x0;

/* Root system descriptor table */
acpi_rsdt_t *rsdt = NULL;

/* eXtended root system descriptor table */
acpi_xsdt_t *xsdt = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "X86:MINACPI", __VA_ARGS__)

/**
 * @brief Validate RSDP signature
 * @returns 0 if invalid, 1 if valid.
 */
static int minacpi_validateRSDP(acpi_rsdp_t *rsdp) {
    uint8_t *ptr = (uint8_t*)rsdp;
    uint8_t sum = 0; // This is meant to overflow back to 0 (signature is that the lowest byte should be 0)

    for (uint32_t i = 0; i < sizeof(acpi_rsdp_t); i++) {
        sum += ptr[i];
    }

    if (sum) return 0; // Bad signature
    return 1;
}

/**
 * @brief Parse the RSDP/XSDP
 * @returns 0 on success, anything else is failure
 */
int minacpi_parseRSDP() {
    // First we need to determine the revision of the RSDP.
    char oemid[7];
    memcpy(oemid, ((acpi_rsdp_t*)rsdp_ptr)->oemid, 6);
    oemid[6] = 0;
    LOG(DEBUG, "RSDP %p: OEMID %s VERSION %d RSDT %08x\n", rsdp_ptr, oemid, ((acpi_rsdp_t*)rsdp_ptr)->revision, ((acpi_rsdp_t*)rsdp_ptr)->rsdt_address);

    if (((acpi_rsdp_t*)rsdp_ptr)->revision) {
        // ACPI 2.0+ - assume XSDP

#ifdef __ARCH_I386__
        // This is I386... that's not gonna work...
        LOG(ERR, "ACPI ERROR: Detected an ACPI 2.0+ structure, but this is a 32-bit OS\n");
        return -1;
#else
        acpi_xsdp_t *xsdp = (acpi_xsdp_t*)rsdp_ptr;

        // Double check the signature.
        if (!minacpi_validateRSDP(&(xsdp->rsdp))) {
            LOG(WARN, "Invalid signature on RSDP\n");
            return -1;
        }

        // TODO: XSDP validation

        // Set the table
        xsdt = (acpi_xsdt_t*)arch_mmu_remap_physical(xsdp->xsdt_address, PAGE_SIZE, REMAP_PERMANENT);
#endif
    } else {
        // ACPI 1.0 - assume RSDP
        acpi_rsdp_t *rsdp = (acpi_rsdp_t*)rsdp_ptr;

        // Check the signature
        if (!minacpi_validateRSDP(rsdp)) {
            LOG(WARN, "Invalid signature on RSDP\n");
            return -1;
        }

        // Set the table
        rsdt = (acpi_rsdt_t*)arch_mmu_remap_physical(rsdp->rsdt_address, PAGE_SIZE, REMAP_PERMANENT);
    }

    return 0;
}

/**
 * @brief Find and parse the MADT for SMP information
 * @returns NULL on failure
 */
smp_info_t *minacpi_parseMADT() {
    acpi_madt_t *madt = NULL;

    if (rsdt) {
        // Use RSDT, ACPI version 1.0
        // Calculate the amount of entries first
        int entries = (rsdt->header.length - sizeof(rsdt->header)) / 8;

        for (int i = 0; i < entries; i++) {
            acpi_table_header_t *header = (acpi_table_header_t*)arch_mmu_remap_physical(rsdt->tables[i], PAGE_SIZE, REMAP_TEMPORARY);
            if (!strncmp(header->signature, "APIC", 4)) {
                // Found the MADT  
                LOG(DEBUG, "MADT found successfully at %p\n", rsdt->tables[i]);
                madt = (acpi_madt_t*)header;
                break;
            }
        
            // Not the MADT, we don't care.
            arch_mmu_unmap_physical((uintptr_t)header, PAGE_SIZE);
        }
    } else if (xsdt) {
        // Use XSDT, ACPI version 2.0+
        // Calculate the amount of entries first
        int entries = (xsdt->header.length - sizeof(xsdt->header)) / 8;
        LOG(INFO, "%d tables detected (table start: %p)\n", entries, xsdt->tables);

        // !!!: Probably a bug in the XSDT structure
        uint64_t *e = (uint64_t*)((uintptr_t)xsdt + 36);

        for (int i = 0; i < entries; i++) {
            acpi_table_header_t *header = (acpi_table_header_t*)arch_mmu_remap_physical(e[i], PAGE_SIZE, REMAP_TEMPORARY);
            if (!strncmp(header->signature, "APIC", 4)) {
                // Found the MADT
                LOG(DEBUG, "MADT found successfully at %p\n", xsdt->tables[i]);
                madt = (acpi_madt_t*)header;
                break;
            }

            arch_mmu_unmap_physical((uintptr_t)header, PAGE_SIZE);
        }
    } else {
        // what
        return NULL;
    }

    // MADT will only be present if SMP is supported
    if (!madt) {
        LOG(WARN, "Could not find MADT table - system does not support multiprocessing.\n");
        return NULL;
    }

    // Make sure our mapping is ok
    if (madt->header.length > PAGE_SIZE) {
        // how many CPUs do we have????
        kernel_panic_extended(ACPI_SYSTEM_ERROR, "minacpi", "*** madt->header.length > PAGE_SIZE, this is a kernel bug.\n");
    }


    // Now we need to start parsing. Allocate smp_info first
    smp_info_t *info = kmalloc(sizeof(smp_info_t));
    memset(info, 0, sizeof(smp_info_t));

    // Construct IRQ override list
    for (int i = 0; i < MAX_INT_OVERRIDES; i++) info->irq_overrides[i] = i;

    // Setup basic variables
    info->lapic_address = (void*)madt->local_apic_address;
    
    // Start iterating
    uint8_t *start_pointer = (uint8_t*)madt + sizeof(acpi_madt_t);
    uint8_t *end_pointer = (uint8_t*)madt + madt->header.length;
    acpi_madt_entry_t *entry;

    int cpu_count = 0; // Count of all CPUs

    while (start_pointer < end_pointer) {
        entry = (acpi_madt_entry_t*)start_pointer;
        
        switch (entry->type) {
            case MADT_LOCAL_APIC: ;
                // We found a CPU!
                acpi_madt_lapic_t *lapic = (acpi_madt_lapic_t*)entry;
                LOG(DEBUG, "LOCAL APIC - ID 0x%x FLAGS 0x%x PROCESSOR ID 0x%x\n", lapic->apic_id, lapic->flags, lapic->processor_id);
                
                // Sanity check for invalid ID (Acer Nitro 5 AN515)
                if (lapic->apic_id == 0xFF) {
                    start_pointer += entry->length;
                    continue;
                }

                if (cpu_count >= MAX_CPUS) {
                    cpu_count++; // Even if we hit the limit, we can still tell the user at the end.
                    break;
                }

                // Update SMP information
                info->processor_ids[cpu_count] = lapic->processor_id;
                info->lapic_ids[cpu_count] = lapic->apic_id;
                info->processor_count++;
                cpu_count++;
                break;
            case MADT_IO_APIC: ;
                acpi_madt_io_apic_entry_t *ioapic = (acpi_madt_io_apic_entry_t*)entry;
                LOG(DEBUG, "I/O APIC - ADDR %p GLOBAL IRQ 0x%x ID 0x%x\n", ioapic->ioapic_address, ioapic->global_irq_base, ioapic->ioapic_id);

                if (info->ioapic_count + 1 >= MAX_CPUS) break; // Hit the limit already

                // Update SMP information
                info->ioapic_addrs[info->ioapic_count] = ioapic->ioapic_address;
                info->ioapic_ids[info->ioapic_count] = ioapic->ioapic_id;
                info->ioapic_irqbases[info->ioapic_count] = ioapic->global_irq_base;
                info->ioapic_count++;
                break;
            
            case MADT_IO_APIC_INT_OVERRIDE: ;
                acpi_madt_io_apic_override_t *override = (acpi_madt_io_apic_override_t*)entry;
                LOG(DEBUG, "INTERRUPT OVERRIDE - SRCIRQ 0x%x BUS 0%x GLOBAL IRQ 0x%x INTI FLAGS 0x%x\n", override->irq_source, override->bus_source, override->gsi, override->flags);

                if (override->irq_source != override->gsi) {
                    // Need to map this one
                    info->irq_overrides[override->irq_source] = override->gsi;
                }

                break;

            case MADT_LOCAL_APIC_NMI: ;
                acpi_madt_lapic_nmi_t *nmi = (acpi_madt_lapic_nmi_t*)entry;
                LOG(DEBUG, "LOCAL APIC NMI - INTI FLAGS 0x%x LINT 0x%x PROCESSOR ID 0x%x\n", nmi->flags, nmi->lint, nmi->processor_id);
                
                break; // TODO: unimplemented

            default:
                LOG(DEBUG, "UNKNOWN/UNIMPLEMENTED TYPE - 0x%x\n", entry->type); // Could be x2apic or i/o apic stuff
                break;
        }

        start_pointer += entry->length;
    }

    LOG(DEBUG, "Finished processing MADT.\n");

    // Now that we're finished, unmap and NULL rsdt/xsdt
    if (rsdt) {
        arch_mmu_unmap_physical((uintptr_t)rsdt, PAGE_SIZE);
        rsdt = NULL;
    } else {
        arch_mmu_unmap_physical((uintptr_t)xsdt, PAGE_SIZE);
        xsdt = NULL;
    }
    
    // Unmap MADT
    arch_mmu_unmap_physical((uintptr_t)madt, PAGE_SIZE);

    return info;
}


/**
 * @brief Initialize the mini ACPI system, finding the RSDP and parsing it
 * @returns 0 on success, anything else is failure.
 */
int minacpi_initialize() {
    // First we need to find the RSDP in memory.
    // It's either located within the first KB of EBDA, but that isn't standardized
    // Instead we'll just use the main BIOS area at 0xE0000 - 0xFFFFF.
    rsdp_ptr = hal_getRSDP(); // Multiboot2 might provide this

    if (rsdp_ptr) {
        rsdp_ptr = arch_mmu_remap_physical(rsdp_ptr, PAGE_SIZE, REMAP_PERMANENT); 
        goto _found_rsdp;
    }
    
    // Now we need to search the main BIOS area. We can map this into memory
    uintptr_t main_bios_area = arch_mmu_remap_physical(0xE0000, 0x20000, REMAP_TEMPORARY);

    // Start searching
    uintptr_t i = main_bios_area;
    while (i < main_bios_area + 0x20000) {

        // Using strncmp with a length of 8 as the string isn't null terminated
        if (!strncmp((char*)i, "RSD PTR ", 8)) {
            // Found it!
            rsdp_ptr = i;
            break;
        }

        i += 16; // Signature is always aligned on a 16-byte boundary
    }


    // Unmap memory
    arch_mmu_unmap_physical(main_bios_area, 0x20000);
    
    // Did we find the signature?
    if (!rsdp_ptr) {
        LOG(WARN, "RSDP not found in memory\n");
        return -1;
    }

_found_rsdp: ; // Found the RSDP

    LOG(DEBUG, "RSDP found at %p - parsing\n", rsdp_ptr);

    // Parse the RSDP 
    return minacpi_parseRSDP();
}
