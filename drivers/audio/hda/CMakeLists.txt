list(APPEND SOURCES
    hda.c
)

add_driver(
    DRIVER_NAME intel_hda
    ARCHITECTURES I386 X86_64
    SOURCES ${SOURCES}
)
