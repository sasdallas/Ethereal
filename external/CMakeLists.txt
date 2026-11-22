if(HEADERS_ONLY)
    return()
endif()

find_program(ETHEREAL_MAKE make REQUIRED)

add_subdirectory(freetype)
add_subdirectory(essence)

if(ENABLE_ACPICA)
    add_subdirectory(acpica)
endif()

if(ENABLE_U80211)
    add_subdirectory(u80211)
    add_subdirectory(u80211_drv)
endif()
