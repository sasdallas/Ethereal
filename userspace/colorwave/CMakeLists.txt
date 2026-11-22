list(APPEND SOURCES
    colorwave.c
)

add_app(
    APP_NAME colorwave
    SOURCES ${SOURCES}
    LIBRARIES celestial
)
