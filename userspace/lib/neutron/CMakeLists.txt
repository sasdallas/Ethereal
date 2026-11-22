install(DIRECTORY neutron/include/neutron
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT headers
)

if(HEADERS_ONLY)
    return()
endif()

file(GLOB NEUTRON_SOURCES CONFIGURE_DEPENDS
    "neutron/neutron/*.c"
    "neutron/neutron/widgets/*.c"
)

list(APPEND SOURCES
    ${NEUTRON_SOURCES}
    neutron/neutron/platform/ethereal.c
    neutron/neutron/platform/ethereal_key.c
)

add_lib(
    LIB_NAME neutron
    LIB_VERSION 1.0.0
    LIB_DESC "Neutron GUI library"
    SOURCES ${SOURCES}
    DEPENDS celestial graphics keyboard
    DEPENDS_PRIVATE freetype m
    PKG_DEPENDS_PRIVATE freetype2
    EXTRA_LIBS_PRIVATE -lm
)

target_compile_definitions(neutron_obj PRIVATE BUILDING_ETHEREAL)
target_include_directories(neutron_obj PRIVATE neutron/include)
target_include_directories(neutron PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/neutron/include")

list(APPEND DEMO_SOURCES
    neutron/demo/demo.c
)

add_app(
    APP_NAME neutron_demo
    SOURCES ${DEMO_SOURCES}
    LIBRARIES neutron
)

install(DIRECTORY neutron/resources DESTINATION "${CMAKE_INSTALL_DATADIR}")
