list(APPEND SOURCES
    plasma.c
)

add_app(
    APP_NAME plasma
    SOURCES ${SOURCES}
    LIBRARIES celestial
)
