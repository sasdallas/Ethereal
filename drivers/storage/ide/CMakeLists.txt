list(APPEND SOURCES
    ata.c
    main.c
)

add_driver(
    DRIVER_NAME ide_ata
    ARCHITECTURES I386 X86_64
    SOURCES ${SOURCES}
)
