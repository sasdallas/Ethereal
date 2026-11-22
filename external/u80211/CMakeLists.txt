file(GLOB SOURCES CONFIGURE_DEPENDS "u80211/src/*.c")

add_library(u80211 STATIC ${SOURCES})
target_compile_definitions(u80211 PRIVATE __KERNEL__)
target_compile_options(u80211 PRIVATE -ffreestanding)
target_include_directories(u80211 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/u80211/include")
target_link_libraries(u80211 PRIVATE ethereal_kernel_abi)
add_dependencies(u80211 libc)

install(TARGETS u80211 ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY u80211/include/u80211 DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
