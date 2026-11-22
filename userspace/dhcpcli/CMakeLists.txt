list(APPEND SOURCES
    dhcpcli.c
)

add_app(
    APP_NAME dhcpcli
    SOURCES ${SOURCES}
)
