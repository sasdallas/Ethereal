list(APPEND SOURCES
    etherealver.c
)

add_app(
    APP_NAME etherealver
    SOURCES ${SOURCES}
    LIBRARIES celestial version
)
