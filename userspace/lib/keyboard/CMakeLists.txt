list(APPEND SOURCES
    keyboard.c
)

add_lib(
    LIB_NAME keyboard
    LIB_VERSION 1.1.0
    LIB_DESC "Keyboard library"
    SOURCES ${SOURCES}
)
