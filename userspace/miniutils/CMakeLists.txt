file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.c"
)

foreach(SOURCE IN LISTS SOURCES)
    get_filename_component(APP_NAME "${SOURCE}" NAME_WE)
    add_app(
        APP_NAME ${APP_NAME}
        TARGET_NAME miniutils_${APP_NAME}
        SOURCES ${SOURCE}
        LIBRARIES version ethereal structures
    )
endforeach()
