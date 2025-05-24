OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    /* Kernel is loaded at 1M */
    . = 1M;

    __kernel_start_phys = .;

    /* Multiboot and initial trampoline */
    __multiboot_start = .;
    .multiboot.data : {
        *(.multiboot.data)
    }

    .multiboot.text : {
        *(.multiboot.text)
    }
    __multiboot_end = .;

    /* Kernel address (virtual) */
    . += 0xFFFFF00000000000;

    __kernel_start = .;

    /* Code section */
	.text ALIGN (4K) : AT (ADDR (.text) - 0xFFFFF00000000000)
    {
        __bootstrap_start = .;
        *(.bootstrap)
        __bootstrap_end = .;

        __text_start = .;
        *(.text)
        __text_end = .;

        __ap_start = .;
        *(.ap_bootstrap)
        __ap_end = .;

        __userspace_start = .;
        *(.userspace)
        __userspace_end = .;
    }


	/* Initialized data */
	.data ALIGN (4K) : AT (ADDR (.data) - 0xFFFFF00000000000)
	{
        __data_start = .;
		*(.data)
        __data_end = .;
	}

    /* Read-only data */	
    .rodata ALIGN (4K) : AT (ADDR (.rodata) - 0xFFFFF00000000000)
	{
        __rodata_start = .;
		*(.rodata)
        __rodata_end = .;
	}

	/* BSS */
	.bss ALIGN (4K) : AT (ADDR (.bss) - 0xFFFFF00000000000)
	{
        __bss_start = .;
		*(COMMON)
		*(.bss)
        __bss_end = .;
	}

    .lbss ALIGN (4K) : AT (ADDR (.lbss) - 0xFFFFF00000000000)
    {
        __lbss_start = .;
        *(.lbss)
        __lbss_end = .;
    }

    __kernel_end_phys = . - 0xFFFFF00000000000;
    __kernel_end = .;

    /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}