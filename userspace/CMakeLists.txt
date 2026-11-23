include("userspace.cmake")

add_custom_target(userspace)

function(add_app)
    set(ONEVALUE APP_NAME TARGET_NAME)
    set(MULTIVALUE LIBRARIES SOURCES)
    cmake_parse_arguments(OPT "" "${ONEVALUE}" "${MULTIVALUE}" ${ARGN})

    if(NOT DEFINED OPT_TARGET_NAME)
        set(OPT_TARGET_NAME ${OPT_APP_NAME})
    endif()

    add_executable(${OPT_TARGET_NAME} ${OPT_SOURCES})
    set_target_properties(${OPT_TARGET_NAME} PROPERTIES OUTPUT_NAME ${OPT_APP_NAME})
    target_link_libraries(${OPT_TARGET_NAME} PRIVATE lib_headers ${OPT_LIBRARIES} m)

    add_dependencies(${OPT_TARGET_NAME} libc)

    install(TARGETS ${OPT_TARGET_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    add_dependencies(userspace ${OPT_TARGET_NAME})
endfunction()

add_subdirectory("lib")
add_dependencies(userspace userspace_lib)

if(HEADERS_ONLY)
    return()
endif()

foreach(APP IN LISTS USERSPACE_APPS)
    add_subdirectory("${APP}")
endforeach()
