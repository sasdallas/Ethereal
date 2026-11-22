list(APPEND SOURCES
    login.c
)

add_app(
    APP_NAME login
    SOURCES ${SOURCES}
    LIBRARIES version
)
