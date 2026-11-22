list(APPEND SOURCES
    login.c
)

add_app(
    APP_NAME login-gui
    SOURCES ${SOURCES}
    LIBRARIES keyboard celestial version widget
)
