list(APPEND SOURCES
    font-viewer.c
)

add_app(
    APP_NAME font-viewer
    SOURCES ${SOURCES}
    LIBRARIES keyboard celestial
)
