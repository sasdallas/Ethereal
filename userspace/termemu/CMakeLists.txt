list(APPEND SOURCES
    terminal.c
)

add_app(
    APP_NAME termemu
    SOURCES ${SOURCES}
    LIBRARIES keyboard celestial ansi
)
