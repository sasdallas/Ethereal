list(APPEND SOURCES
    sudo.c
)

add_app(
    APP_NAME sudo
    SOURCES ${SOURCES}
)
