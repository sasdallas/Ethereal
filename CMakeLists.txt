cmake_minimum_required(VERSION 3.23)
project(Ethereal LANGUAGES NONE)

option(HEADERS_ONLY "Only install headers to system root" OFF)
option(ENABLE_U80211 "Build the U80211 wifi drivers" OFF)
option(ENABLE_ACPICA "Build the ACPICA ACPI library" OFF)
set(KERNEL_BUILD_CONF "DEBUG" CACHE STRING "Kernel build configuration")

include(ExternalProject)

if(NOT HEADERS_ONLY)
    enable_language(C ASM)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(BUILD_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
string(TOUPPER "${BUILD_ARCH}" BUILD_ARCH_UPPER)

set(CMAKE_INSTALL_PREFIX "/usr")
set(CMAKE_INSTALL_LIBDIR "${CMAKE_SYSROOT}/usr/lib")
set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_SYSROOT}/usr/include")
set(CMAKE_INSTALL_BINDIR "${CMAKE_SYSROOT}/usr/bin")
set(CMAKE_INSTALL_DATADIR "${CMAKE_SYSROOT}/usr/share")

set(ETHEREAL_BOOT_DIR "${CMAKE_SYSROOT}/boot")
cmake_path(GET CMAKE_SYSROOT PARENT_PATH ETHEREAL_OUTPUT_DIR)
set(ETHEREAL_INITRD_DIR "${ETHEREAL_OUTPUT_DIR}/initrd")

add_compile_definitions(
    "__ARCH__=${BUILD_ARCH}" "__ARCH_${BUILD_ARCH_UPPER}__"
    "$<IF:$<CONFIG:Debug>,__DEBUG__,__RELEASE__>"
)

if(BUILD_ARCH STREQUAL "x86_64")
    add_compile_options(-mcmodel=large -fstrict-volatile-bitfields)
endif()

add_library(ethereal_kernel_abi INTERFACE)
if(BUILD_ARCH STREQUAL "x86_64")
    target_compile_options(ethereal_kernel_abi INTERFACE -mno-red-zone)
endif()

add_subdirectory(base)
add_subdirectory(libc)
add_subdirectory(libkstructures)
add_subdirectory(external)
add_subdirectory(hexahedron)
add_subdirectory(drivers)
add_subdirectory(userspace)

if(HEADERS_ONLY)
    return()
endif()

function(collect_build_targets DIRECTORY RESULT)
    get_property(TARGETS DIRECTORY "${DIRECTORY}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(SUBDIRECTORIES DIRECTORY "${DIRECTORY}" PROPERTY SUBDIRECTORIES)
    foreach(SUBDIRECTORY IN LISTS SUBDIRECTORIES)
        collect_build_targets("${SUBDIRECTORY}" CHILD_TARGETS)
        list(APPEND TARGETS ${CHILD_TARGETS})
    endforeach()
    set("${RESULT}" "${TARGETS}" PARENT_SCOPE)
endfunction()

collect_build_targets("${CMAKE_CURRENT_SOURCE_DIR}" SYSROOT_TARGETS)

add_custom_target(sysroot
    COMMAND "${CMAKE_COMMAND}" -E env --unset=DESTDIR
        "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --config "$<CONFIG>"
    COMMENT "Installing the sysroot"
    USES_TERMINAL
    VERBATIM
)
add_dependencies(sysroot ${SYSROOT_TARGETS})

add_custom_target(initrd
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/buildscripts/mkinitrd.py"
        "${ETHEREAL_BOOT_DIR}/initrd.tar.img" "${CMAKE_SYSROOT}"
    BYPRODUCTS "${ETHEREAL_BOOT_DIR}/initrd.tar.img"
    COMMENT "Packing the initrd (WILL TAKE A WHILE!)"
    VERBATIM
)
add_dependencies(initrd sysroot)

add_custom_target(initrd_tiny
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/buildscripts/mkinitrd.py"
        "${ETHEREAL_BOOT_DIR}/initrd_tiny.tar.img" "${ETHEREAL_INITRD_DIR}"
    BYPRODUCTS "${ETHEREAL_BOOT_DIR}/initrd_tiny.tar.img"
    COMMENT "Packing the tiny initrd"
    VERBATIM
)
add_dependencies(initrd_tiny sysroot)

add_custom_target(iso
    COMMAND bash "${CMAKE_SOURCE_DIR}/buildscripts/iso.sh"
        "${ETHEREAL_OUTPUT_DIR}/hexahedron.iso" "${ETHEREAL_BOOT_DIR}" "${CMAKE_SOURCE_DIR}/conf"
    BYPRODUCTS "${ETHEREAL_OUTPUT_DIR}/hexahedron.iso"
    COMMENT "Building the ISO"
    USES_TERMINAL
    VERBATIM
)
add_dependencies(iso initrd)
