list(APPEND SOURCES
    file-browser.c
)

add_app(
    APP_NAME file-browser
    SOURCES ${SOURCES}
    LIBRARIES neutron
)
