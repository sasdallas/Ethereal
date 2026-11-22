add_library(structures_headers INTERFACE)
file(GLOB_RECURSE STRUCTURES_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

target_sources(structures_headers INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${STRUCTURES_HEADERS}
)

install(TARGETS structures_headers
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT headers
)

if(HEADERS_ONLY)
    return()
endif()

include("sources.cmake")

set(STRUCTURES_CFLAGS
    -ffreestanding -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter
)

add_library(kstructures STATIC ${SOURCES})
target_compile_definitions(kstructures PRIVATE
    __LIBKSTRUCTURES __KERNEL__
)
target_compile_options(kstructures PRIVATE ${STRUCTURES_CFLAGS})
target_link_libraries(kstructures PRIVATE structures_headers kernel_headers lib_headers ethereal_kernel_abi)
add_dependencies(kstructures libc)
install(TARGETS kstructures ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

add_library(structures SHARED ${SOURCES})
target_compile_definitions(structures PRIVATE __LIBCSTRUCTURES)
target_compile_options(structures PRIVATE ${STRUCTURES_CFLAGS})
target_link_libraries(structures PUBLIC structures_headers)
add_dependencies(structures libc)
install(TARGETS structures LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(FILES structures.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
