set(FT_VERSION "2.14.3")
set(FT_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}/freetype2")
set(FT_LIB_PATH "${CMAKE_INSTALL_LIBDIR}/libfreetype.so")
set(FT_ARCHIVE "freetype-${FT_VERSION}.tar.xz")
set(FT_URL "https://downloads.sourceforge.net/freetype/${FT_ARCHIVE}")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FT_ARCHIVE}")
    set(FT_URL "${CMAKE_CURRENT_SOURCE_DIR}/${FT_ARCHIVE}")
endif()

ExternalProject_Add(freetype2
    URL "${FT_URL}"
    URL_HASH SHA256=36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    BUILD_IN_SOURCE 1

    PATCH_COMMAND patch -p1 -i "${CMAKE_CURRENT_SOURCE_DIR}/freetype-${FT_VERSION}.patch"
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
        "CC=${CMAKE_C_COMPILER} --sysroot=${CMAKE_SYSROOT}"
        ./configure --host=${BUILD_ARCH}-ethereal --prefix=${CMAKE_INSTALL_PREFIX}
            --build=x86_64-linux-gnu --enable-shared
            --without-zlib --without-png --without-brotli
            --without-librsvg --without-bzip2 --without-harfbuzz
    BUILD_COMMAND "${ETHEREAL_MAKE}"
    INSTALL_COMMAND ""
)

# Headers and the imported library must be in the sysroot
ExternalProject_Add_Step(freetype2 stage
    COMMAND "${ETHEREAL_MAKE}" "DESTDIR=${CMAKE_SYSROOT}" install
    WORKING_DIRECTORY <SOURCE_DIR>
    DEPENDEES build
    DEPENDERS install
    BYPRODUCTS "${FT_LIB_PATH}" "${FT_INCLUDE_DIR}/ft2build.h"
)

file(MAKE_DIRECTORY "${FT_INCLUDE_DIR}")

add_library(freetype SHARED IMPORTED GLOBAL)
set_target_properties(freetype PROPERTIES
    IMPORTED_LOCATION "${FT_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${FT_INCLUDE_DIR}"
)

add_dependencies(freetype freetype2)
add_dependencies(freetype2 libc)
