list(APPEND SOURCES
    getty.c
)

add_app(
    APP_NAME getty
    SOURCES ${SOURCES}
    LIBRARIES version
)
