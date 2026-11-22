list(APPEND SOURCES
    hub.c
)

add_driver(
    DRIVER_NAME usb_hub
    ARCHITECTURES X86_64
    SOURCES ${SOURCES}
)
