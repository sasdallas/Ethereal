file(GLOB SOURCES CONFIGURE_DEPENDS "u80211_drv/src/*.c" "u80211_drv/src/*/*.c")

add_library(u80211_drv STATIC ${SOURCES})
target_compile_definitions(u80211_drv PRIVATE __KERNEL__)
target_compile_options(u80211_drv PRIVATE -ffreestanding)
target_include_directories(u80211_drv PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/u80211_drv/include")
target_link_libraries(u80211_drv PRIVATE ethereal_kernel_abi)
add_dependencies(u80211_drv libc)

install(TARGETS u80211_drv ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY u80211_drv/include/u80211_drv DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY u80211_drv/firmware_blobs/ DESTINATION "${CMAKE_INSTALL_LIBDIR}/firmware"
    FILES_MATCHING PATTERN "*.bin"
)
