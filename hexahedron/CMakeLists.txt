# Main build file for the Hexahedron kernel

add_library(kernel_headers INTERFACE)
file(GLOB_RECURSE KERNEL_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

target_sources(kernel_headers INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${KERNEL_HEADERS}
)

install(TARGETS kernel_headers
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT headers
)

if(HEADERS_ONLY)
    return()
endif()

include("sources.cmake")
include("arch/${BUILD_ARCH}/config.cmake")

# forgot why this is needed
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_LINKER> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>"
)

add_executable(hexahedron-kernel ${__KERNEL_SOURCES})
set_target_properties(hexahedron-kernel PROPERTIES SUFFIX ".unstripped.elf")
target_link_libraries(hexahedron-kernel PRIVATE
    kernel_headers structures_headers lib_headers ethereal_kernel_abi
)
target_compile_definitions(hexahedron-kernel PRIVATE
    __LIBK __HEXAHEDRON__ __KERNEL__
    "__KERNEL_${KERNEL_BUILD_CONF}__"
)

target_compile_options(hexahedron-kernel PRIVATE
    ${ARCH_CFLAGS}
    -ffreestanding -Wall -Wextra -Werror -Wno-unused-parameter
    -Wno-restrict -Wno-format -Wno-unused-function
)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_FILE}")
set_property(TARGET hexahedron-kernel APPEND PROPERTY LINK_DEPENDS "${LINKER_SCRIPT}")

target_link_options(hexahedron-kernel PRIVATE
    ${ARCH_LDFLAGS}
    -nostdlib
    -T "${LINKER_SCRIPT}"
)

execute_process(
    COMMAND "${CMAKE_C_COMPILER}" -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

# ACPICA
if(ENABLE_ACPICA)
    target_compile_definitions(hexahedron-kernel PRIVATE ENABLE_ACPICA)
    target_link_libraries(hexahedron-kernel PRIVATE acpica)
endif()

# u80211
if(ENABLE_U80211)
    target_compile_definitions(hexahedron-kernel PRIVATE ENABLE_U80211)
    target_link_libraries(hexahedron-kernel PRIVATE u80211)
    target_include_directories(hexahedron-kernel PRIVATE
        "$<TARGET_PROPERTY:u80211_drv,INTERFACE_INCLUDE_DIRECTORIES>"
    )
endif()

target_link_libraries(hexahedron-kernel PRIVATE kstructures "${LIBGCC}")

# Refresh the build timestamp in config.c.
set(BUILD_STAMP "${CMAKE_CURRENT_BINARY_DIR}/kernel-build.stamp")
add_custom_target(kernel_build_stamp
    COMMAND "${CMAKE_COMMAND}" -E touch "${BUILD_STAMP}"
    BYPRODUCTS "${BUILD_STAMP}"
    VERBATIM
)

add_dependencies(hexahedron-kernel kernel_build_stamp)
set_property(SOURCE kernel/config.c
    APPEND PROPERTY OBJECT_DEPENDS "${BUILD_STAMP}"
)

# Final link stage
find_program(GRUB_FILE grub-file REQUIRED)

set(KERNEL_ELF "${CMAKE_CURRENT_BINARY_DIR}/hexahedron-kernel.elf")
set(KERNEL_SYMBOLS "${CMAKE_CURRENT_BINARY_DIR}/hexahedron-kernel-symbols.sym")
set(KERNEL_SYMMAP "${CMAKE_CURRENT_BINARY_DIR}/hexahedron-kernel-symmap.map")

add_custom_command(
    OUTPUT "${KERNEL_ELF}" "${KERNEL_SYMBOLS}" "${KERNEL_SYMMAP}"
    COMMAND "${CMAKE_OBJCOPY}" --only-keep-debug $<TARGET_FILE:hexahedron-kernel> "${KERNEL_SYMBOLS}"
    COMMAND "${CMAKE_OBJCOPY}" --strip-debug $<TARGET_FILE:hexahedron-kernel> "${KERNEL_ELF}"
    COMMAND "${GRUB_FILE}" --is-x86-multiboot "${KERNEL_ELF}"
    COMMAND "${GRUB_FILE}" --is-x86-multiboot2 "${KERNEL_ELF}"
    COMMAND "${CMAKE_NM}" "${KERNEL_ELF}" > "${KERNEL_SYMMAP}"
    DEPENDS hexahedron-kernel
    VERBATIM
)
add_custom_target(kernel_artifacts ALL
    DEPENDS "${KERNEL_ELF}" "${KERNEL_SYMBOLS}" "${KERNEL_SYMMAP}"
)

install(PROGRAMS "${KERNEL_ELF}" DESTINATION "${ETHEREAL_BOOT_DIR}" COMPONENT kernel)
install(FILES "${KERNEL_SYMBOLS}" DESTINATION "${ETHEREAL_BOOT_DIR}" COMPONENT kernel)
install(FILES "${KERNEL_SYMMAP}" DESTINATION "${ETHEREAL_BOOT_DIR}" COMPONENT symmap)
install(FILES "${KERNEL_SYMMAP}" DESTINATION "${ETHEREAL_INITRD_DIR}/boot" COMPONENT symmap)
