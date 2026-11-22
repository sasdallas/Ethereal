list(APPEND SOURCES
    shapes/rect.c
    blend.c
    blur.c
    gfx.c
    matrix.c
    pattern.c
    sprite.c
    text.c
    transform.c
)

add_lib(
    LIB_NAME graphics
    LIB_VERSION 1.1.0
    LIB_DESC "Graphics library for Ethereal"
    SOURCES ${SOURCES}
    DEPENDS freetype structures
    DEPENDS_PRIVATE m
    PKG_DEPENDS freetype2 structures
    EXTRA_LIBS_PRIVATE -lm
)
