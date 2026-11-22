list(APPEND SOURCES
    terminal.c
)

add_app(
    APP_NAME terminal
    SOURCES ${SOURCES}
    LIBRARIES keyboard
)
