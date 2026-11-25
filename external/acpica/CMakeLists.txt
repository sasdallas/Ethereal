set(ACPICA_VERSION 20260930)
set(ACPICA_URL "https://github.com/open-acpica/acpica/releases/download/${ACPICA_VERSION}/acpica-unix-${ACPICA_VERSION}.tar.gz")

include(FetchContent)

FetchContent_Declare(acpica_src 
    URL "${ACPICA_URL}"
    URL_HASH SHA256=aa18901b92e30749be0edc3081c8d550c61fce4fa37546fc6a65d367a4ae71a5
    PATCH_COMMAND patch -p1 -i "${CMAKE_CURRENT_SOURCE_DIR}/acpica-${ACPICA_VERSION}.patch"
)

FetchContent_MakeAvailable(acpica_src)

# debugger + disassembler + resources/rsdump.c are not supported
file(GLOB SOURCES CONFIGURE_DEPENDS "${acpica_src_SOURCE_DIR}/source/components/*/*.c")
list(FILTER SOURCES EXCLUDE REGEX "/(debugger|disassembler)/|/resources/rsdump\\.c$")

# lesgo
add_library(acpica STATIC ${SOURCES})
target_compile_definitions(acpica PRIVATE __KERNEL__)
target_compile_options(acpica PRIVATE -ffreestanding)
target_include_directories(acpica PUBLIC "${acpica_src_SOURCE_DIR}/source/include/")
target_link_libraries(acpica PRIVATE ethereal_kernel_abi kernel_headers structures_headers lib_headers)
add_dependencies(acpica libc)

install(TARGETS acpica ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY "${ACPICA_INCLUDE_DIR}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
