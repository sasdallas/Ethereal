list(APPEND SOURCES
    play.c
)

add_app(
    APP_NAME play
    SOURCES ${SOURCES}
    LIBRARIES symphony
)
