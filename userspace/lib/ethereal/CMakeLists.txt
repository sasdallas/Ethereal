list(APPEND SOURCES
    driver.c
    reboot.c
    shared.c
)

add_lib(
    LIB_NAME ethereal
    LIB_VERSION 1.1.0
    LIB_DESC "Generic Ethereal library"
    SOURCES ${SOURCES}
)
