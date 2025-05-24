ENTRY(_start)

SECTIONS
{
    /* Kernel is loaded at 1M */
    . = 1M;

    __kernel_start = .;

    /* Code section */
    .text BLOCK(4K) : ALIGN (4k)
    {
        __multiboot_start = .;
        *(.multiboot)
        __multiboot_end = .;

        __ap_start = .;
        *(.ap_bootstrap)
        __ap_end = .;

        __text_start = .;
        *(.text)
        __text_end = .;

        __userspace_start = .;
        *(.userspace)
        __userspace_end = .;
    }

    /* Read-only data */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
        __rodata_start = .;
		*(.rodata)
        __rodata_end = .;
	}

	/* Initialized data */
	.data BLOCK(4K) : ALIGN(4K)
	{
        __data_start = .;
		*(.data)
        __data_end = .;
	}

	/* BSS */
	.bss BLOCK(4K) : ALIGN(4K)
	{
        __bss_start = .;
		*(COMMON)
		*(.bss)
        __bss_end = .;
	}

    __kernel_end = .;
}