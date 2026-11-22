list(APPEND SOURCES
    celestial.c
    decor.c
    event.c
    mercury.c
    mintia.c
    request.c
    window.c
)

add_lib(
    LIB_NAME celestial
    LIB_VERSION 1.2.0
    LIB_DESC "Window manager library"
    SOURCES ${SOURCES}
    DEPENDS graphics structures ethereal
)
