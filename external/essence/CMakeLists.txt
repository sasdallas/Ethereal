ExternalProject_Add(essence
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/essence"
    CONFIGURE_COMMAND ""
    BUILD_ALWAYS TRUE
    BUILD_COMMAND "${ETHEREAL_MAKE}" -C <SOURCE_DIR>
        "BUILD_DIR=<BINARY_DIR>"
        "CC=${CMAKE_C_COMPILER} --sysroot=${CMAKE_SYSROOT}"

    INSTALL_COMMAND ""
)
add_dependencies(essence libc)

ExternalProject_Get_Property(essence BINARY_DIR)
install(PROGRAMS "${BINARY_DIR}/essence" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(CODE "
    file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_SYSROOT}/bin\")
    file(CREATE_LINK /usr/bin/essence \"\$ENV{DESTDIR}${CMAKE_SYSROOT}/bin/sh\" SYMBOLIC)
")
