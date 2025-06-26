ENTRY(_start)

SECTIONS
{
	. = 0x3F000000;
	__linker_start = .;

	.text BLOCK(4K) : ALIGN(4K)
	{
		__text_start = .;
		*(.text)
		__text_end = .;
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		__data_start = .;
		*(.data)
		__data_end = .;
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		__rodata_start = .;
		*(.rodata)
		__rodata_end = .;
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		__bss_start = .;
		*(COMMON)
		*(.bss)
		__bss_end = .;
	}

	.eh_frame BLOCK(4K) : ALIGN(4K)
	{
		__eh_frame_start = .;
		*(.eh_frame)
		__eh_frame_end = .;
	}

	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		*(.init_array)
		PROVIDE_HIDDEN (__init_array_end = .);
	}

	__linker_end = .;

	/DISCARD/ :
	{
		*(.comment)
		*(.note.gnu.build-id)
	}

}