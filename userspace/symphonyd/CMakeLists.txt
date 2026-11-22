list(APPEND SOURCES
    symphonyd.c
)

add_app(
    APP_NAME symphonyd
    SOURCES ${SOURCES}
    LIBRARIES ethereal structures
)
