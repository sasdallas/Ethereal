list(APPEND SOURCES
    ansi.c
)

add_lib(
    LIB_NAME ansi
    LIB_VERSION 1.1.1
    LIB_DESC "ANSI parser library"
    SOURCES ${SOURCES}
    DEPENDS graphics
)
