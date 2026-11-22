list(APPEND SOURCES
    desktopmgr.c
)

add_app(
    APP_NAME desktopmgr
    SOURCES ${SOURCES}
    LIBRARIES neutron
)
