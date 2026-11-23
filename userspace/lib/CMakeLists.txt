add_custom_target(userspace_lib)

add_library(lib_headers INTERFACE)
file(GLOB_RECURSE LIBRARY_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

target_sources(lib_headers INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${LIBRARY_HEADERS}
)

install(TARGETS lib_headers
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT headers
)

if(HEADERS_ONLY)
    add_subdirectory(neutron)
    return()
endif()

target_link_libraries(lib_headers INTERFACE kernel_headers structures_headers)

include("libraries.cmake")

function(add_lib)
    set(OPTIONS STATIC NO_SHARED)
    set(ONEVALUE LIB_NAME LIB_VERSION LIB_DESC)
    set(MULTIVALUE SOURCES DEPENDS DEPENDS_PRIVATE PKG_DEPENDS PKG_DEPENDS_PRIVATE EXTRA_LIBS_PRIVATE)
    cmake_parse_arguments(OPT "${OPTIONS}" "${ONEVALUE}" "${MULTIVALUE}" ${ARGN})

    if(NOT DEFINED OPT_PKG_DEPENDS)
        set(OPT_PKG_DEPENDS ${OPT_DEPENDS})
    endif()

    string(JOIN ", " PC_REQUIRES ${OPT_PKG_DEPENDS})
    string(JOIN ", " PC_REQUIRES_PRIVATE ${OPT_PKG_DEPENDS_PRIVATE})
    string(JOIN " " PC_LIBS_PRIVATE ${OPT_EXTRA_LIBS_PRIVATE})

    set(OBJ_TARGET "${OPT_LIB_NAME}_obj")
    add_library(${OBJ_TARGET} OBJECT ${OPT_SOURCES})
    add_dependencies(${OBJ_TARGET} libc)
    target_link_libraries(${OBJ_TARGET}
        PUBLIC lib_headers ${OPT_DEPENDS}
        PRIVATE ${OPT_DEPENDS_PRIVATE}
    )

    set_target_properties(${OBJ_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    set(TARGETS)
    if(NOT OPT_NO_SHARED)
        add_library(${OPT_LIB_NAME} SHARED)
        target_link_libraries(${OPT_LIB_NAME}
            PRIVATE ${OBJ_TARGET}
            PUBLIC ${OPT_DEPENDS}
        )
        string(REGEX MATCH "^[0-9]+" ABI_VERSION "${OPT_LIB_VERSION}")
        set_target_properties(${OPT_LIB_NAME} PROPERTIES
            VERSION ${OPT_LIB_VERSION}
            SOVERSION ${ABI_VERSION}
        )
        list(APPEND TARGETS ${OPT_LIB_NAME})
    endif()

    if(OPT_STATIC OR OPT_NO_SHARED)
        if(OPT_NO_SHARED)
            set(STATIC_TARGET "${OPT_LIB_NAME}")
        else()
            set(STATIC_TARGET "${OPT_LIB_NAME}_static")
        endif()
        add_library(${STATIC_TARGET} STATIC)
        target_link_libraries(${STATIC_TARGET} PUBLIC ${OBJ_TARGET})
        set_target_properties(${STATIC_TARGET} PROPERTIES OUTPUT_NAME ${OPT_LIB_NAME})
        list(APPEND TARGETS ${STATIC_TARGET})
    endif()

    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/lib.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${OPT_LIB_NAME}.pc"
        @ONLY
    )

    install(TARGETS ${TARGETS}
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${OPT_LIB_NAME}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )

    add_dependencies(userspace_lib ${TARGETS})
endfunction()

foreach(LIB IN LISTS LIBRARIES)
    add_subdirectory("${LIB}")
endforeach()
