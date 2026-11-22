list(APPEND SOURCES
    taskbar.c
)

add_app(
    APP_NAME taskbar
    SOURCES ${SOURCES}
    LIBRARIES neutron
)
