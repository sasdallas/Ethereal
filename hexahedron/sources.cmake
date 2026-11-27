list(APPEND __DEBUG_SOURCES
    debug/debug_output.c
)

list(APPEND __DRIVER_SOURCES
    drivers/clock.c
    drivers/fb.c
    drivers/fb_linux.c
    drivers/grubvid.c
    drivers/net/arp.c
    drivers/net/ethernet.c
    drivers/net/icmp.c
    drivers/net/ipv4.c
    drivers/net/loopback.c
    drivers/net/nic.c
    drivers/net/route.c
    drivers/net/socket.c
    drivers/net/socket_fs.c
    drivers/net/tcp.c
    drivers/net/udp.c
    drivers/net/unix.c
    drivers/pci.c
    drivers/serial.c
    drivers/storage/drive.c
    drivers/storage/gpt.c
    drivers/storage/mbr.c
    drivers/storage/partition.c
    drivers/usb/device.c
    drivers/usb/driver.c
    drivers/usb/hid.c
    drivers/usb/hid_parser.c
    drivers/usb/roothub.c
    drivers/usb/usb.c
    drivers/usb/util.c
    drivers/virtio.c
)

list(APPEND __FILESYSTEM_SOURCES
    fs/devfs.c
    fs/fbcon.c
    fs/initrd.c
    fs/periphfs.c
    fs/pipe.c
    fs/poll.c
    fs/psuedo.c
    fs/random.c
    fs/shared.c
    fs/systemfs.c
    fs/systemfs_kernel.c
    fs/tmpfs.c
    fs/tty.c
    fs/vfs_new.c
)

list(APPEND __GRAPHICS_SOURCES
    gfx/flanterm/fb.c
    gfx/flanterm/flanterm.c
    gfx/gfx.c
    gfx/term.c
)

list(APPEND __CORE_SOURCES
    kernel/config.c
    kernel/init.c
    kernel/kernel.c
    kernel/panic/panic.c
    kernel/panic/panic_messages.c
    kernel/smp.c
    kernel/tasklet.c
)

list(APPEND __KLIB_SOURCES
    klib/stdio/printf.c
    klib/stdlib/abs.c
    klib/stdlib/assert.c
    klib/stdlib/inet.c
    klib/stdlib/malloc.c
    klib/string/ctype.c
    klib/string/memchr.c
    klib/string/memcmp.c
    klib/string/memcpy.c
    klib/string/memcpy_fast.S
    klib/string/memset.c
    klib/string/memset_fast.S
    klib/string/rand.c
    klib/string/select.c
    klib/string/strcat.c
    klib/string/strchr.c
    klib/string/strcmp.c
    klib/string/strcpy.c
    klib/string/strdup.c
    klib/string/strlen.c
    klib/string/strsignal.c
    klib/string/strstr.c
    klib/string/strto.c
    klib/string/strtok.c
    klib/time/time.c
)

list(APPEND __LOADER_SOURCES
    loader/binfmt.c
    loader/driver.c
    loader/elf64_loader.c
    loader/elf64_loaderv2.c
)

list(APPEND __LOCK_SOURCES
    lock/rwsem.c
)

list(APPEND __MISC_SOURCES
    misc/args.c
    misc/hash.c
    misc/ksym.c
    misc/mutex.c
    misc/semaphore.c
    misc/spinlock.c
    misc/stack.c
    misc/waitqueue.c
    misc/xarray.c
)

list(APPEND __MEMORY_SOURCES
    mm/alloc.c
    mm/cache.c
    mm/memleak.c
    mm/mm_sysfs.c
    mm/pmm.c
    mm/pmm_zero.c
    mm/slab.c
    mm/vmm.c
    mm/vmmclone.c
    mm/vmmfault.c
    mm/vmminit.c
    mm/vmmrange.c
    mm/vmmspecial.c
)

list(APPEND __SUBSYSTEM_SOURCES
    subsystems/audio.c
    subsystems/auth.c
    subsystems/clock.c
    subsystems/irq.c
    subsystems/timemonitor.c
    subsystems/timer.c
)

list(APPEND __TASK_SOURCES
    task/fd.c
    task/futex.c
    task/process.c
    task/ptrace.c
    task/reaper.c
    task/sched/sched.c
    task/sched/sched_dumb.c
    task/sched/sched_ule.c
    task/session.c
    task/signal.c
    task/sleep.c
    task/syscall.c
    task/syscall_ethereal.c
    task/syscall_trace.c
    task/thread.c
    task/workqueue.c
    task/itimer.c
)

list(APPEND __SYSCALL_SOURCES
    task/syscalls/accept.c
    task/syscalls/access.c
    task/syscalls/bind.c
    task/syscalls/chdir.c
    task/syscalls/clock_gettime.c
    task/syscalls/close.c
    task/syscalls/connect.c
    task/syscalls/dup2.c
    task/syscalls/execve.c
    task/syscalls/exit.c
    task/syscalls/faccessat.c
    task/syscalls/fchmodat.c
    task/syscalls/fchownat.c
    task/syscalls/fcntl.c
    task/syscalls/flock.c
    task/syscalls/fork.c
    task/syscalls/fstatat.c
    task/syscalls/fsync.c
    task/syscalls/ftruncate.c
    task/syscalls/futex.c
    task/syscalls/getcwd.c
    task/syscalls/getpeername.c
    task/syscalls/getsockname.c
    task/syscalls/getsockopt.c
    task/syscalls/hostname.c
    task/syscalls/id.c
    task/syscalls/ioctl.c
    task/syscalls/kill.c
    task/syscalls/linkat.c
    task/syscalls/listen.c
    task/syscalls/lseek.c
    task/syscalls/mkdir.c
    task/syscalls/mknodat.c
    task/syscalls/mmap.c
    task/syscalls/mount.c
    task/syscalls/mprotect.c
    task/syscalls/msync.c
    task/syscalls/open.c
    task/syscalls/openpty.c
    task/syscalls/pause.c
    task/syscalls/pipe.c
    task/syscalls/poll.c
    task/syscalls/pread.c
    task/syscalls/pselect.c
    task/syscalls/ptrace.c
    task/syscalls/pwrite.c
    task/syscalls/read.c
    task/syscalls/read_entries.c
    task/syscalls/readlinkat.c
    task/syscalls/recvmsg.c
    task/syscalls/renameat.c
    task/syscalls/sendmsg.c
    task/syscalls/setitimer.c
    task/syscalls/setsockopt.c
    task/syscalls/sigaction.c
    task/syscalls/sigaltstack.c
    task/syscalls/sigpending.c
    task/syscalls/sigprocmask.c
    task/syscalls/sigreturn.c
    task/syscalls/sigsuspend.c
    task/syscalls/sigwait.c
    task/syscalls/socket.c
    task/syscalls/socketpair.c
    task/syscalls/stat.c
    task/syscalls/symlinkat.c
    task/syscalls/timeofday.c
    task/syscalls/times.c
    task/syscalls/tkill.c
    task/syscalls/umask.c
    task/syscalls/umount.c
    task/syscalls/uname.c
    task/syscalls/unlinkat.c
    task/syscalls/usleep.c
    task/syscalls/wait.c
    task/syscalls/write.c
    task/syscalls/yield.c
)

if(BUILD_ARCH STREQUAL "x86_64")
    list(APPEND __ARCH_SOURCES
        arch/x86_64/arch.c
        arch/x86_64/boot.S
        arch/x86_64/context.S
        arch/x86_64/cpu.c
        arch/x86_64/hal.c
        arch/x86_64/interrupt.c
        arch/x86_64/irq.S
        arch/x86_64/misc.S
        arch/x86_64/mmu.c
        arch/x86_64/multiboot.c
        arch/x86_64/smp.c
        arch/x86_64/smp_trampoline.S
        arch/x86_64/util.c
        drivers/x86/acpica_interface.c
        drivers/x86/acpica_osl.c
        drivers/x86/clock.c
        drivers/x86/early_log.c
        drivers/x86/io_apic.c
        drivers/x86/irq.c
        drivers/x86/local_apic.c
        drivers/x86/minacpi.c
        drivers/x86/pic.c
        drivers/x86/pit.c
        drivers/x86/serial.c
    )
else()
    message(FATAL_ERROR "Add architecture sources!!!")
endif()

list(APPEND __KERNEL_SOURCES
    ${__DEBUG_SOURCES}
    ${__DRIVER_SOURCES}
    ${__FILESYSTEM_SOURCES}
    ${__GRAPHICS_SOURCES}
    ${__CORE_SOURCES}
    ${__KLIB_SOURCES}
    ${__LOADER_SOURCES}
    ${__LOCK_SOURCES}
    ${__MISC_SOURCES}
    ${__MEMORY_SOURCES}
    ${__SUBSYSTEM_SOURCES}
    ${__TASK_SOURCES}
    ${__SYSCALL_SOURCES}
    ${__ARCH_SOURCES}
)