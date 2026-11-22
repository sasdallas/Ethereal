list(APPEND SOURCES
    damage.c
    input_generic.c
    ipc.c
    main.c
    renderer_generic.c
    request.c
    window.c
    backend/input_ethereal.c
    backend/renderer_ethereal.c
)

add_app(
    APP_NAME celestial
    TARGET_NAME celestial_wm
    SOURCES ${SOURCES}
    LIBRARIES graphics ethereal structures keyboard freetype
)
