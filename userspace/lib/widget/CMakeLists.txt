list(APPEND SOURCES
    button.c
    event.c
    frame.c
    geometry.c
    input.c
    label.c
    menu.c
    widget.c
)

add_lib(
    LIB_NAME widget
    LIB_VERSION 1.0.0
    LIB_DESC "Widget library"
    SOURCES ${SOURCES}
    DEPENDS celestial graphics keyboard
)
