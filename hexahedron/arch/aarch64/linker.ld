ENTRY(_start)


SECTIONS
{
    . = 0xFFFFF00000000000;

    __kernel_start = .;

    .text ALIGN (4K) : AT (ADDR (.text) - 0xFFFFF00000000000)
    {
        __bootstrap_start = .;
        *(.bootstrap)
        __bootstrap_end = .;

        __text_start = .;
        *(.text)
        __text_end = .;

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

    __stack_bottom = .;
    . += 0x10000;
    __stack_top = .;


    __kernel_end = .;

    /DISCARD/ :
	{
		*(.comment)
		*(.eh_frame)
		*(.note.gnu.build-id)
	}
}