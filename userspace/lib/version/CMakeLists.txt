list(APPEND SOURCES
    version.c
)

add_lib(
    STATIC
    LIB_NAME version
    LIB_VERSION 1.0.0
    LIB_DESC "Version library"
    SOURCES ${SOURCES}
)
