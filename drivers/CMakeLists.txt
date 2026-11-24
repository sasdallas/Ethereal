if(HEADERS_ONLY)
    return()
endif()

add_custom_target(drivers)

function(add_driver)
    cmake_parse_arguments(OPT "" "DRIVER_NAME" "ARCHITECTURES;SOURCES;LIBRARIES" ${ARGN})

    if(NOT ("ANY" IN_LIST OPT_ARCHITECTURES OR BUILD_ARCH_UPPER IN_LIST OPT_ARCHITECTURES))
        return()
    endif()

    # drivers should be compiled as object libraries, then transformed into .sys files
    set(TARGET "${OPT_DRIVER_NAME}")
    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${OPT_DRIVER_NAME}.sys")
    add_library(${TARGET} OBJECT ${OPT_SOURCES})

    target_compile_definitions(${TARGET} PRIVATE __KERNEL_DRIVER__)
    target_compile_options(${TARGET} PRIVATE
        -ffreestanding -O2 -g -Wall -Wextra -Werror -fno-pie
        -Wno-unused-parameter -Wno-unused-function
    )

    # drivers can be compiled before headers are installed
    target_include_directories(${TARGET} SYSTEM PRIVATE
        "$<TARGET_PROPERTY:kernel_headers,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:structures_headers,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:lib_headers,INTERFACE_INCLUDE_DIRECTORIES>"
    )

    # they also need the kernel ABI
    target_link_libraries(${TARGET} PRIVATE ethereal_kernel_abi)
    if (BUILD_ARCH STREQUAL "x86_64")
        target_compile_options(${TARGET} PRIVATE -mno-mmx -mno-sse -mno-sse2)
    endif()

    set(DRIVER_LIBRARIES)
    foreach(LIBRARY IN LISTS OPT_LIBRARIES)
        target_link_libraries(${TARGET} PRIVATE "${LIBRARY}")
        list(APPEND DRIVER_LIBRARIES "$<TARGET_FILE:${LIBRARY}>")
    endforeach()

    # The kernel loads drivers as relocatable ELF objects
    set(OBJECTS $<TARGET_OBJECTS:${TARGET}>)
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND "${CMAKE_LINKER}" -r -o "${OUTPUT}" ${OBJECTS} ${DRIVER_LIBRARIES}
        DEPENDS ${TARGET} ${OBJECTS} ${OPT_LIBRARIES}
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target("${OPT_DRIVER_NAME}.sys" ALL DEPENDS "${OUTPUT}")
    install(FILES "${OUTPUT}" DESTINATION "${ETHEREAL_BOOT_DIR}/drivers")
    install(FILES "${OUTPUT}" DESTINATION "${ETHEREAL_INITRD_DIR}/boot/drivers")
    set_property(DIRECTORY "${CMAKE_CURRENT_FUNCTION_LIST_DIR}" APPEND PROPERTY DRIVER_CONFIG_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")

    add_dependencies(drivers "${OPT_DRIVER_NAME}.sys")
endfunction()

include("drivers.cmake")

foreach(DRIVER IN LISTS HEXAHEDRON_DRIVERS)
    add_subdirectory("${DRIVER}")
endforeach()

# JSON generation

get_property(DRIVER_CONFIG_DIRS DIRECTORY PROPERTY DRIVER_CONFIG_DIRS)
set(DRIVER_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/driver_conf.json")
set(DRIVER_CONFIG_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../buildscripts/create_driver_data.py")

# depend on the driver.conf files, if they change
set(DRIVER_CONFIG_INPUTS )
foreach(DIRECTORY IN LISTS DRIVER_CONFIG_DIRS)
    list(APPEND DRIVER_CONFIG_INPUTS "${DIRECTORY}/driver.conf")
endforeach()

add_custom_command(
    OUTPUT "${DRIVER_CONFIG}"
    COMMAND "${Python3_EXECUTABLE}" "${DRIVER_CONFIG_SCRIPT}" ${DRIVER_CONFIG_DIRS}
    DEPENDS "${DRIVER_CONFIG_SCRIPT}" ${DRIVER_CONFIG_INPUTS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM
)

add_custom_target(driver_config ALL DEPENDS "${DRIVER_CONFIG}")

install(FILES "${DRIVER_CONFIG}" DESTINATION "${ETHEREAL_BOOT_DIR}/drivers")
install(FILES "${DRIVER_CONFIG}" DESTINATION "${ETHEREAL_INITRD_DIR}/boot/drivers")

add_dependencies(drivers driver_config)
