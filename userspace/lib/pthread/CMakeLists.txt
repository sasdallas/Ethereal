list(APPEND SOURCES
    dummy.c
)

add_lib(
    NO_SHARED
    LIB_NAME pthread
    LIB_VERSION 1.0.0
    LIB_DESC "Stub pthread library"
    SOURCES ${SOURCES}
)
