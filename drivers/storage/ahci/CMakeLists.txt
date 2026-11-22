list(APPEND SOURCES
    ahci.c
)

add_driver(
    DRIVER_NAME ahci
    ARCHITECTURES I386 X86_64
    SOURCES ${SOURCES}
)
