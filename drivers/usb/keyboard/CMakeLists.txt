list(APPEND SOURCES
    keyboard.c
)

add_driver(
    DRIVER_NAME usb_keyboard
    ARCHITECTURES ANY
    SOURCES ${SOURCES}
)
