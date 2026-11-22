list(APPEND SOURCES
    show-dialog.c
)

add_app(
    APP_NAME show-dialog
    SOURCES ${SOURCES}
    LIBRARIES keyboard widget celestial
)
