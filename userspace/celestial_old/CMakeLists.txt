list(APPEND SOURCES
    event.c
    kbd.c
    main.c
    mouse.c
    socket.c
    window.c
)

add_app(
    APP_NAME celestial_old
    SOURCES ${SOURCES}
    LIBRARIES graphics ethereal structures
)
