list(APPEND SOURCES
    symphony.c
)

add_lib(
    LIB_NAME symphony
    LIB_VERSION 1.0.0
    LIB_DESC "Symphony audio library"
    SOURCES ${SOURCES}
    DEPENDS ethereal structures
)
