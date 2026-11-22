list(APPEND SOURCES
    nvme.c
)

add_driver(
    DRIVER_NAME nvme
    ARCHITECTURES I386 X86_64
    SOURCES ${SOURCES}
)
