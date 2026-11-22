list(APPEND SOURCES
    celestial-theme.c
)

add_app(
    APP_NAME celestial-theme
    SOURCES ${SOURCES}
    LIBRARIES celestial
)
