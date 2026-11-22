list(APPEND SOURCES
    wallpaper-chooser.c
)

add_app(
    APP_NAME wallpaper-chooser
    SOURCES ${SOURCES}
    LIBRARIES neutron
)
