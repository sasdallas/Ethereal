find_program(MESON_EXECUTABLE meson REQUIRED)

if(HEADERS_ONLY)
    set(MLIBC_HEADERS_ONLY true)
else()
    set(MLIBC_HEADERS_ONLY false)
endif()

ExternalProject_Add(libc
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mlibc"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/libc"

    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env "LDFLAGS=" "CFLAGS=-nostdlib"
        "${MESON_EXECUTABLE}" setup --reconfigure
        --cross-file "${CMAKE_CURRENT_SOURCE_DIR}/ethereal_cross_${BUILD_ARCH}.txt"
        --prefix "${CMAKE_SYSROOT}/usr" --libdir lib
        --default-library shared "-Dheaders_only=${MLIBC_HEADERS_ONLY}"
        <BINARY_DIR> <SOURCE_DIR>

    BUILD_COMMAND "${CMAKE_COMMAND}" -E env --unset=DESTDIR
        "${MESON_EXECUTABLE}" install --only-changed --quiet -C <BINARY_DIR>

    BUILD_ALWAYS TRUE
    INSTALL_COMMAND ""
)
