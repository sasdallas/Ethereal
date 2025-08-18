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

define print_sleep_state_as_string
    set $sleep = (thread_sleep_t*)$arg0

    if $sleep->sleep_state == 0
        printf "NOCOND"
    end
    if $sleep->sleep_state == 1
        printf "WAKEUP_NOW"
    end
    if $sleep->sleep_state == 2
        printf "TIME"
    end
    if $sleep->sleep_state == 3
        printf "COND"
    end
    if $sleep->sleep_state == 4
        printf "WAKEUP_SIGNAL"
    end
    if $sleep->sleep_state == 5
        printf "WAKEUP_TIME"
    end
    if $sleep->sleep_state == 6
        printf "WAKEUP_COND"
    end
    if $sleep->sleep_state == 7
        printf "WAKEUP_ANOTHER_THREAD"
    end
end

define procqueue
    set $n = thread_queue->head
    while ($n)
        printf "Thread %p in queue (TID: %d, belongs to process %s:%d)\n", $n->value, ((thread_t*)$n->value)->tid, ((thread_t*)$n->value)->parent->name, ((thread_t*)$n->value)->parent->pid
        set $n = $n->next
    end
end

define sleepers
    set $n = sleep_queue->head
    while ($n)
        set $sleep = (thread_sleep_t*)$n->value
        printf "Thread %p is sleeping (TID: %d, belongs to process %s:%d)\n", $sleep->thread, $sleep->thread->tid, $sleep->thread->parent->name, $sleep->thread->parent->pid
        printf "\tSleeping until: "
        print_sleep_state_as_string $sleep
        printf " (context=%p)\n", $sleep->context

        if $sleep->context
            printf "\tPut here by: "
            info symbol $sleep->context
            printf "\n"
        end

        set $n = $n->next
    end
end

define process-list-thread
    set $t = (thread_t*)$arg0

    printf "\tThread %d (%p): ", $t->tid, $t

    if $t->status & 0x01
        printf "KERNEL, "
    end

    if $t->status & 0x02
        printf "STOPPED, "
    end

    if $t->status & 0x04
        printf "RUNNING, "
    end

    if $t->status & 0x08
        printf "SLEEPING, "
    end

    if $t->status & 0x10
        printf "STOPPING, "
    end

    printf "\n"
end

define process-list
    set $n = process_list->head
    while ($n)
        set $p = (process_t*)$n->value
        set $thread_count = 1

        if $p->thread_list
            set $thread_count = 1 + $p->thread_list->length
        end

        set $parent_name = "N/A"
        set $parent_pid = -1

        if $p->parent   
            set $parent_name = $p->parent->name
            set $parent_pid = $p->parent->pid
        end

        printf "Process %s (PID %d) contains %d threads, owned by UID %d GID %d and parented by %s:%d\n", $p->name, $p->pid, $thread_count, $p->uid, $p->gid, $parent_name, $parent_pid
    
        process-list-thread $p->main_thread

        if $p->thread_list
            set $n2 = $p->thread_list->head
            while ($n2)
                process-list-thread $n2->value
                set $n2 = $n2->next
            end
        end

        set $n = $n->next
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