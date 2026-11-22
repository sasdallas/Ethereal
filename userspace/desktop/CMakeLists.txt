list(APPEND SOURCES
    desktop.c
)

add_app(
    APP_NAME desktop
    SOURCES ${SOURCES}
    LIBRARIES celestial version
)
