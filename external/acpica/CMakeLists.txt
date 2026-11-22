set(ACPICA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/acpica-src/source")
set(ACPICA_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/acpica")

file(GLOB SOURCES CONFIGURE_DEPENDS "${ACPICA_SOURCE_DIR}/components/*/*.c")
list(FILTER SOURCES EXCLUDE REGEX "/(debugger|disassembler)/|/resources/rsdump\\.c$")
file(GLOB_RECURSE ACPICA_HEADERS CONFIGURE_DEPENDS
    RELATIVE "${ACPICA_SOURCE_DIR}/include"
    "${ACPICA_SOURCE_DIR}/include/*.h"
)
foreach(HEADER IN LISTS ACPICA_HEADERS)
    if(NOT HEADER STREQUAL "platform/acenv.h")
        configure_file("${ACPICA_SOURCE_DIR}/include/${HEADER}" "${ACPICA_INCLUDE_DIR}/${HEADER}" COPYONLY)
    endif()
endforeach()

set(ACPICA_ENV_HEADER "${ACPICA_SOURCE_DIR}/include/platform/acenv.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ACPICA_ENV_HEADER}")
file(READ "${ACPICA_ENV_HEADER}" ACPICA_ENV)
string(REPLACE "aclinux.h" "achexahedron.h" ACPICA_ENV "${ACPICA_ENV}")
string(REPLACE "defined(_LINUX)" "defined(__HEXAHEDRON__)" ACPICA_ENV "${ACPICA_ENV}")
file(GENERATE OUTPUT "${ACPICA_INCLUDE_DIR}/platform/acenv.h" CONTENT "${ACPICA_ENV}")
configure_file(achexahedron.h "${ACPICA_INCLUDE_DIR}/platform/achexahedron.h" COPYONLY)

add_library(acpica STATIC ${SOURCES})
target_compile_definitions(acpica PRIVATE __HEXAHEDRON__ __KERNEL__)
target_compile_options(acpica PRIVATE -ffreestanding)
target_include_directories(acpica PUBLIC "${ACPICA_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/include")
target_link_libraries(acpica PRIVATE ethereal_kernel_abi kernel_headers structures_headers lib_headers)
add_dependencies(acpica libc)

install(TARGETS acpica ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY "${ACPICA_INCLUDE_DIR}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
