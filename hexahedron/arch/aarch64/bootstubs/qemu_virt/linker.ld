ENTRY(_start)

SECTIONS {
    . = 0x40100000;

    .startup . : { boot.o(.text) }

    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss COMMON) }
    
    __stack_bottom = .;
    . = ALIGN(8);
    . += 0x1000;
    __stack_top = .;       

    __kernel_tmp_load_address = .;
}