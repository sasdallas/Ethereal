list(APPEND SOURCES
    memvisualize.c
)

add_app(
    APP_NAME memvisualize
    SOURCES ${SOURCES}
    LIBRARIES celestial
)
