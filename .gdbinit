file build-output/sysroot/boot/hexahedron-kernel.elf
symbol-file build-output/sysroot/boot/hexahedron-kernel-symbols.sym
target remote 127.0.0.1:1234

define process_info
    set $proc = processor_data[$arg0].current_process
    set $thr = processor_data[$arg0].current_thread
    printf "CPU%d: Process %s (PID %d) - pd %p heap %p - %p\n", $arg0, $proc->name, $proc->pid, $proc->dir, $proc->heap_base, $proc->heap
    printf "\tExecuting thread %p with stack %p page dir %p\n", $thr, $thr->stack, $thr->dir
end

define cpus
    set $ipx = 0
    set $end = processor_count
    while ($ipx<$end)
        process_info $ipx
        set $ipx=$ipx+1
    end
end

define debug-elf
	# GDB can use add-symbol-file <file> <.text offset>, and that can be calculated with readelf
	# This just makes my life easier!
	# https://stackoverflow.com/questions/20380204/how-to-load-multiple-symbol-files-in-gdb
	# Parse the .text address into a temporary file (that's a bash script)
	shell echo set \$text_address=$(readelf -WS $arg0 | grep .text | awk '{print "0x"$5 }') > /tmp/temp_gdb_text_address.txt
	
	# Load it and delete the temporary file
	source /tmp/temp_gdb_text_address.txt
	shell rm -f /tmp/temp_gdb_text_address.txt

	# Load the symbol file
	add-symbol-file $arg0 $text_address
end

define debug-driver
    set $driver = $arg0
    set $addr = $arg1

    add-symbol-file $arg0 $addr
end

define offsetof
    print &(($arg0 *)0x0)->$arg1
end