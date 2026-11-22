list(APPEND SOURCES
    storage.c
)

add_driver(
    DRIVER_NAME usb_storage
    ARCHITECTURES X86_64
    SOURCES ${SOURCES}
)
