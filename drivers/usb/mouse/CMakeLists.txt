list(APPEND SOURCES
    mouse.c
)

add_driver(
    DRIVER_NAME usb_mouse
    ARCHITECTURES ANY
    SOURCES ${SOURCES}
)
