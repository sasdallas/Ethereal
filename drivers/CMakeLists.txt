if(HEADERS_ONLY)
    return()
endif()

function(add_driver)
    cmake_parse_arguments(OPT "" "DRIVER_NAME" "ARCHITECTURES;SOURCES;LIBRARIES" ${ARGN})

    if(NOT ("ANY" IN_LIST OPT_ARCHITECTURES OR BUILD_ARCH_UPPER IN_LIST OPT_ARCHITECTURES))
        return()
    endif()

    add_library(${OPT_DRIVER_NAME}_objects OBJECT ${OPT_SOURCES})
    target_compile_definitions(${OPT_DRIVER_NAME}_objects PRIVATE __KERNEL_DRIVER__)
    target_compile_options(${OPT_DRIVER_NAME}_objects PRIVATE
        -ffreestanding -O2 -g -Wall -Wextra -Werror -fno-pie
        -Wno-unused-parameter -Wno-unused-function
    )

    target_include_directories(${OPT_DRIVER_NAME}_objects SYSTEM PRIVATE
        "$<TARGET_PROPERTY:kernel_headers,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:structures_headers,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:lib_headers,INTERFACE_INCLUDE_DIRECTORIES>"
    )
    target_link_libraries(${OPT_DRIVER_NAME}_objects PRIVATE ethereal_kernel_abi)
    add_dependencies(${OPT_DRIVER_NAME}_objects libc)

    if(BUILD_ARCH_UPPER STREQUAL "X86_64")
        target_compile_options(${OPT_DRIVER_NAME}_objects PRIVATE
            -mno-mmx -mno-sse -mno-sse2 -fno-omit-frame-pointer
        )
    endif()

    set(DRIVER_LIBRARIES)
    foreach(LIBRARY IN LISTS OPT_LIBRARIES)
        target_link_libraries(${OPT_DRIVER_NAME}_objects PRIVATE "${LIBRARY}")
        list(APPEND DRIVER_LIBRARIES "$<TARGET_FILE:${LIBRARY}>")
    endforeach()

    # The kernel loads drivers as relocatable ELF objects.
    set(OBJECTS $<TARGET_OBJECTS:${OPT_DRIVER_NAME}_objects>)
    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OPT_DRIVER_NAME}.sys")
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND "${CMAKE_LINKER}" -r -o "${OUTPUT}" ${OBJECTS} ${DRIVER_LIBRARIES}
        DEPENDS ${OPT_DRIVER_NAME}_objects ${OBJECTS} ${OPT_LIBRARIES}
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${OPT_DRIVER_NAME} ALL DEPENDS "${OUTPUT}")
    install(FILES "${OUTPUT}" DESTINATION "${ETHEREAL_BOOT_DIR}/drivers")
    install(FILES "${OUTPUT}" DESTINATION "${ETHEREAL_INITRD_DIR}/boot/drivers")
    set_property(DIRECTORY "${CMAKE_CURRENT_FUNCTION_LIST_DIR}" APPEND PROPERTY DRIVER_CONFIG_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

include("drivers.cmake")

foreach(DRIVER IN LISTS HEXAHEDRON_DRIVERS)
    add_subdirectory("${DRIVER}")
endforeach()

# JSON generation

get_property(DRIVER_CONFIG_DIRS DIRECTORY PROPERTY DRIVER_CONFIG_DIRS)
set(DRIVER_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/driver_conf.json")
set(DRIVER_CONFIG_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../buildscripts/create_driver_data.py")

set(DRIVER_CONFIG_INPUTS )
foreach(DIRECTORY IN LISTS DRIVER_CONFIG_DIRS)
    list(APPEND DRIVER_CONFIG_INPUTS "${DIRECTORY}/driver.conf")
endforeach()

set(DRIVER_CONFIG_LIST "${CMAKE_CURRENT_BINARY_DIR}/driver_config_inputs.txt")
string(JOIN "\n" DRIVER_CONFIG_CONTENT ${DRIVER_CONFIG_INPUTS})
file(GENERATE OUTPUT "${DRIVER_CONFIG_LIST}" CONTENT "${DRIVER_CONFIG_CONTENT}\n")

add_custom_command(
    OUTPUT "${DRIVER_CONFIG}"
    COMMAND "${Python3_EXECUTABLE}" "${DRIVER_CONFIG_SCRIPT}" ${DRIVER_CONFIG_DIRS}
    DEPENDS "${DRIVER_CONFIG_SCRIPT}" ${DRIVER_CONFIG_INPUTS} "${DRIVER_CONFIG_LIST}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM
)

add_custom_target(driver_config ALL DEPENDS "${DRIVER_CONFIG}")

install(FILES "${DRIVER_CONFIG}" DESTINATION "${ETHEREAL_BOOT_DIR}/drivers")
install(FILES "${DRIVER_CONFIG}" DESTINATION "${ETHEREAL_INITRD_DIR}/boot/drivers")
