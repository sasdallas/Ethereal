set(LINKER_FILE "arch/x86_64/linker.ld")

set(ARCH_CFLAGS
    -mno-mmx
    -mno-sse
    -mno-sse2
    -mfsgsbase
    -mgeneral-regs-only
    -fno-omit-frame-pointer
    -mno-omit-leaf-frame-pointer
    -Wno-unused-local-typedefs
    -fstack-protector
)

set(ARCH_LDFLAGS
    -z max-page-size=0x1000
)
